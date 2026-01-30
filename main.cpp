/*
 * OUIRP-GPU: Hybrid Genetic Search for the Inventory Routing Problem
 *
 * Based on HGSADC algorithm by Thibaut Vidal
 * Original: thibaut.vidal@cirrelt.ca
 *
 * This implementation extends the original HGSADC framework with:
 * - Two-stage stochastic optimization for uncertain demand
 * - GPU acceleration via gRPC for inventory optimization
 * - Multi-scenario parallel processing
 */

#include <iostream>
#include <chrono>
#include <thread>
#include <string>

#include "irp/core/Genetic.h"
#include "irp/model/ParamsList.h"
#include "irp/utils/commandline.h"
#include "irp/utils/ThreadPool.h"

using namespace std;

/**
 * Main IRP solver function
 *
 * @param c Command line parameters containing:
 *          - Instance file path
 *          - Number of scenarios
 *          - Thread configuration
 *          - Time limit and other solver parameters
 * @return 0 on success, non-zero on error
 */
int mainIRP(commandline& c)
{
    bool traces = true;

    if (c.is_valid())
    {
        // Time limit for program execution (wall clock time)
        int time_limit = c.get_cpu_time();
        std::chrono::seconds nb_ticks_allowed(time_limit);

        // Initialize parameters from instance file
        ParamsList paramsList(c);
        if (traces) cout << "Parameters initialized" << endl;

        // Create initial population
        Population *population = new Population(&paramsList);
        if (traces) cout << "Population created" << endl;

        // Create genetic algorithm solver
        Genetic solver(&paramsList, population, nb_ticks_allowed, true, true);
        if (traces) cout << "Solver created" << endl;

        // Run evolution
        int max_iter = 10000000;
        int maxIterNonProd = c.get_iter() != 0 ? c.get_iter() : 100;
        solver.evolve(max_iter, maxIterNonProd, 1);
        if (traces) cout << "Evolution completed" << endl;

        // Export solution
        population->ExportPop(c.get_path_to_solution(), true);

        // Cleanup
        delete population;
        return 0;
    }
    else
    {
        throw string("Invalid command line arguments. Usage: irp <instance> [options]");
    }
}

/**
 * Main entry point
 *
 * Handles:
 * - Command line parsing
 * - Thread pool initialization (CPU or gRPC mode)
 * - Solver invocation
 * - Resource cleanup
 */
int main(int argc, char *argv[])
{
    std::cout << "========================================" << std::endl;
    std::cout << " IRP Solver - Starting Initialization" << std::endl;
    std::cout << "========================================" << std::endl;

    // Parse command line parameters
    commandline c(argc, argv);

    if (!c.is_valid()) {
        std::cerr << "ERROR: Invalid command line arguments" << std::endl;
        std::cerr << "Usage: irp <instance> [-seed <int>] [-type <int>] [-veh <int>] "
                  << "[-stock <float>] [-scenario <int>] [-threads <int>] [-t <int>]" << std::endl;
        return 1;
    }

    std::cout << "Command line arguments parsed successfully:" << std::endl;
    std::cout << " - Instance: " << c.get_path_to_instance() << std::endl;
    std::cout << " - Scenarios: " << c.get_nb_scenario() << std::endl;
    std::cout << " - Threads: " << c.get_num_threads() << std::endl;
    std::cout << " - Seed: " << c.get_seed() << std::endl;

    // Determine threading mode and initialize thread pools
    size_t num_threads;
    if (c.get_num_threads() == 0) {
        // gRPC mode: threads=0 disables local inventory optimization
        std::cout << "\n✓ gRPC mode enabled (threads=0)" << std::endl;
        std::cout << "  Inventory optimization will use remote GPU server" << std::endl;
        g_thread_pool = nullptr; // Ensure thread pool is empty to trigger gRPC mode

        // Auto-detect CPU cores for routing optimization
        size_t routing_threads = std::thread::hardware_concurrency();
        if (routing_threads == 0) {
            routing_threads = 4; // Fallback if detection fails
        }
        std::cout << "  Routing optimization: " << routing_threads << " CPU cores (auto-detected)" << std::endl;
        initRoutingThreadPool(routing_threads);
    }
    else if (c.get_num_threads() > 0) {
        // Local multi-threading mode
        num_threads = c.get_num_threads();

        // Auto-detect CPU cores for routing optimization
        size_t routing_threads = std::thread::hardware_concurrency();
        if (routing_threads == 0) {
            routing_threads = 4; // Fallback if detection fails
        }

        std::cout << "\n✓ Local multi-threading mode (threads=" << num_threads << ")" << std::endl;
        std::cout << "  Routing optimization: " << routing_threads << " CPU cores (auto-detected)" << std::endl;
        std::cout << "  Inventory optimization: " << num_threads << " threads (user-specified)" << std::endl;

        // Initialize both thread pools
        initRoutingThreadPool(routing_threads);
        initThreadPool(num_threads);
    }
    else {
        // Default: use hardware concurrency
        num_threads = std::thread::hardware_concurrency();
        std::cout << "\n✓ Using default " << num_threads << " threads (hardware concurrency)" << std::endl;
        initThreadPool(num_threads);
    }

    try {
        // Run the solver
        mainIRP(c);

        std::cout << "\n========================================" << std::endl;
        std::cout << " Solver completed successfully" << std::endl;
        std::cout << "========================================" << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "\nERROR: " << e.what() << std::endl;
        return 1;
    }
    catch (const std::string& e) {
        std::cerr << "\nERROR: " << e << std::endl;
        return 1;
    }

    // Clean up thread pools
    if (c.get_num_threads() == 0) {
        // gRPC mode: clean up routing thread pool only
        destroyRoutingThreadPool();
    }
    else if (c.get_num_threads() > 0) {
        // Local mode: clean up both thread pools
        destroyRoutingThreadPool();
        destroyThreadPool();
    }
    else {
        // Default mode: clean up thread pool
        destroyThreadPool();
    }

    return 0;
}
