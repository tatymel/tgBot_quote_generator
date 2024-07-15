# Define the compiler
CXX = g++

# Define the compiler flags
CXXFLAGS = -std=c++20 -I/opt/homebrew/opt/boost/include -I/opt/homebrew/opt/openssl@3/include -I/opt/homebrew/opt/opencv/include/opencv4

# Define the linker flags
LDFLAGS = -L/opt/homebrew/opt/boost/lib -L/opt/homebrew/opt/openssl/lib -L/opt/homebrew/opt/opencv/lib

# Define the libraries to link against
LIBS = -ltgbot -lboost_system -lboost_filesystem -lboost_iostreams -lcurl -lssl -lcrypto -lz \
       -lopencv_core -lopencv_imgproc -lopencv_highgui -lopencv_imgcodecs


# Define the source files
SRCS = test.cpp

# Define the output executable
TARGET = main

# The default rule to build the target
all: $(TARGET)

# Rule to build the target
$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRCS) $(LDFLAGS) $(LIBS)

# Clean rule to remove the built files
clean:
	rm -f $(TARGET)
