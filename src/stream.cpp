#include "stream.h"
#include <algorithm>
#include <chrono>

Stream::Stream() : last_id_(0, 0) {
}

Stream::~Stream() = default;

StreamID Stream::add_entry(const StreamID& id, const std::vector<std::pair<std::string, std::string>>& fields) {
    std::lock_guard<std::mutex> lock(entries_mutex_);
    
    StreamID actual_id = id;
    
    // Handle auto-generation and sequencing
    if (id.timestamp_ms == 0 && id.sequence == 0) {
        // Full auto-generation
        actual_id = StreamID::generate_auto();
        if (actual_id <= last_id_) {
            actual_id = StreamID(last_id_.timestamp_ms, last_id_.sequence + 1);
        }
    } else if (id.sequence == 0) {
        // Auto-generate sequence for given timestamp
        if (id.timestamp_ms == last_id_.timestamp_ms) {
            actual_id = StreamID(id.timestamp_ms, last_id_.sequence + 1);
        } else if (id.timestamp_ms > last_id_.timestamp_ms) {
            actual_id = StreamID(id.timestamp_ms, 0);
        } else {
            throw std::invalid_argument("Stream ID must be greater than last ID");
        }
    } else {
        // Explicit ID provided
        if (actual_id <= last_id_) {
            throw std::invalid_argument("Stream ID must be greater than last ID");
        }
    }
    
    // Create and store the entry
    entries_.emplace(actual_id, StreamEntry(actual_id, fields));
    last_id_ = actual_id;
    
    // Notify blocked clients
    notify_blocked_clients();
    
    return actual_id;
}

std::vector<StreamEntry> Stream::get_range(const StreamID& start, const StreamID& end, int count) const {
    std::lock_guard<std::mutex> lock(entries_mutex_);
    
    std::vector<StreamEntry> result;
    
    auto start_it = (start.timestamp_ms == 0 && start.sequence == 0) ? 
                    entries_.begin() : entries_.lower_bound(start);
    
    auto end_it = (end.timestamp_ms == UINT64_MAX && end.sequence == UINT64_MAX) ? 
                  entries_.end() : entries_.upper_bound(end);
    
    int added = 0;
    for (auto it = start_it; it != end_it && (count < 0 || added < count); ++it, ++added) {
        result.push_back(it->second);
    }
    
    return result;
}

std::vector<StreamEntry> Stream::get_entries_after(const StreamID& id, int count) const {
    std::lock_guard<std::mutex> lock(entries_mutex_);
    
    std::vector<StreamEntry> result;
    
    auto it = entries_.upper_bound(id);
    int added = 0;
    
    for (; it != entries_.end() && (count < 0 || added < count); ++it, ++added) {
        result.push_back(it->second);
    }
    
    return result;
}

bool Stream::delete_entries(const std::vector<StreamID>& ids) {
    std::lock_guard<std::mutex> lock(entries_mutex_);
    
    bool any_deleted = false;
    for (const auto& id : ids) {
        if (entries_.erase(id) > 0) {
            any_deleted = true;
        }
    }
    
    return any_deleted;
}

size_t Stream::length() const {
    std::lock_guard<std::mutex> lock(entries_mutex_);
    return entries_.size();
}

bool Stream::create_consumer_group(const std::string& group_name, const StreamID& start_id) {
    std::lock_guard<std::mutex> lock(groups_mutex_);
    
    if (consumer_groups_.count(group_name) > 0) {
        return false; // Group already exists
    }
    
    StreamID actual_start_id = start_id;
    
    // Handle special start IDs
    if (start_id.timestamp_ms == 0 && start_id.sequence == 0) {
        // Start from beginning
        actual_start_id = StreamID(0, 0);
    } else if (start_id.timestamp_ms == UINT64_MAX && start_id.sequence == UINT64_MAX) {
        // Start from end ($ in Redis)
        actual_start_id = last_id_;
    }
    
    auto group = std::make_shared<ConsumerGroup>(group_name, actual_start_id);
    consumer_groups_[group_name] = group;
    
    return true;
}

std::shared_ptr<ConsumerGroup> Stream::get_consumer_group(const std::string& group_name) {
    std::lock_guard<std::mutex> lock(groups_mutex_);
    
    auto it = consumer_groups_.find(group_name);
    return (it != consumer_groups_.end()) ? it->second : nullptr;
}

bool Stream::delete_consumer_group(const std::string& group_name) {
    std::lock_guard<std::mutex> lock(groups_mutex_);
    return consumer_groups_.erase(group_name) > 0;
}

StreamID Stream::get_last_id() const {
    std::lock_guard<std::mutex> lock(entries_mutex_);
    return last_id_;
}

void Stream::add_blocked_client(int client_socket, const StreamID& last_id) {
    std::lock_guard<std::mutex> lock(blocked_clients_mutex_);
    blocked_clients_.push_back({client_socket, last_id});
}

void Stream::notify_blocked_clients() {
    std::lock_guard<std::mutex> lock(blocked_clients_mutex_);
    
    // In a real implementation, we would notify the blocked clients here
    // For now, we'll just clear the list as a placeholder
    blocked_clients_.clear();
}
