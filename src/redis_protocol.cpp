#include "redis_protocol.h"
#include "stream_entry.h"
#include <sstream>
#include <stdexcept>

std::vector<std::string> RedisProtocol::parse_command(const std::string& input) {
    if (input.empty()) {
        return {};
    }
    
    size_t pos = 0;
    
    if (input[0] == '*') {
        return parse_array(input, pos);
    } else {
        // Handle inline commands (simple space-separated)
        std::vector<std::string> parts;
        std::istringstream iss(input);
        std::string part;
        
        while (iss >> part) {
            parts.push_back(part);
        }
        
        return parts;
    }
}

std::string RedisProtocol::format_simple_string(const std::string& str) {
    return "+" + str + "\r\n";
}

std::string RedisProtocol::format_error(const std::string& error) {
    return "-" + error + "\r\n";
}

std::string RedisProtocol::format_integer(int64_t value) {
    return ":" + std::to_string(value) + "\r\n";
}

std::string RedisProtocol::format_bulk_string(const std::string& str) {
    return "$" + std::to_string(str.length()) + "\r\n" + str + "\r\n";
}

std::string RedisProtocol::format_array(const std::vector<std::string>& elements) {
    std::ostringstream oss;
    oss << "*" << elements.size() << "\r\n";
    
    for (const auto& element : elements) {
        oss << format_bulk_string(element);
    }
    
    return oss.str();
}

std::string RedisProtocol::format_null_bulk_string() {
    return "$-1\r\n";
}

std::string RedisProtocol::format_null_array() {
    return "*-1\r\n";
}

std::string RedisProtocol::format_stream_entries(const std::vector<StreamEntry>& entries) {
    if (entries.empty()) {
        return format_null_array();
    }
    
    std::ostringstream oss;
    oss << "*" << entries.size() << "\r\n";
    
    for (const auto& entry : entries) {
        oss << entry.to_resp_format();
    }
    
    return oss.str();
}

std::string RedisProtocol::format_stream_read_response(
    const std::vector<std::pair<std::string, std::vector<StreamEntry>>>& stream_entries) {
    
    if (stream_entries.empty()) {
        return format_null_array();
    }
    
    std::ostringstream oss;
    oss << "*" << stream_entries.size() << "\r\n";
    
    for (const auto& stream_pair : stream_entries) {
        const std::string& stream_name = stream_pair.first;
        const std::vector<StreamEntry>& entries = stream_pair.second;
        
        // Each stream response is an array of [stream_name, entries_array]
        oss << "*2\r\n";
        oss << format_bulk_string(stream_name);
        oss << format_stream_entries(entries);
    }
    
    return oss.str();
}

// Private helper methods

std::string RedisProtocol::parse_bulk_string(const std::string& input, size_t& pos) {
    if (pos >= input.length() || input[pos] != '$') {
        throw std::invalid_argument("Expected bulk string");
    }
    
    pos++; // Skip '$'
    
    // Find the length
    size_t length_end = input.find("\r\n", pos);
    if (length_end == std::string::npos) {
        throw std::invalid_argument("Invalid bulk string format");
    }
    
    int length = std::stoi(input.substr(pos, length_end - pos));
    pos = length_end + 2; // Skip "\r\n"
    
    if (length == -1) {
        return ""; // Null bulk string
    }
    
    if (pos + length > input.length()) {
        throw std::invalid_argument("Bulk string length exceeds input");
    }
    
    std::string result = input.substr(pos, length);
    pos += length + 2; // Skip the string and trailing "\r\n"
    
    return result;
}

int64_t RedisProtocol::parse_integer(const std::string& input, size_t& pos) {
    if (pos >= input.length() || input[pos] != ':') {
        throw std::invalid_argument("Expected integer");
    }
    
    pos++; // Skip ':'
    
    size_t end = input.find("\r\n", pos);
    if (end == std::string::npos) {
        throw std::invalid_argument("Invalid integer format");
    }
    
    int64_t result = std::stoll(input.substr(pos, end - pos));
    pos = end + 2; // Skip "\r\n"
    
    return result;
}

std::vector<std::string> RedisProtocol::parse_array(const std::string& input, size_t& pos) {
    if (pos >= input.length() || input[pos] != '*') {
        throw std::invalid_argument("Expected array");
    }
    
    pos++; // Skip '*'
    
    // Find the array length
    size_t length_end = input.find("\r\n", pos);
    if (length_end == std::string::npos) {
        throw std::invalid_argument("Invalid array format");
    }
    
    int length = std::stoi(input.substr(pos, length_end - pos));
    pos = length_end + 2; // Skip "\r\n"
    
    if (length == -1) {
        return {}; // Null array
    }
    
    std::vector<std::string> result;
    result.reserve(length);
    
    for (int i = 0; i < length; i++) {
        if (pos >= input.length()) {
            throw std::invalid_argument("Array length exceeds input");
        }
        
        if (input[pos] == '$') {
            result.push_back(parse_bulk_string(input, pos));
        } else if (input[pos] == ':') {
            result.push_back(std::to_string(parse_integer(input, pos)));
        } else if (input[pos] == '+') {
            // Simple string
            pos++; // Skip '+'
            size_t end = input.find("\r\n", pos);
            if (end == std::string::npos) {
                throw std::invalid_argument("Invalid simple string format");
            }
            result.push_back(input.substr(pos, end - pos));
            pos = end + 2; // Skip "\r\n"
        } else {
            throw std::invalid_argument("Unsupported array element type");
        }
    }
    
    return result;
}
