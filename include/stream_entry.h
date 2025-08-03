#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

struct StreamID {
    uint64_t timestamp_ms;
    uint64_t sequence;
    
    StreamID() : timestamp_ms(0), sequence(0) {}
    StreamID(uint64_t ts, uint64_t seq) : timestamp_ms(ts), sequence(seq) {}
    
    std::string to_string() const;
    static StreamID from_string(const std::string& id_str);
    static StreamID generate_auto();
    
    bool operator<(const StreamID& other) const;
    bool operator==(const StreamID& other) const;
    bool operator>(const StreamID& other) const;
    bool operator<=(const StreamID& other) const;
    bool operator>=(const StreamID& other) const;
    bool operator!=(const StreamID& other) const;
};

class StreamEntry {
public:
    StreamEntry(const StreamID& id, const std::vector<std::pair<std::string, std::string>>& fields);
    
    const StreamID& get_id() const { return id_; }
    const std::unordered_map<std::string, std::string>& get_fields() const { return fields_; }
    
    std::string to_resp_format() const;
    
private:
    StreamID id_;
    std::unordered_map<std::string, std::string> fields_;
};
