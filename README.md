# Redis Streams Service

A C++ implementation of the Redis Streams protocol and API, providing a compatible server for Redis stream operations.

## Features

This service implements the core Redis Streams functionality including:

### Basic Stream Operations
- **XADD** - Add entries to a stream
- **XREAD** - Read entries from streams
- **XRANGE** - Get a range of entries from a stream
- **XLEN** - Get the length of a stream
- **XDEL** - Delete entries from a stream

### Consumer Group Operations
- **XGROUP CREATE** - Create consumer groups
- **XREADGROUP** - Read from streams as part of a consumer group
- **XACK** - Acknowledge processed messages

### Additional Features
- Full RESP (Redis Serialization Protocol) compatibility
- Multi-threaded client handling
- Thread-safe stream operations
- Auto-generated stream IDs
- Consumer group management with pending entry lists (PEL)

## Building the Service

### Prerequisites
- C++17 compatible compiler (GCC 7+, Clang 5+)
- POSIX-compliant system (Linux, macOS)
- Optional: CMake 3.10 or higher (alternative build method)

### Build Instructions

#### Method 1: Using Make (Recommended)
```bash
# Build the service directly
make

# Run the service (default port 6379)
./redis_streams_service

# Or run on a custom port
./redis_streams_service 8080
```

#### Method 2: Using CMake (if available)
```bash
# Install cmake on macOS if needed
make install-deps-mac

# Create build directory
mkdir build
cd build

# Configure with CMake
cmake ..

# Build the service
make

# Run the service
./redis_streams_service
```

#### Clean Build
```bash
# Clean build artifacts
make clean

# Rebuild
make
```

## Usage Examples

### Basic Stream Operations

Connect to the service using any Redis client (like redis-cli):

```bash
# Add entries to a stream
XADD mystream * field1 value1 field2 value2
XADD mystream * temperature 25.5 humidity 60

# Read from the beginning of the stream
XREAD STREAMS mystream 0-0

# Get stream length
XLEN mystream

# Get a range of entries
XRANGE mystream - +

# Delete entries
XDEL mystream 1234567890123-0
```

### Consumer Groups

```bash
# Create a consumer group
XGROUP CREATE mystream mygroup $

# Read as part of a consumer group
XREADGROUP GROUP mygroup consumer1 STREAMS mystream >

# Acknowledge processed messages
XACK mystream mygroup 1234567890123-0
```

## Architecture

The service is built with the following components:

### Core Classes

- **RedisServer** - Main server class handling TCP connections and command routing
- **Stream** - Manages individual stream data and operations
- **StreamEntry** - Represents individual stream entries with ID and field-value pairs
- **StreamID** - Handles stream ID generation, parsing, and comparison
- **ConsumerGroup** - Manages consumer groups and message delivery
- **Consumer** - Represents individual consumers within groups
- **RedisProtocol** - Handles RESP protocol parsing and formatting

### Thread Safety

- All operations are thread-safe using mutexes
- Each client connection is handled in a separate thread
- Stream operations are protected with fine-grained locking

### Stream ID Generation

Stream IDs follow the Redis format: `timestamp-sequence`

- Auto-generation using current system time
- Monotonically increasing sequence numbers
- Support for explicit ID specification
- Validation to ensure IDs are greater than previous entries

## Protocol Compatibility

This service implements the Redis Serialization Protocol (RESP) and is compatible with:

- Standard Redis clients (redis-cli, redis-py, node_redis, etc.)
- Redis connection libraries in various programming languages
- Redis monitoring and management tools

## Limitations

This is a focused implementation of Redis Streams with the following limitations:

- Only stream-related commands are implemented
- No persistence (data is stored in memory only)
- No clustering support
- Limited blocking operations support
- Simplified consumer group management

## Testing the Service

You can test the service using `redis-cli`:

```bash
# Start the service
./redis_streams_service

# In another terminal, connect with redis-cli
redis-cli -p 6379

# Test basic functionality
127.0.0.1:6379> PING
PONG
127.0.0.1:6379> XADD test * hello world
"1234567890123-0"
127.0.0.1:6379> XREAD STREAMS test 0-0
1) 1) "test"
   2) 1) 1) "1234567890123-0"
         2) 1) "hello"
            2) "world"
```

## License

This project is provided as-is for educational and development purposes.
