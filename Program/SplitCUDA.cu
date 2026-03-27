#include "SplitCUDA.h"

#include <chrono>
#include <cstring>


// inline double propagate(int i, int j, int k)
// {
//     return potential[k][i] + sumDistance[j] - sumDistance[i + 1] + cliSplit[i + 1].d0_x + cliSplit[j].dx_0
//         + params.penaltyCapacity * std::max<double>(sumLoad[j] - sumLoad[i] - params.vehicleCapacity, 0.);
// }


// __global__ void propagate_kernel(const float* potential, const float* sumDistance, int i, int j, int k) {
//     int idx = blockIdx.x * blockDim.x + threadIdx.x;
//     if (idx < N) {
//         C[idx] = A[idx] + B[idx];
//     }
// }



// inline double SplitCUDA::propagate(int i, int j, int k){
//     return potential[k*n+i] + sumDistance[j] - sumDistance[i + 1] + cliSplit[(i + 1)*size_clisplit + idx_d0_x] + cliSplit[j*size_clisplit + idx_dx_0]
//         + params.penaltyCapacity * std::max<double>(sumLoad[j] - sumLoad[i] - params.vehicleCapacity, 0.);
// }

// inline bool SplitCUDA::dominates(int i, int j, int k)
// {
//     return potential[k*n+j] + cliSplit[(j + 1)*size_clisplit + idx_d0_x] > potential[k*n+i] + cliSplit[(i + 1)*size_clisplit + idx_d0_x] + sumDistance[j + 1] 
//             - sumDistance[i + 1] + params.penaltyCapacity * (sumLoad[j] - sumLoad[i]);
// }

// inline bool SplitCUDA::dominatesRight(int i, int j, int k)
// {
//     return potential[k*n+j] + cliSplit[(j + 1)*size_clisplit + idx_d0_x] < potential[k*n+i] + cliSplit[(i + 1)*size_clisplit + idx_d0_x] + sumDistance[j + 1] - sumDistance[i + 1] + MY_EPSILON;
// }

// __global__ void move_cilSplit(const double* source, const double* target, int n, int m, int ent_select) {
// 	//     int icol = blockIdx.x * blockDim.x + threadIdx.x; // x → columns
// 	//     int irow = blockIdx.y * blockDim.y + threadIdx.y; // y → rows
//     if (irow < n && icol < m) {
//         target[idx] = A[idx] + B[idx];
//     }
// }


// __global__ void updateDemand(const float* potential, const float* sumDistance, int i, int j, int k) {
//     int icol = blockIdx.x * blockDim.x + threadIdx.x; // x → columns
//     int irow = blockIdx.y * blockDim.y + threadIdx.y; // y → rows
//     if (idx < N) {
//         C[idx] = A[idx] + B[idx];
//     }
// }

__global__ void init_with_value( int * tensor, int n, int m, double value) {
	int i = blockIdx.x * blockDim.x + threadIdx.x;
	if (i < n*m){
		tensor[i] = value;
	}
}

__global__ void init_with_value_nnz( double * tensor, int nnz, double value) {
	for (int i = blockIdx.x * blockDim.x + threadIdx.x; i < nnz; i += blockDim.x * gridDim.x){
		tensor[i] = value;
	}
}

__global__ void init_with_value_nnz_int( int * tensor, int nnz, int value) {
	for (int i = blockIdx.x * blockDim.x + threadIdx.x; i < nnz; i += blockDim.x * gridDim.x){
		tensor[i] = value;
	}
}

// myDeque [n_scen * m]
__global__ void split_simple_nocon( int * myDeque, double * potential, 
									int * pred, double * sumDistance, 
									double * cliSplit_d0_x, 
									double * cliSplit_dx_0,
									double * sumLoad,
									int n_scen, 
									int n_vehi, int ncli,
									double penaltyCapacity,
									double vehicleCapacity
									) {
	// int i_scenario = blockIdx.x * blockDim.x * blockDim.y + threadIdx.x;
	int i_scenario = blockIdx.x * blockDim.x + threadIdx.x;
	// printf("%d/%d\n",i_scenario,n_scen);
	// int i_scenario_y = blockIdx.y * blockDim.y + threadIdx.y;
	int nbClients = ncli - 1;
	if (i_scenario < n_scen){
		bool debug = false;
		// if (i_scenario==0) debug = true;
		// int nm = n_vehi*ncli;
		int nm = ncli;
		// potential[i_scenario * (nm) + 0 * n_vehi + 0] = 0.;
		potential[i_scenario * (nm) + 0] = 0.;
		int indexFront = 0;
		int indexBack = 0;
		for (int ic = 1; ic <= nbClients; ic++){
			// potential[0][ic] = propagate(i=queue.get_front()=i, j=ic, k=0);
			//  return potential[0][i] + sumDistance[ic] - sumDistance[i + 1] + cliSplit[i + 1].d0_x + cliSplit[j].dx_0
			// 	 + params.penaltyCapacity * std::max<double>(sumLoad[j] - sumLoad[i] - params.vehicleCapacity, 0.);
			int i = myDeque[indexFront * n_scen + i_scenario];
			double potential_term = sumLoad[ic * n_scen + i_scenario] - sumLoad[i * n_scen + i_scenario] - vehicleCapacity;
			if (potential_term < 0){
				potential_term = 0.0;
			}
			potential_term = potential_term * penaltyCapacity;
			// potential[0][ic] = propagate(queue.get_front(), ic, 0);
			// potential[i_scenario * (nm) + 0 * n_vehi + ic] = potential[i_scenario * (nm) + 0 * n_vehi + i] + sumDistance[ic] - sumDistance[i + 1] + cliSplit_d0_x[i + 1] + cliSplit_dx_0[ic] + potential_term;
			potential[i_scenario * (nm) + ic] = potential[i_scenario * (nm) + i] + sumDistance[ic] - sumDistance[i + 1] + cliSplit_d0_x[i + 1] + cliSplit_dx_0[ic] + potential_term;
			if (debug){
				printf("Scenario %d: data[%d]--> %f = %f + %f - %f + %f + %f %d\n", i_scenario, i_scenario,potential[i_scenario * (nm) + 0 * n_vehi + ic],potential[i_scenario * (nm) + 0 * n_vehi + i], sumLoad[ic * n_scen + i_scenario], 
								sumDistance[i + 1], cliSplit_d0_x[i + 1], cliSplit_dx_0[ic], ic * n_scen + i_scenario);
				printf("   Scenario %d:   front: %d   back: %d\n", i_scenario, indexFront, indexBack);
			}
			// pred[0][i] = queue.get_front();
			// pred[i_scenario * (nm) + 0 * n_vehi + ic] = i;
			pred[i_scenario * (nm) + ic] = i;


			if (ic < nbClients){
				// (!dominates(queue.get_back()=i, j=ic, k=0))
					//   potential[k][j] + cliSplit[j + 1].d0_x
					// > potential[k][i] + cliSplit[i + 1].d0_x + sumDistance[j + 1] - sumDistance[i + 1]
					// 	 + params.penaltyCapacity * (sumLoad[j] - sumLoad[i]);
				i = myDeque[indexBack * n_scen + i_scenario];
				// double v1 = potential[i_scenario * (nm) + 0 * n_vehi + ic] + cliSplit_d0_x[ic + 1];
				double v1 = potential[i_scenario * (nm) + ic] + cliSplit_d0_x[ic + 1];
				// double v2 = potential[i_scenario * (nm) + 0 * n_vehi + i] + cliSplit_d0_x[i + 1] + sumDistance[ic + 1] - sumDistance[i + 1]	 + penaltyCapacity * (sumLoad[ic] - sumLoad[i]);
				// potential[k][i] + cliSplit[i + 1].d0_x + sumDistance[j + 1] - sumDistance[i + 1]+ params.penaltyCapacity * (sumLoad[j] - sumLoad[i])
				double v2 = potential[i_scenario * (nm) + i] + cliSplit_d0_x[i + 1] + sumDistance[ic + 1] - sumDistance[i + 1]	 + penaltyCapacity * (sumLoad[ic * n_scen + i_scenario] - sumLoad[i * n_scen + i_scenario]);
				if (debug)printf("%f %f %f %f %f\n",potential[i_scenario * (nm) + i],cliSplit_d0_x[i + 1],sumDistance[ic + 1],sumDistance[i + 1],sumLoad[ic * n_scen + i_scenario]- sumLoad[i * n_scen + i_scenario]);
				bool dominates = v1 > v2;
				if (debug) printf("   Scenario %d:   dominates? %d    v1:%f  v2:%f \n", i_scenario,dominates,v1,v2);

				if (!dominates)
				{
					// then i will be inserted, need to remove whoever is dominated by i.
					// dominatesRight(queue.get_back(), ic, 0)
					i = myDeque[indexBack * n_scen + i_scenario];
					// double v1r = potential[i_scenario * (nm) + 0 * n_vehi + ic] + cliSplit_d0_x[ic + 1];
					double v1r = potential[i_scenario * (nm) + ic] + cliSplit_d0_x[ic + 1];
					// double v2r = potential[i_scenario * (nm) + 0 * n_vehi + i] + cliSplit_d0_x[i + 1] + sumDistance[ic + 1] - sumDistance[i + 1] + MY_EPSILON;
					double v2r = potential[i_scenario * (nm) + i] + cliSplit_d0_x[i + 1] + sumDistance[ic + 1] - sumDistance[i + 1] + MY_EPSILON;
					bool dominatesRight = v1r < v2r;
					while (indexBack - indexFront + 1 > 0 && dominatesRight){
						if (debug) printf("   Scenario %d:   dominatesRight? %d...... size %d\n", i_scenario, dominatesRight, indexBack - indexFront + 1);
						indexBack--;
						i = myDeque[indexBack * n_scen + i_scenario];
						// v1r = potential[i_scenario * (nm) + 0 * n_vehi + ic] + cliSplit_d0_x[ic + 1];
						v1r = potential[i_scenario * (nm) + ic] + cliSplit_d0_x[ic + 1];
						// v2r = potential[i_scenario * (nm) + 0 * n_vehi + i] + cliSplit_d0_x[i + 1] + sumDistance[ic + 1] - sumDistance[i + 1] + MY_EPSILON;
						v2r = potential[i_scenario * (nm) + i] + cliSplit_d0_x[i + 1] + sumDistance[ic + 1] - sumDistance[i + 1] + MY_EPSILON;
						dominatesRight = v1r < v2r;
					}
					if (debug) printf("    Scenario %d:  dominatesRight? %d...... size %d\n", i_scenario, dominatesRight, indexBack - indexFront + 1);
					indexBack++; 
					myDeque[indexBack * n_scen + i_scenario] = ic;
				}
				if (debug) printf("  Scenario %d: out   front: %d   back: %d\n", i_scenario,indexFront, indexBack);
				// Check iteratively if front is dominated by the next front
				i = myDeque[indexFront * n_scen + i_scenario];
				// propagate(i, ic + 1, 0);
				// double v1p = potential[i_scenario * (nm) + 0 * n_vehi + i] + sumDistance[ic+1] - sumDistance[i + 1] + cliSplit_d0_x[i + 1] + cliSplit_dx_0[ic+1];
				double v1p = potential[i_scenario * (nm) + i] + sumDistance[ic+1] - sumDistance[i + 1] + cliSplit_d0_x[i + 1] + cliSplit_dx_0[ic+1];
				potential_term = sumLoad[(ic+1) * n_scen + i_scenario] - sumLoad[i * n_scen + i_scenario] - vehicleCapacity;
				if (potential_term < 0){
					potential_term = 0.0;
				}
				potential_term = potential_term * penaltyCapacity;
				v1p += potential_term;
				i = myDeque[(indexFront + 1) * n_scen + i_scenario];
				// propagate(i, ic + 1, 0) ;
				// double v2p = potential[i_scenario * (nm) + 0 * n_vehi + i] + sumDistance[ic+1] - sumDistance[i + 1] + cliSplit_d0_x[i + 1] + cliSplit_dx_0[ic+1];
				double v2p = potential[i_scenario * (nm) + i] + sumDistance[ic+1] - sumDistance[i + 1] + cliSplit_d0_x[i + 1] + cliSplit_dx_0[ic+1];
				if (debug) printf("    ^^^^^    %f %f %f %f %f\n",potential[i_scenario * (nm) + i], sumDistance[ic+1], sumDistance[i + 1], cliSplit_d0_x[i + 1], cliSplit_dx_0[ic+1]);
				potential_term = sumLoad[(ic+1) * n_scen + i_scenario] - sumLoad[i * n_scen + i_scenario] - vehicleCapacity;
				if (potential_term < 0){
					potential_term = 0.0;
				}
				potential_term = potential_term * penaltyCapacity;
				v2p += potential_term;
				if (debug) printf("    Scenario %d:  prop 2 = %f\n",i_scenario, v1p);
				if (debug) printf("    Scenario %d:  prop 1 = %f, next front: %d\n\n", i_scenario,v2p, i);
				while (indexBack - indexFront + 1  > 1 && v1p > v2p - MY_EPSILON){
					indexFront++;

					i = myDeque[indexFront * n_scen + i_scenario];
					// v1p = potential[i_scenario * (nm) + 0 * n_vehi + i] + sumDistance[ic+1] - sumDistance[i + 1] + cliSplit_d0_x[i + 1] + cliSplit_dx_0[ic+1];
					v1p = potential[i_scenario * (nm) + i] + sumDistance[ic+1] - sumDistance[i + 1] + cliSplit_d0_x[i + 1] + cliSplit_dx_0[ic+1];
					potential_term = sumLoad[(ic+1) * n_scen + i_scenario] - sumLoad[i * n_scen + i_scenario] - vehicleCapacity;
					if (potential_term < 0){
						potential_term = 0.0;
					}
					potential_term = potential_term * penaltyCapacity;
					v1p += potential_term;
					i = myDeque[(indexFront + 1) * n_scen + i_scenario];
					// propagate(i, ic + 1, 0) ;
					// v2p = potential[i_scenario * (nm) + 0 * n_vehi + i] + sumDistance[ic+1] - sumDistance[i + 1] + cliSplit_d0_x[i + 1] + cliSplit_dx_0[ic+1];
					v2p = potential[i_scenario * (nm) + i] + sumDistance[ic+1] - sumDistance[i + 1] + cliSplit_d0_x[i + 1] + cliSplit_dx_0[ic+1];
					potential_term = sumLoad[(ic+1) * n_scen + i_scenario] - sumLoad[i * n_scen + i_scenario] - vehicleCapacity;
					if (potential_term < 0){
						potential_term = 0.0;
					}
					potential_term = potential_term * penaltyCapacity;
					v2p += potential_term;
				}
			}
		}
	}

}










__global__ void accumulation_row(const double* input_data, double* out_data, int n, int m) {
    int icol = blockIdx.x * blockDim.x + threadIdx.x; // x → columns
	// printf(" current pos: blockIdx: %d, blockDim: %d, threadIDX: %d ",blockIdx.x,blockDim.x,threadIdx.x);
	double sum = 0.0;
    if (icol < m) {
        for (int i = 1; i < n; ++i){
			sum += input_data[i * m + icol];
			out_data[i * m + icol] = sum;
			if (input_data[i*m+icol]>0.1){
				// printf("data[%d] = %f (%f)\n", icol,sum, input_data[i * m + icol]);
			}
		}
	}
}

void SplitCUDA::generate_split(){


	// printPotential();

	// printMat(m, n_scen, sumLoad);
	// std::cout<<"Pringint sumload\n";
	// int g; std::cin>>g;

	dim3 threads3(1024);
	dim3 blocks3(1024);
	split_simple_nocon<<<blocks3, threads3>>>(myDeque,potential,pred,sumDistance,cliSplit_d0_x,cliSplit_dx_0,sumLoad,n_scen,n,m,params.penaltyCapacity,params.vehicleCapacity);
			// int g;std::cin>>g;
	cudaCheck(cudaDeviceSynchronize(), "Kernel sync 1");
	// printPotential();
	// printPred();
	// int g;
	// std::cin>>g;
}


// __global__ void reconstruct_cuda(const double* input_data, double* out_data, int n_scen, int m) {
//     int i_scenario = blockIdx.x * blockDim.x + threadIdx.x; // x → columns
// 	// printf(" current pos: blockIdx: %d, blockDim: %d, threadIDX: %d ",blockIdx.x,blockDim.x,threadIdx.x);
// 	double sum = 0.0;
//     if (ii_scenariocol < n_scen) {
//         for (int i = 0; i < n; ++i){
// 			sum += input_data[i * m + icol];
// 			out_data[i * m + icol] = sum;
// 			if (input_data[i*m+icol]>0.1){
// 				// printf("data[%d] = %f (%f)\n", icol,sum, input_data[i * m + icol]);
// 			}
// 		}
// 	}
// }



void SplitCUDA::reconstruct_from_pred(Individual & indiv){
	cudaCheck(cudaDeviceSynchronize(), "Kernel sync 2");
	cudaMemcpy(pred_host, pred, (size_t)n_scen * m * sizeof(int), cudaMemcpyDeviceToHost);

	#pragma omp parallel for schedule(static)
	for (int idx_scen = 0; idx_scen < n_scen; ++idx_scen){
		for (int k = params.nbVehicles - 1; k >= maxVehicles_host[idx_scen]; k--)
			indiv.chromR_scen[idx_scen][k].clear();

		int end = params.nbClients;
		for (int k = maxVehicles_host[idx_scen] - 1; k >= 0; k--)
		{
			indiv.chromR_scen[idx_scen][k].clear();
			int begin = pred_host[idx_scen*m + end];
			for (int ii = begin; ii < end; ii++)
				indiv.chromR_scen[idx_scen][k].push_back(indiv.chromT[ii]);
			end = begin;
		}
	}
}


__global__ void print_vec(const double* input_data, int n) {
    int icol = blockIdx.x * blockDim.x + threadIdx.x; // x → columns
	// printf(" current pos: blockIdx: %d, blockDim: %d, threadIDX: %d ",blockIdx.x,blockDim.x,threadIdx.x);
    if (icol == 1) {
		for (int i = 0; i < n; ++i){
			printf("%f, ",input_data[i]);
		}
		printf("\n");
	}
}

void SplitCUDA::reset(){
	int size = n_scen * m;
	int nthreads = 1024;
	dim3 threads(nthreads);
	dim3 blocks((size + nthreads - 1) / nthreads);
	init_with_value_nnz_int<<<blocks, threads>>>(myDeque,size,0);
	init_with_value_nnz_int<<<blocks, threads>>>(pred,size,0);

	int pot_size = n_scen * m;
	dim3 pot_blocks((pot_size + nthreads - 1) / nthreads);
	init_with_value_nnz<<<pot_blocks, dim3(nthreads)>>>(potential,pot_size,1.e30);
	cudaCheck(cudaDeviceSynchronize(), "Kernel sync 4");


	// cudaMalloc(&pred, n_scen * m * n * sizeof(int));
	// cudaMalloc(&sumLoad, n_scen * m * sizeof(double));
	// cudaMalloc(&sumDistance, m * sizeof(double));
	// cudaMalloc(&sumService, m * sizeof(double));
	// // clisplit
	// cudaMalloc(&cliSplit_demand, n_scen * m * sizeof(double));
	// cudaMalloc(&cliSplit_serviceTime, m * sizeof(double));
	// cudaMalloc(&cliSplit_d0_x, m * sizeof(double));
	// cudaMalloc(&cliSplit_dx_0, m * sizeof(double));
	// cudaMalloc(&cliSplit_dnext, m * sizeof(double));
	// // move total demands to device
	// cudaMalloc(&totalDemands, n_scen * sizeof(double));
	// copy_vec(params.totalDemands, totalDemands, n_scen);
	// // move max_vehicles to device
	// cudaMalloc(&maxVehicles, n_scen * sizeof(int));
	// maxVehicles_host = std::vector <int>(n_scen);
	// cudaCheck(cudaDeviceSynchronize(), "Kernel sync");
}



void check(double * tensor, const double * original, int n, int m, int i){
	double narr[m];
	cudaMemcpy(&narr, tensor,  m * sizeof(double), cudaMemcpyDeviceToHost);
	for (int j = 0; j < m; ++j){
		if (narr[j]!=original[j]){
			std::cout<<narr[j]<<" ... "<<original[j]<<"\n";
			int g; std::cin>>g;

		}
	}

}


void SplitCUDA::preprocess(Individual & indiv, int nbMaxVehicles)
{
	int maxV = std::max<int>(nbMaxVehicles, (int)std::ceil(params.totalDemand / params.vehicleCapacity));
	for (int i = 0; i < n_scen; i++)
		maxVehicles_host[i] = maxV;

	std::vector<double> copy_d0_x(m, 0.0);
	std::vector<double> copy_dx_0(m, 0.0);
	std::vector<double> copy_dnext(m, 0.0);
	std::vector<double> copy_sumDistance(m, 0.0);

	for (int i = 1; i <= params.nbClients; i++)
	{
		const double * src = params.cli[indiv.chromT[i - 1]].demands_scenarios.data();
		std::memcpy(demand_host.data() + (size_t)i * n_scen, src, n_scen * sizeof(double));

		copy_d0_x[i] = params.timeCost[0][indiv.chromT[i - 1]];
		copy_dx_0[i] = params.timeCost[indiv.chromT[i - 1]][0];

		if (i < params.nbClients) copy_dnext[i] = params.timeCost[indiv.chromT[i - 1]][indiv.chromT[i]];
		else copy_dnext[i] = -1.e30;

		copy_sumDistance[i] = copy_sumDistance[i - 1] + copy_dnext[i - 1];
	}

	cudaMemcpy(cliSplit_demand, demand_host.data(), (size_t)m * n_scen * sizeof(double), cudaMemcpyHostToDevice);
	cudaMemcpy(cliSplit_d0_x, copy_d0_x.data(), m * sizeof(double), cudaMemcpyHostToDevice);
	cudaMemcpy(cliSplit_dx_0, copy_dx_0.data(), m * sizeof(double), cudaMemcpyHostToDevice);
	cudaMemcpy(sumDistance, copy_sumDistance.data(), m * sizeof(double), cudaMemcpyHostToDevice);

	int n_threads = 1024;
	dim3 threads(n_threads);
	dim3 blocks((n_scen + n_threads - 1) / n_threads);
	accumulation_row<<<blocks, threads>>>(cliSplit_demand, sumLoad, m, n_scen);
	cudaCheck(cudaDeviceSynchronize(), "Kernel sync 5");
}