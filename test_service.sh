#!/bin/bash

echo "Redis Streams Service Test Script"
echo "================================="

# Function to test a Redis command
test_command() {
    echo -n "Testing: $1 ... "
    result=$(echo "$1" | nc localhost 6379 2>/dev/null)
    if [ $? -eq 0 ]; then
        echo "✓ Success"
    else
        echo "✗ Failed"
    fi
}

# Check if the service is running
if ! nc -z localhost 6379 2>/dev/null; then
    echo "Error: Redis Streams Service is not running on localhost:6379"
    echo "Please start the service first: cd build && ./redis_streams_service"
    exit 1
fi

echo "Service is running. Starting tests..."
echo

# Basic connectivity
test_command "PING"

# Basic stream operations
echo "Testing basic stream operations..."
test_command "XADD teststream * field1 value1 field2 value2"
test_command "XADD teststream * temperature 25.5 humidity 60"
test_command "XLEN teststream"
test_command "XRANGE teststream - +"

# Consumer group operations
echo "Testing consumer group operations..."
test_command "XGROUP CREATE teststream testgroup \$"
test_command "XREADGROUP GROUP testgroup consumer1 STREAMS teststream >"

# Test reading from stream
echo "Testing stream reading..."
test_command "XREAD STREAMS teststream 0-0"

echo
echo "Test script completed."
echo "For interactive testing, use: redis-cli -p 6379"
