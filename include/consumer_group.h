#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <vector>
#include "stream_entry.h"
#include "consumer.h"

class ConsumerGroup {
public:
    ConsumerGroup(const std::string& name, const StreamID& start_id);
    ~ConsumerGroup();
    
    const std::string& get_name() const { return name_; }
    const StreamID& get_last_delivered_id() const { return last_delivered_id_; }
    
    // Consumer management
    std::shared_ptr<Consumer> get_or_create_consumer(const std::string& consumer_name);
    bool delete_consumer(const std::string& consumer_name);
    std::vector<std::string> get_consumer_names() const;
    
    // Message delivery
    std::vector<StreamEntry> read_pending_messages(const std::string& consumer_name, 
                                                   const std::vector<StreamEntry>& available_entries,
                                                   int count = -1);
    
    // Acknowledgment
    int acknowledge_messages(const std::string& consumer_name, const std::vector<StreamID>& ids);
    
    // Pending entries list (PEL)
    std::vector<StreamEntry> get_pending_entries(const std::string& consumer_name = "") const;
    
    void set_last_delivered_id(const StreamID& id);
    
private:
    std::string name_;
    StreamID last_delivered_id_;
    mutable std::mutex consumers_mutex_;
    mutable std::mutex pending_mutex_;
    
    std::unordered_map<std::string, std::shared_ptr<Consumer>> consumers_;
    
    // Pending Entry List (PEL) - messages delivered but not acknowledged
    struct PendingEntry {
        StreamID id;
        std::string consumer_name;
        uint64_t delivery_time;
        int delivery_count;
    };
    std::unordered_map<std::string, PendingEntry> pending_entries_; // key: stream_id_string
};
