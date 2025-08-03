#include "stream_entry.h"
#include <chrono>
#include <sstream>
#include <stdexcept>

// StreamID implementation
std::string StreamID::to_string() const {
    return std::to_string(timestamp_ms) + "-" + std::to_string(sequence);
}

StreamID StreamID::from_string(const std::string& id_str) {
    if (id_str == "*") {
        return generate_auto();
    }
    
    size_t dash_pos = id_str.find('-');
    if (dash_pos == std::string::npos) {
        throw std::invalid_argument("Invalid stream ID format");
    }
    
    uint64_t timestamp = std::stoull(id_str.substr(0, dash_pos));
    uint64_t sequence;
    
    std::string seq_str = id_str.substr(dash_pos + 1);
    if (seq_str == "*") {
        sequence = 0; // Will be auto-incremented
    } else {
        sequence = std::stoull(seq_str);
    }
    
    return StreamID(timestamp, sequence);
}

StreamID StreamID::generate_auto() {
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return StreamID(static_cast<uint64_t>(now), 0);
}

bool StreamID::operator<(const StreamID& other) const {
    if (timestamp_ms != other.timestamp_ms) {
        return timestamp_ms < other.timestamp_ms;
    }
    return sequence < other.sequence;
}

bool StreamID::operator==(const StreamID& other) const {
    return timestamp_ms == other.timestamp_ms && sequence == other.sequence;
}

bool StreamID::operator>(const StreamID& other) const {
    return other < *this;
}

bool StreamID::operator<=(const StreamID& other) const {
    return *this < other || *this == other;
}

bool StreamID::operator>=(const StreamID& other) const {
    return *this > other || *this == other;
}

bool StreamID::operator!=(const StreamID& other) const {
    return !(*this == other);
}

// StreamEntry implementation
StreamEntry::StreamEntry(const StreamID& id, const std::vector<std::pair<std::string, std::string>>& fields)
    : id_(id) {
    for (const auto& field : fields) {
        fields_[field.first] = field.second;
    }
}

std::string StreamEntry::to_resp_format() const {
    std::ostringstream oss;
    
    // Entry format: [stream_id, [field1, value1, field2, value2, ...]]
    oss << "*2\r\n"; // Array of 2 elements
    
    // Stream ID as bulk string
    std::string id_str = id_.to_string();
    oss << "$" << id_str.length() << "\r\n" << id_str << "\r\n";
    
    // Fields array
    oss << "*" << (fields_.size() * 2) << "\r\n"; // Each field has key and value
    
    for (const auto& field : fields_) {
        // Field name
        oss << "$" << field.first.length() << "\r\n" << field.first << "\r\n";
        // Field value
        oss << "$" << field.second.length() << "\r\n" << field.second << "\r\n";
    }
    
    return oss.str();
}
