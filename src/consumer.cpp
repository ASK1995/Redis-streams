#include "consumer.h"
#include <chrono>
#include <algorithm>

Consumer::Consumer(const std::string& name) 
    : name_(name) {
    update_seen_time();
}

Consumer::~Consumer() = default;

void Consumer::add_pending_message(const StreamID& id) {
    std::lock_guard<std::mutex> lock(pending_mutex_);
    pending_messages_.insert(id.to_string());
}

bool Consumer::remove_pending_message(const StreamID& id) {
    std::lock_guard<std::mutex> lock(pending_mutex_);
    return pending_messages_.erase(id.to_string()) > 0;
}

bool Consumer::has_pending_message(const StreamID& id) const {
    std::lock_guard<std::mutex> lock(pending_mutex_);
    return pending_messages_.count(id.to_string()) > 0;
}

std::vector<StreamID> Consumer::get_pending_messages() const {
    std::lock_guard<std::mutex> lock(pending_mutex_);
    std::vector<StreamID> result;
    
    for (const std::string& id_str : pending_messages_) {
        try {
            result.push_back(StreamID::from_string(id_str));
        } catch (const std::exception&) {
            // Skip invalid IDs
        }
    }
    
    return result;
}

size_t Consumer::pending_count() const {
    std::lock_guard<std::mutex> lock(pending_mutex_);
    return pending_messages_.size();
}

void Consumer::update_seen_time() {
    seen_time_ = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}
