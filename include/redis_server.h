#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include "stream.h"

class RedisServer {
public:
    explicit RedisServer(int port = 6379);
    ~RedisServer();
    
    void start();
    void stop();
    
    // Stream operations
    std::string xadd(const std::string& stream_name, const std::string& id, 
                     const std::vector<std::pair<std::string, std::string>>& fields);
    std::string xread(const std::vector<std::string>& streams, 
                      const std::vector<std::string>& ids, 
                      int count = -1, int block = -1);
    std::string xrange(const std::string& stream_name, 
                       const std::string& start, const std::string& end, 
                       int count = -1);
    std::string xlen(const std::string& stream_name);
    std::string xdel(const std::string& stream_name, const std::vector<std::string>& ids);
    
    // Consumer group operations
    std::string xgroup_create(const std::string& stream_name, 
                              const std::string& group_name, 
                              const std::string& start_id);
    std::string xreadgroup(const std::string& group_name, const std::string& consumer_name,
                           const std::vector<std::string>& streams,
                           const std::vector<std::string>& ids,
                           int count = -1, int block = -1);
    std::string xack(const std::string& stream_name, const std::string& group_name,
                     const std::vector<std::string>& ids);

private:
    void accept_connections();
    void handle_client(int client_socket);
    std::string process_command(const std::string& command);
    
    int port_;
    int server_socket_;
    std::atomic<bool> running_;
    std::thread accept_thread_;
    
    // Storage
    std::unordered_map<std::string, std::shared_ptr<Stream>> streams_;
    mutable std::mutex streams_mutex_;
};
