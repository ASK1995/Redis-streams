# Simple Makefile for Redis Streams Service (alternative to CMake)

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Iinclude -pthread
TARGET = redis_streams_service
SRCDIR = src
INCDIR = include

SOURCES = $(SRCDIR)/main.cpp \
          $(SRCDIR)/redis_server.cpp \
          $(SRCDIR)/redis_protocol.cpp \
          $(SRCDIR)/stream.cpp \
          $(SRCDIR)/stream_entry.cpp \
          $(SRCDIR)/consumer_group.cpp \
          $(SRCDIR)/consumer.cpp

OBJECTS = $(SOURCES:.cpp=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJECTS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

install-deps-mac:
	@echo "Installing cmake on macOS..."
	@if command -v brew >/dev/null 2>&1; then \
		brew install cmake; \
	else \
		echo "Homebrew not found. Please install Homebrew first:"; \
		echo "  /bin/bash -c \"\$$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)\""; \
		echo "Then run: brew install cmake"; \
	fi

help:
	@echo "Available targets:"
	@echo "  all           - Build the Redis Streams Service"
	@echo "  clean         - Remove build artifacts"
	@echo "  install-deps-mac - Install cmake using Homebrew (macOS)"
	@echo "  help          - Show this help message"
	@echo ""
	@echo "Usage:"
	@echo "  make          # Build the service"
	@echo "  ./$(TARGET)   # Run the service"
