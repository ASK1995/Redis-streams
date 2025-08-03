#pragma once

#include <string>
#include <vector>
#include <unordered_set>
#include <mutex>
#include "stream_entry.h"

class Consumer {
public:
    explicit Consumer(const std::string& name);
    ~Consumer();
    
    const std::string& get_name() const { return name_; }
    
    // Pending messages management
    void add_pending_message(const StreamID& id);
    bool remove_pending_message(const StreamID& id);
    bool has_pending_message(const StreamID& id) const;
    std::vector<StreamID> get_pending_messages() const;
    size_t pending_count() const;
    
    // Consumer info
    uint64_t get_seen_time() const { return seen_time_; }
    void update_seen_time();
    
private:
    std::string name_;
    uint64_t seen_time_;
    mutable std::mutex pending_mutex_;
    
    // Set of pending message IDs for this consumer
    std::unordered_set<std::string> pending_messages_; // stream_id as string
};
