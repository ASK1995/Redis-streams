#include "redis_server.h"
#include <iostream>
#include <signal.h>
#include <memory>
#include <thread>
#include <chrono>

std::unique_ptr<RedisServer> server;

void signal_handler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down server..." << std::endl;
    if (server) {
        server->stop();
    }
    exit(0);
}

int main(int argc, char* argv[]) {
    // Handle command line arguments
    int port = 6379; // Default Redis port
    
    if (argc > 1) {
        try {
            port = std::stoi(argv[1]);
        } catch (const std::exception& e) {
            std::cerr << "Invalid port number: " << argv[1] << std::endl;
            return 1;
        }
    }
    
    // Set up signal handling
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    try {
        std::cout << "Starting Redis Streams Service..." << std::endl;
        
        // Create and start the server
        server = std::make_unique<RedisServer>(port);
        server->start();
        
        std::cout << "Server started successfully. Press Ctrl+C to stop." << std::endl;
        
        // Keep the main thread alive
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
