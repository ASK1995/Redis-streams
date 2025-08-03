#include "consumer_group.h"
#include <chrono>
#include <algorithm>

ConsumerGroup::ConsumerGroup(const std::string& name, const StreamID& start_id)
    : name_(name), last_delivered_id_(start_id) {
}

ConsumerGroup::~ConsumerGroup() = default;

std::shared_ptr<Consumer> ConsumerGroup::get_or_create_consumer(const std::string& consumer_name) {
    std::lock_guard<std::mutex> lock(consumers_mutex_);
    
    auto it = consumers_.find(consumer_name);
    if (it != consumers_.end()) {
        return it->second;
    }
    
    auto consumer = std::make_shared<Consumer>(consumer_name);
    consumers_[consumer_name] = consumer;
    return consumer;
}

bool ConsumerGroup::delete_consumer(const std::string& consumer_name) {
    std::lock_guard<std::mutex> lock(consumers_mutex_);
    return consumers_.erase(consumer_name) > 0;
}

std::vector<std::string> ConsumerGroup::get_consumer_names() const {
    std::lock_guard<std::mutex> lock(consumers_mutex_);
    
    std::vector<std::string> names;
    names.reserve(consumers_.size());
    
    for (const auto& pair : consumers_) {
        names.push_back(pair.first);
    }
    
    return names;
}

std::vector<StreamEntry> ConsumerGroup::read_pending_messages(
    const std::string& consumer_name,
    const std::vector<StreamEntry>& available_entries,
    int count) {
    
    auto consumer = get_or_create_consumer(consumer_name);
    consumer->update_seen_time();
    
    std::vector<StreamEntry> result;
    int delivered = 0;
    
    for (const auto& entry : available_entries) {
        if (count > 0 && delivered >= count) {
            break;
        }
        
        if (entry.get_id() > last_delivered_id_) {
            result.push_back(entry);
            
            // Add to pending list
            {
                std::lock_guard<std::mutex> lock(pending_mutex_);
                PendingEntry pending{
                    entry.get_id(),
                    consumer_name,
                    static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count()),
                    1
                };
                pending_entries_[entry.get_id().to_string()] = pending;
            }
            
            consumer->add_pending_message(entry.get_id());
            last_delivered_id_ = entry.get_id();
            delivered++;
        }
    }
    
    return result;
}

int ConsumerGroup::acknowledge_messages(const std::string& consumer_name, const std::vector<StreamID>& ids) {
    auto consumer = get_or_create_consumer(consumer_name);
    int acknowledged = 0;
    
    for (const auto& id : ids) {
        std::string id_str = id.to_string();
        
        {
            std::lock_guard<std::mutex> lock(pending_mutex_);
            auto it = pending_entries_.find(id_str);
            if (it != pending_entries_.end() && it->second.consumer_name == consumer_name) {
                pending_entries_.erase(it);
                acknowledged++;
            }
        }
        
        consumer->remove_pending_message(id);
    }
    
    return acknowledged;
}

std::vector<StreamEntry> ConsumerGroup::get_pending_entries(const std::string& consumer_name) const {
    std::lock_guard<std::mutex> lock(pending_mutex_);
    
    std::vector<StreamEntry> result;
    
    for (const auto& pair : pending_entries_) {
        if (consumer_name.empty() || pair.second.consumer_name == consumer_name) {
            // Note: We can't fully reconstruct StreamEntry from just the ID
            // In a real implementation, we'd store more information or reference back to the stream
            // For now, create empty entries with just the ID
            std::vector<std::pair<std::string, std::string>> empty_fields;
            result.emplace_back(pair.second.id, empty_fields);
        }
    }
    
    return result;
}

void ConsumerGroup::set_last_delivered_id(const StreamID& id) {
    last_delivered_id_ = id;
}
