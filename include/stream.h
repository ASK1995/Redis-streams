#pragma once

#include <map>
#include <vector>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include "stream_entry.h"
#include "consumer_group.h"

class Stream {
public:
    Stream();
    ~Stream();
    
    // Basic stream operations
    StreamID add_entry(const StreamID& id, const std::vector<std::pair<std::string, std::string>>& fields);
    std::vector<StreamEntry> get_range(const StreamID& start, const StreamID& end, int count = -1) const;
    std::vector<StreamEntry> get_entries_after(const StreamID& id, int count = -1) const;
    bool delete_entries(const std::vector<StreamID>& ids);
    size_t length() const;
    
    // Consumer group operations
    bool create_consumer_group(const std::string& group_name, const StreamID& start_id);
    std::shared_ptr<ConsumerGroup> get_consumer_group(const std::string& group_name);
    bool delete_consumer_group(const std::string& group_name);
    
    // Get last entry ID
    StreamID get_last_id() const;
    
    // Blocking operations
    void add_blocked_client(int client_socket, const StreamID& last_id);
    void notify_blocked_clients();
    
private:
    mutable std::mutex entries_mutex_;
    mutable std::mutex groups_mutex_;
    mutable std::mutex blocked_clients_mutex_;
    
    // Entries stored in chronological order
    std::map<StreamID, StreamEntry> entries_;
    
    // Consumer groups
    std::unordered_map<std::string, std::shared_ptr<ConsumerGroup>> consumer_groups_;
    
    // Blocked clients waiting for new entries
    struct BlockedClient {
        int socket;
        StreamID last_id;
    };
    std::vector<BlockedClient> blocked_clients_;
    
    StreamID last_id_;
};
