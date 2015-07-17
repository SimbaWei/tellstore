#include "ServerConfig.hpp"
#include "ServerSocket.hpp"

#include <tellstore.hpp>
#include <util/StorageConfig.hpp>

#include <crossbow/allocator.hpp>
#include <crossbow/infinio/InfinibandService.hpp>
#include <crossbow/logger.hpp>
#include <crossbow/program_options.hpp>

#include <iostream>
#include <system_error>

int main(int argc, const char** argv) {
    tell::store::StorageConfig storageConfig;
    tell::store::ServerConfig serverConfig;
    bool help = false;
    crossbow::string logLevel("DEBUG");

    auto opts = crossbow::program_options::create_options(argv[0],
            crossbow::program_options::value<'h'>("help", &help),
            crossbow::program_options::value<'l'>("log-level", &logLevel),
            crossbow::program_options::value<'p'>("port", &serverConfig.port),
            crossbow::program_options::value<'m'>("memory", &storageConfig.totalMemory),
            crossbow::program_options::value<'c'>("capacity", &storageConfig.hashMapCapacity),
            crossbow::program_options::value<'s'>("scan-threads", &storageConfig.numScanThreads));

    try {
        crossbow::program_options::parse(opts, argc, argv);
    } catch (crossbow::program_options::argument_not_found e) {
        std::cerr << e.what() << std::endl << std::endl;
        crossbow::program_options::print_help(std::cout, opts);
        return 1;
    }

    if (help) {
        crossbow::program_options::print_help(std::cout, opts);
        return 0;
    }

    crossbow::infinio::InfinibandLimits infinibandLimits;
    infinibandLimits.receiveBufferCount = 128;
    infinibandLimits.sendBufferCount = 128;
    infinibandLimits.bufferLength = 32 * 1024;
    infinibandLimits.sendQueueLength = 128;
    infinibandLimits.maxScatterGather = 32;

    crossbow::logger::logger->config.level = crossbow::logger::logLevelFromString(logLevel);

    LOG_INFO("Starting TellStore server [port = %1%, memory = %2%GB, capacity = %3%, scan-threads = %4%]",
            serverConfig.port, double(storageConfig.totalMemory) / double(1024 * 1024 * 1024),
            storageConfig.hashMapCapacity, storageConfig.numScanThreads);

    // Initialize allocator
    crossbow::allocator::init();

    LOG_INFO("Initialize storage");
    tell::store::Storage storage(storageConfig);

    LOG_INFO("Initialize network server");
    crossbow::infinio::InfinibandService service(infinibandLimits);
    tell::store::ServerManager server(service, storage, serverConfig);
    service.run();

    LOG_INFO("Exiting TellStore server");
    return 0;
}
