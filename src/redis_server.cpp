#include "redis_server.h"
#include "redis_protocol.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>

RedisServer::RedisServer(int port) 
    : port_(port), server_socket_(-1), running_(false) {
}

RedisServer::~RedisServer() {
    stop();
}

void RedisServer::start() {
    // Create socket
    server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_ == -1) {
        throw std::runtime_error("Failed to create socket");
    }
    
    // Set socket options
    int opt = 1;
    if (setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        close(server_socket_);
        throw std::runtime_error("Failed to set socket options");
    }
    
    // Bind socket
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port_);
    
    if (bind(server_socket_, (struct sockaddr*)&address, sizeof(address)) < 0) {
        close(server_socket_);
        throw std::runtime_error("Failed to bind socket");
    }
    
    // Listen
    if (listen(server_socket_, 3) < 0) {
        close(server_socket_);
        throw std::runtime_error("Failed to listen on socket");
    }
    
    running_ = true;
    std::cout << "Redis Streams Server listening on port " << port_ << std::endl;
    
    // Start accepting connections in a separate thread
    accept_thread_ = std::thread(&RedisServer::accept_connections, this);
}

void RedisServer::stop() {
    running_ = false;
    
    if (server_socket_ != -1) {
        close(server_socket_);
        server_socket_ = -1;
    }
    
    if (accept_thread_.joinable()) {
        accept_thread_.join();
    }
}

void RedisServer::accept_connections() {
    while (running_) {
        struct sockaddr_in client_address;
        socklen_t client_len = sizeof(client_address);
        
        int client_socket = accept(server_socket_, (struct sockaddr*)&client_address, &client_len);
        
        if (client_socket < 0) {
            if (running_) {
                std::cerr << "Failed to accept client connection" << std::endl;
            }
            continue;
        }
        
        // Handle client in a separate thread
        std::thread client_thread(&RedisServer::handle_client, this, client_socket);
        client_thread.detach();
    }
}

void RedisServer::handle_client(int client_socket) {
    char buffer[4096];
    std::string accumulated_data;
    
    while (running_) {
        ssize_t bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        
        if (bytes_received <= 0) {
            break; // Client disconnected or error
        }
        
        buffer[bytes_received] = '\0';
        accumulated_data += buffer;
        
        // Process complete commands
        size_t pos = 0;
        while (pos < accumulated_data.length()) {
            size_t command_end = accumulated_data.find("\r\n", pos);
            if (command_end == std::string::npos) {
                // Need more data for RESP parsing
                if (accumulated_data[pos] == '*') {
                    // Try to parse as array command
                    try {
                        std::string command_data = accumulated_data.substr(pos);
                        std::string response = process_command(command_data);
                        
                        send(client_socket, response.c_str(), response.length(), 0);
                        accumulated_data.clear();
                        break;
                    } catch (const std::exception&) {
                        // Not enough data yet, continue reading
                        break;
                    }
                } else {
                    break; // Wait for more data
                }
            } else {
                // Simple inline command
                std::string command = accumulated_data.substr(pos, command_end - pos);
                std::string response = process_command(command);
                
                send(client_socket, response.c_str(), response.length(), 0);
                pos = command_end + 2;
            }
        }
        
        if (pos > 0) {
            accumulated_data = accumulated_data.substr(pos);
        }
    }
    
    close(client_socket);
}

std::string RedisServer::process_command(const std::string& command) {
    try {
        auto parts = RedisProtocol::parse_command(command);
        
        if (parts.empty()) {
            return RedisProtocol::format_error("ERR empty command");
        }
        
        // Convert command to uppercase
        std::string cmd = parts[0];
        std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);
        
        if (cmd == "XADD") {
            if (parts.size() < 4 || (parts.size() - 3) % 2 != 0) {
                return RedisProtocol::format_error("ERR wrong number of arguments for 'xadd' command");
            }
            
            std::string stream_name = parts[1];
            std::string id = parts[2];
            
            std::vector<std::pair<std::string, std::string>> fields;
            for (size_t i = 3; i < parts.size(); i += 2) {
                fields.emplace_back(parts[i], parts[i + 1]);
            }
            
            return xadd(stream_name, id, fields);
            
        } else if (cmd == "XREAD") {
            // XREAD [COUNT count] [BLOCK milliseconds] STREAMS key [key ...] id [id ...]
            size_t streams_pos = 0;
            int count = -1;
            int block = -1;
            
            for (size_t i = 1; i < parts.size(); i++) {
                std::string arg = parts[i];
                std::transform(arg.begin(), arg.end(), arg.begin(), ::toupper);
                
                if (arg == "STREAMS") {
                    streams_pos = i + 1;
                    break;
                } else if (arg == "COUNT" && i + 1 < parts.size()) {
                    count = std::stoi(parts[++i]);
                } else if (arg == "BLOCK" && i + 1 < parts.size()) {
                    block = std::stoi(parts[++i]);
                }
            }
            
            if (streams_pos == 0 || streams_pos >= parts.size()) {
                return RedisProtocol::format_error("ERR wrong number of arguments for 'xread' command");
            }
            
            size_t num_streams = (parts.size() - streams_pos) / 2;
            if (num_streams == 0 || (parts.size() - streams_pos) % 2 != 0) {
                return RedisProtocol::format_error("ERR Unbalanced XREAD list of streams: for each stream key an ID or $ must be specified");
            }
            
            std::vector<std::string> streams(parts.begin() + streams_pos, parts.begin() + streams_pos + num_streams);
            std::vector<std::string> ids(parts.begin() + streams_pos + num_streams, parts.end());
            
            return xread(streams, ids, count, block);
            
        } else if (cmd == "XRANGE") {
            if (parts.size() < 4 || parts.size() > 6) {
                return RedisProtocol::format_error("ERR wrong number of arguments for 'xrange' command");
            }
            
            std::string stream_name = parts[1];
            std::string start = parts[2];
            std::string end = parts[3];
            int count = -1;
            
            if (parts.size() >= 6 && parts[4] == "COUNT") {
                count = std::stoi(parts[5]);
            }
            
            return xrange(stream_name, start, end, count);
            
        } else if (cmd == "XLEN") {
            if (parts.size() != 2) {
                return RedisProtocol::format_error("ERR wrong number of arguments for 'xlen' command");
            }
            
            return xlen(parts[1]);
            
        } else if (cmd == "XDEL") {
            if (parts.size() < 3) {
                return RedisProtocol::format_error("ERR wrong number of arguments for 'xdel' command");
            }
            
            std::string stream_name = parts[1];
            std::vector<std::string> ids(parts.begin() + 2, parts.end());
            
            return xdel(stream_name, ids);
            
        } else if (cmd == "XGROUP") {
            if (parts.size() < 2) {
                return RedisProtocol::format_error("ERR wrong number of arguments for 'xgroup' command");
            }
            
            std::string subcommand = parts[1];
            std::transform(subcommand.begin(), subcommand.end(), subcommand.begin(), ::toupper);
            
            if (subcommand == "CREATE" && parts.size() == 5) {
                return xgroup_create(parts[2], parts[3], parts[4]);
            } else {
                return RedisProtocol::format_error("ERR Unknown XGROUP subcommand or wrong number of arguments");
            }
            
        } else if (cmd == "XREADGROUP") {
            // XREADGROUP GROUP group consumer [COUNT count] [BLOCK milliseconds] STREAMS key [key ...] ID [ID ...]
            if (parts.size() < 6) {
                return RedisProtocol::format_error("ERR wrong number of arguments for 'xreadgroup' command");
            }
            
            std::string group_name = parts[2];
            std::string consumer_name = parts[3];
            
            size_t streams_pos = 0;
            int count = -1;
            int block = -1;
            
            for (size_t i = 4; i < parts.size(); i++) {
                std::string arg = parts[i];
                std::transform(arg.begin(), arg.end(), arg.begin(), ::toupper);
                
                if (arg == "STREAMS") {
                    streams_pos = i + 1;
                    break;
                } else if (arg == "COUNT" && i + 1 < parts.size()) {
                    count = std::stoi(parts[++i]);
                } else if (arg == "BLOCK" && i + 1 < parts.size()) {
                    block = std::stoi(parts[++i]);
                }
            }
            
            if (streams_pos == 0 || streams_pos >= parts.size()) {
                return RedisProtocol::format_error("ERR wrong number of arguments for 'xreadgroup' command");
            }
            
            size_t num_streams = (parts.size() - streams_pos) / 2;
            if (num_streams == 0) {
                return RedisProtocol::format_error("ERR Unbalanced XREADGROUP list of streams");
            }
            
            std::vector<std::string> streams(parts.begin() + streams_pos, parts.begin() + streams_pos + num_streams);
            std::vector<std::string> ids(parts.begin() + streams_pos + num_streams, parts.end());
            
            return xreadgroup(group_name, consumer_name, streams, ids, count, block);
            
        } else if (cmd == "XACK") {
            if (parts.size() < 4) {
                return RedisProtocol::format_error("ERR wrong number of arguments for 'xack' command");
            }
            
            std::string stream_name = parts[1];
            std::string group_name = parts[2];
            std::vector<std::string> ids(parts.begin() + 3, parts.end());
            
            return xack(stream_name, group_name, ids);
            
        } else if (cmd == "PING") {
            return RedisProtocol::format_simple_string("PONG");
            
        } else {
            return RedisProtocol::format_error("ERR unknown command '" + parts[0] + "'");
        }
        
    } catch (const std::exception& e) {
        return RedisProtocol::format_error("ERR " + std::string(e.what()));
    }
}

// Stream command implementations will be in separate files
// For now, let's implement them here directly

std::string RedisServer::xadd(const std::string& stream_name, const std::string& id, 
                              const std::vector<std::pair<std::string, std::string>>& fields) {
    std::lock_guard<std::mutex> lock(streams_mutex_);
    
    auto& stream = streams_[stream_name];
    if (!stream) {
        stream = std::make_shared<Stream>();
    }
    
    try {
        StreamID stream_id = StreamID::from_string(id);
        StreamID actual_id = stream->add_entry(stream_id, fields);
        return RedisProtocol::format_bulk_string(actual_id.to_string());
    } catch (const std::exception& e) {
        return RedisProtocol::format_error("ERR " + std::string(e.what()));
    }
}

std::string RedisServer::xread(const std::vector<std::string>& streams, 
                               const std::vector<std::string>& ids, 
                               int count, int /* block */) {
    std::vector<std::pair<std::string, std::vector<StreamEntry>>> results;
    
    std::lock_guard<std::mutex> lock(streams_mutex_);
    
    for (size_t i = 0; i < streams.size(); i++) {
        const std::string& stream_name = streams[i];
        const std::string& id_str = ids[i];
        
        auto it = streams_.find(stream_name);
        if (it == streams_.end()) {
            continue; // Stream doesn't exist
        }
        
        try {
            StreamID start_id = StreamID::from_string(id_str);
            auto entries = it->second->get_entries_after(start_id, count);
            
            if (!entries.empty()) {
                results.emplace_back(stream_name, entries);
            }
        } catch (const std::exception&) {
            continue; // Skip invalid ID
        }
    }
    
    return RedisProtocol::format_stream_read_response(results);
}

std::string RedisServer::xrange(const std::string& stream_name, 
                                const std::string& start, const std::string& end, 
                                int count) {
    std::lock_guard<std::mutex> lock(streams_mutex_);
    
    auto it = streams_.find(stream_name);
    if (it == streams_.end()) {
        return RedisProtocol::format_null_array();
    }
    
    try {
        StreamID start_id = (start == "-") ? StreamID(0, 0) : StreamID::from_string(start);
        StreamID end_id = (end == "+") ? StreamID(UINT64_MAX, UINT64_MAX) : StreamID::from_string(end);
        
        auto entries = it->second->get_range(start_id, end_id, count);
        return RedisProtocol::format_stream_entries(entries);
    } catch (const std::exception& e) {
        return RedisProtocol::format_error("ERR " + std::string(e.what()));
    }
}

std::string RedisServer::xlen(const std::string& stream_name) {
    std::lock_guard<std::mutex> lock(streams_mutex_);
    
    auto it = streams_.find(stream_name);
    if (it == streams_.end()) {
        return RedisProtocol::format_integer(0);
    }
    
    return RedisProtocol::format_integer(static_cast<int64_t>(it->second->length()));
}

std::string RedisServer::xdel(const std::string& stream_name, const std::vector<std::string>& ids) {
    std::lock_guard<std::mutex> lock(streams_mutex_);
    
    auto it = streams_.find(stream_name);
    if (it == streams_.end()) {
        return RedisProtocol::format_integer(0);
    }
    
    std::vector<StreamID> stream_ids;
    for (const std::string& id_str : ids) {
        try {
            stream_ids.push_back(StreamID::from_string(id_str));
        } catch (const std::exception&) {
            // Skip invalid IDs
        }
    }
    
    bool deleted = it->second->delete_entries(stream_ids);
    return RedisProtocol::format_integer(deleted ? stream_ids.size() : 0);
}

std::string RedisServer::xgroup_create(const std::string& stream_name, 
                                       const std::string& group_name, 
                                       const std::string& start_id) {
    std::lock_guard<std::mutex> lock(streams_mutex_);
    
    auto& stream = streams_[stream_name];
    if (!stream) {
        stream = std::make_shared<Stream>();
    }
    
    try {
        StreamID id = (start_id == "$") ? stream->get_last_id() : StreamID::from_string(start_id);
        bool created = stream->create_consumer_group(group_name, id);
        
        if (created) {
            return RedisProtocol::format_simple_string("OK");
        } else {
            return RedisProtocol::format_error("BUSYGROUP Consumer Group name already exists");
        }
    } catch (const std::exception& e) {
        return RedisProtocol::format_error("ERR " + std::string(e.what()));
    }
}

std::string RedisServer::xreadgroup(const std::string& group_name, const std::string& consumer_name,
                                    const std::vector<std::string>& streams,
                                    const std::vector<std::string>& /* ids */,
                                    int count, int /* block */) {
    std::vector<std::pair<std::string, std::vector<StreamEntry>>> results;
    
    std::lock_guard<std::mutex> lock(streams_mutex_);
    
    for (size_t i = 0; i < streams.size(); i++) {
        const std::string& stream_name = streams[i];
        
        auto it = streams_.find(stream_name);
        if (it == streams_.end()) {
            continue;
        }
        
        auto group = it->second->get_consumer_group(group_name);
        if (!group) {
            continue; // Group doesn't exist
        }
        
        // Get available entries after the group's last delivered ID
        auto available_entries = it->second->get_entries_after(group->get_last_delivered_id(), -1);
        
        // Read pending messages for this consumer
        auto entries = group->read_pending_messages(consumer_name, available_entries, count);
        
        if (!entries.empty()) {
            results.emplace_back(stream_name, entries);
        }
    }
    
    return RedisProtocol::format_stream_read_response(results);
}

std::string RedisServer::xack(const std::string& stream_name, const std::string& group_name,
                              const std::vector<std::string>& ids) {
    std::lock_guard<std::mutex> lock(streams_mutex_);
    
    auto it = streams_.find(stream_name);
    if (it == streams_.end()) {
        return RedisProtocol::format_integer(0);
    }
    
    auto group = it->second->get_consumer_group(group_name);
    if (!group) {
        return RedisProtocol::format_integer(0);
    }
    
    std::vector<StreamID> stream_ids;
    for (const std::string& id_str : ids) {
        try {
            stream_ids.push_back(StreamID::from_string(id_str));
        } catch (const std::exception&) {
            // Skip invalid IDs
        }
    }
    
    // For XACK, we need to know which consumer to acknowledge for
    // In a real implementation, this would be tracked better
    // For now, let's acknowledge for all consumers in the group
    int acknowledged = 0;
    for (const std::string& consumer_name : group->get_consumer_names()) {
        acknowledged += group->acknowledge_messages(consumer_name, stream_ids);
    }
    
    return RedisProtocol::format_integer(acknowledged);
}
