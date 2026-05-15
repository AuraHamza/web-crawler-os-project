# Compiler and flags
CXX = g++
# C++17 use karna zaroori hai naye code ke liye
CXXFLAGS = -std=c++17 -Wall -O2
LIBS = -pthread -lcurl

# Target binary name
TARGET = crawler
SRC = crawler_threads.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET) $(LIBS)

# Target to run with 4 threads by default
run: $(TARGET)
	./$(TARGET) 4

# Cleanup purani files ko hatane ke liye
clean:
	rm -f $(TARGET)

.PHONY: all clean run
