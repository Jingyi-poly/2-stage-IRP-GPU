
#ifndef SPLIT_CUDA_H
#define SPLIT_CUDA_H

#include "Params.h"
#include "Individual.h"
#include <cuda_runtime.h>
#include <omp.h>
#include <cstring>


struct ClientSplitCUDA
{
	double demand;
	// std::vector<double> demands_scenarios;
	double serviceTime;
	double d0_x;
	double dx_0;
	double dnext;
	ClientSplitCUDA() : demand(0.), serviceTime(0.), d0_x(0.), dx_0(0.), dnext(0.) {};
};
// This is transformed into a matrix:
//     [demand, serviceTime, d0_x, dx_0, dnext]

// Simple Deque which is used for all Linear Split algorithms
struct Trivial_DequeCUDA
{
	std::vector <int> myDeque; // Simply a vector structure to keep the elements of the queue
	int indexFront; // Index of the front element
	int indexBack; // Index of the back element
	inline void pop_front(){indexFront++;} // Removes the front element of the queue D
	inline void pop_back(){indexBack--;} // Removes the back element of the queue D
	inline void push_back(int i){indexBack++; myDeque[indexBack] = i;} // Appends a new element to the back of the queue D
	inline int get_front(){return myDeque[indexFront];}
	inline int get_next_front(){return myDeque[indexFront + 1];}
	inline int get_back(){return myDeque[indexBack];}
	void reset(int firstNode) { myDeque[0] = firstNode; indexBack = 0; indexFront = 0; }
	inline int size(){return indexBack - indexFront + 1;}
	
	Trivial_DequeCUDA(int nbElements, int firstNode)
	{
		myDeque = std::vector <int>(nbElements);
		myDeque[0] = firstNode;
		indexBack = 0;
		indexFront = 0;
	}
};

class SplitCUDA
{

 private:
  const int size_clisplit = 5;
  const int idx_demand = 0;
  const int idx_serviceTime = 1;
  const int idx_d0_x = 2;
  const int idx_dx_0 = 3;
  const int idx_dnext = 4;

  const Params & params ;
  int * maxVehicles ;
  std::vector<int> maxVehicles_host;

  int n,m,n_scen;
  int maxBatchSize;

  /* Auxiliary data structures to run the Linear Split algorithm */
  // std::vector < ClientSplitCUDA > cliSplit;
  double * cliSplit_demand;
  double * cliSplit_serviceTime;
  double * cliSplit_d0_x;
  double * cliSplit_dx_0;
  double * cliSplit_dnext;
  //  std::vector < std::vector < double > > potential;  // Potential vector
  double * potential;
  // std::vector < std::vector < int > > pred;  // Indice of the predecessor in an optimal path
  int * pred;
  int * pred_host;
  // std::vector <double> sumDistance; // sumDistance[i] for i > 1 contains the sum of distances : sum_{k=1}^{i-1} d_{k,k+1}
  double * sumDistance;
  // std::vector <double> sumLoad; // sumLoad[i] for i >= 1 contains the sum of loads : sum_{k=1}^{i} q_k
  double * sumLoad;
  // std::vector <double> sumService; // sumService[i] for i >= 1 contains the sum of service time : sum_{k=1}^{i} s_k
  double * sumService;

  double * totalDemands;
  int * myDeque;
  double * demand_host;
  double * host_d0_x;
  double * host_dx_0;
  double * host_sumDistance;
  cudaStream_t stream;

  // GPU eval members
  double * d_timeCost;      // [m_full * m_full] distance matrix on device
  int * d_chromT;           // [maxBatchSize * nbClients] chromT on device
  int * h_chromT;           // pinned host chromT staging
  double * d_evalResults;   // [maxBatchSize * n_scen * 4] per-scen results (dist,capEx,durEx,nRoutes)
  double * h_evalResults;   // pinned host for results
  int m_full;               // original m = nbClients+1 (never changes with setActiveClients)



  //  // To be called with i < j only
  //  // Computes the cost of propagating the label i until j
  inline double propagate(int i, int j, int k);
  //  {
  // 	 return potential[k][i] + sumDistance[j] - sumDistance[i + 1] + cliSplit[i + 1].d0_x + cliSplit[j].dx_0
  // 		 + params.penaltyCapacity * std::max<double>(sumLoad[j] - sumLoad[i] - params.vehicleCapacity, 0.);
  //  }

  //  // Tests if i dominates j as a predecessor for all nodes x >= j+1
  //  // We assume that i < j
   inline bool dominates(int i, int j, int k);
  //  {
  // 	 return potential[k][j] + cliSplit[j + 1].d0_x > potential[k][i] + cliSplit[i + 1].d0_x + sumDistance[j + 1] - sumDistance[i + 1]
  // 		 + params.penaltyCapacity * (sumLoad[j] - sumLoad[i]);
  //  }

  //  // Tests if j dominates i as a predecessor for all nodes x >= j+1
  //  // We assume that i < j
   inline bool dominatesRight(int i, int j, int k);
  //  {
  // 	 return potential[k][j] + cliSplit[j + 1].d0_x < potential[k][i] + cliSplit[i + 1].d0_x + sumDistance[j + 1] - sumDistance[i + 1] + MY_EPSILON;
  //  }

  //   Split for unlimited fleet
    // int splitSimple(Individual & indiv, int idx_scen);

  //   Split for limited fleet
    // int splitLF(Individual & indiv, int idx_scen);


public:

  // General Split function (tests the unlimited fleet, and only if it does not produce a feasible solution, runs the Split algorithm for limited fleet)
  void preprocess(Individual & indiv, int nbMaxVehicles);

  void copy_vec(double * source, double * tar, int n_ele){
    if (!tar){
      cudaMalloc(&tar, n_ele * sizeof(double));
    }
    cudaMemcpy(tar, source, n_ele * sizeof(double), cudaMemcpyHostToDevice);
  }
  void copy_vecVec(std::vector<double> source, double * tar){
    if (!tar){
      cudaMalloc(&tar, source.size() * sizeof(double));
    }
    cudaMemcpy(tar, source.data(), source.size() * sizeof(double), cudaMemcpyHostToDevice);
  }

  static inline void cudaCheck(cudaError_t e, const char* what) {
    if (e != cudaSuccess) throw std::runtime_error(std::string(what) + ": " + cudaGetErrorString(e));
  }

  void printMat(const int n, const int m, double * mat){
	std::cout<<"\n\n\n\n\n";
    double h_mat[n*m];
    // double * h_mat = new double[2000]; // allocate on host
    // double * h_mat = (double*)malloc(n*mm*sizeof(double)); // allocate on host
    // Copy from device to host
    cudaMemcpy(&h_mat, mat, n * m * sizeof(double), cudaMemcpyDeviceToHost);
    for (int i = 0; i < n; ++i){
      for (int j = 0; j < m; ++j){
        std::cout<<h_mat[i*m + j]<<" , ";
      }
      std::cout<<"|\n";
    }
  }


  void printPotential(){
    std::cout<<"Potential: n_scen->"<<n_scen<<"\n";

    double h_potential[n];
    // double * h_mat = new double[2000]; // allocate on host
    // double * h_mat = (double*)malloc(n*mm*sizeof(double)); // allocate on host
    // Copy from device to host

    for (int i = 0; i < n_scen; ++i){
      cudaMemcpy(&h_potential, potential,  n * sizeof(double), cudaMemcpyDeviceToHost);
        for (int k = 0; k < n; ++k){
          std::cout<<h_potential[ k]<<", ";
        }
        std::cout<<"\n";
        std::cout<<"--------------\n";
        break;
    }
    std::cout<<"Potential: \n";
  }


  void printPred(){
    std::cout<<n_scen * m * n<<"\n\n\n\n";

    int h_pred[ m * n];
    // double * h_mat = new double[2000]; // allocate on host
    // double * h_mat = (double*)malloc(n*mm*sizeof(double)); // allocate on host
    // Copy from device to host

    for (int iscen = 0; iscen < n_scen;++iscen){
      int p = 0;
      int last = -1;
      int n_segs = 0;
      cudaMemcpy(&h_pred, pred+iscen * (n*m), 1 * m * n * sizeof(int), cudaMemcpyDeviceToHost);
    // int iscen = 0;
      for (int j = 0; j < m; ++j){
        std::cout<<h_pred[ 0 * m + j]<<" . ";
        if (h_pred[ 0 * m + j] != last){
          last = h_pred[ 0 * m + j];
          n_segs +=1;
        }
        p+=1;
      }
      std::cout<<"    size: "<<p<<"    n vehi: "<<n_segs<<"|\n";

    }
  }

  void generate_split();
  void reconstruct_from_pred(Individual & indiv);

  void reset_batch(int batchSize);
  void preprocess_batch(std::vector<Individual*>& indivs, int nbMaxVehicles);
  void generate_split_batch(int batchSize);
  void reconstruct_from_pred_batch(std::vector<Individual*>& indivs);
  int getMaxBatchSize() const { return maxBatchSize; }

  void evaluateOnGPU_batch(std::vector<Individual*>& indivs);
  void deriveSuccessorsPredecessors_batch(std::vector<Individual*>& indivs);

  int getOriginalM() const { return params.nbClients + 1; }
  void setActiveClients(int nCli) { m = nCli + 1; }

  // Constructor
//   SplitCUDA(const Params & params): params(params)
//     {
//         // Structures of the linear Split
//         // cliSplit = std::vector <ClientSplitCUDA>(params.nbClients + 1);
//         // sumDistance = std::vector <double>(params.nbClients + 1,0.);
//         // sumLoad = std::vector <double>(params.nbClients + 1,0.);
//         // sumService = std::vector <double>(params.nbClients + 1, 0.);
//         // potential = std::vector < std::vector <double> >(params.nbVehicles + 1, std::vector <double>(params.nbClients + 1,1.e30));
//         // pred = std::vector < std::vector <int> >(params.nbVehicles + 1, std::vector <int>(params.nbClients + 1,0));
//     }
    SplitCUDA(const Params & params, int batchSz = 1): params(params)
    {
      n = params.nbVehicles + 1;
      m = params.nbClients + 1;
      n_scen = params.n_scenarios;
      maxBatchSize = batchSz;
      size_t B = (size_t)maxBatchSize;
      size_t nst = B * n_scen;
      cudaMalloc(&potential, nst * m * sizeof(double));
      cudaMalloc(&pred, nst * m * sizeof(int));
      cudaMallocHost(&pred_host, nst * m * sizeof(int));
      cudaMallocHost(&demand_host, (size_t)m * nst * sizeof(double));
      std::memset(demand_host, 0, (size_t)m * nst * sizeof(double));
      cudaMallocHost(&host_d0_x, B * m * sizeof(double));
      cudaMallocHost(&host_dx_0, B * m * sizeof(double));
      cudaMallocHost(&host_sumDistance, B * m * sizeof(double));
      cudaMalloc(&sumLoad, nst * m * sizeof(double));
      cudaMalloc(&sumDistance, B * m * sizeof(double));
      cudaMalloc(&sumService, m * sizeof(double));
      cudaMalloc(&cliSplit_demand, nst * m * sizeof(double));
      cudaMalloc(&cliSplit_serviceTime, m * sizeof(double));
      cudaMalloc(&cliSplit_d0_x, B * m * sizeof(double));
      cudaMalloc(&cliSplit_dx_0, B * m * sizeof(double));
      cudaMalloc(&cliSplit_dnext, m * sizeof(double));
      cudaMalloc(&totalDemands, n_scen * sizeof(double));
      copy_vec(params.totalDemands, totalDemands, n_scen);
      maxVehicles_host = std::vector<int>(nst);
      cudaStreamCreate(&stream);
      cudaMalloc(&myDeque, nst * m * sizeof(int));

      // GPU eval buffers
      m_full = m;
      cudaMalloc(&d_timeCost, (size_t)m * m * sizeof(double));
      // Flatten and upload timeCost
      std::vector<double> flat_tc((size_t)m * m);
      for (int i = 0; i < m; i++)
        for (int j = 0; j < m; j++)
          flat_tc[i * m + j] = params.timeCost[i][j];
      cudaMemcpy(d_timeCost, flat_tc.data(), m * m * sizeof(double), cudaMemcpyHostToDevice);

      int nbCli = m - 1;
      cudaMalloc(&d_chromT, B * nbCli * sizeof(int));
      cudaMallocHost(&h_chromT, B * nbCli * sizeof(int));
      cudaMalloc(&d_evalResults, nst * 4 * sizeof(double));
      cudaMallocHost(&h_evalResults, nst * 4 * sizeof(double));
    }

    ~SplitCUDA() {
      cudaStreamDestroy(stream);
      cudaFreeHost(pred_host);
      cudaFreeHost(demand_host);
      cudaFreeHost(host_d0_x);
      cudaFreeHost(host_dx_0);
      cudaFreeHost(host_sumDistance);
      cudaFree(d_timeCost);
      cudaFree(d_chromT);
      cudaFreeHost(h_chromT);
      cudaFree(d_evalResults);
      cudaFreeHost(h_evalResults);
    }

    void reset();

};
#endif
