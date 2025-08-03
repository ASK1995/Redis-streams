#pragma once

#include <string>
#include <vector>
#include <sstream>

class RedisProtocol {
public:
    // RESP (Redis Serialization Protocol) parsing
    static std::vector<std::string> parse_command(const std::string& input);
    
    // RESP formatting
    static std::string format_simple_string(const std::string& str);
    static std::string format_error(const std::string& error);
    static std::string format_integer(int64_t value);
    static std::string format_bulk_string(const std::string& str);
    static std::string format_array(const std::vector<std::string>& elements);
    static std::string format_null_bulk_string();
    static std::string format_null_array();
    
    // Stream-specific formatting
    static std::string format_stream_entries(const std::vector<class StreamEntry>& entries);
    static std::string format_stream_read_response(const std::vector<std::pair<std::string, std::vector<class StreamEntry>>>& stream_entries);
    
private:
    static std::string parse_bulk_string(const std::string& input, size_t& pos);
    static int64_t parse_integer(const std::string& input, size_t& pos);
    static std::vector<std::string> parse_array(const std::string& input, size_t& pos);
};
