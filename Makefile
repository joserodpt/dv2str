CXX ?= c++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2

TARGET := dv2str
SOURCES := main.cpp
DIST := dist

# Cross builds for Linux and Windows use Zig as the C++ compiler
# (`brew install zig`). The macOS build uses the native toolchain.
ZIG ?= zig

.PHONY: all run clean dist macos linux linux-arm windows

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $@

run: $(TARGET)
	./$(TARGET) sample/DVsample.23-09-06_17-22-00.avi

# Build release binaries for every platform into $(DIST)/.
dist: macos linux linux-arm windows
	@echo "Binaries written to $(DIST)/"
	@ls -lh $(DIST)

# Universal macOS binary (Apple Silicon + Intel), built with the system
# compiler. Requires building on macOS.
macos:
	@mkdir -p $(DIST)
	$(CXX) $(CXXFLAGS) -arch arm64 -arch x86_64 $(SOURCES) -o $(DIST)/$(TARGET)-macos-universal

# Linux (statically linked against musl, so no runtime dependencies).
# -s strips debug symbols for smaller release binaries.
linux:
	@mkdir -p $(DIST)
	$(ZIG) c++ -target x86_64-linux-musl $(CXXFLAGS) -s $(SOURCES) -o $(DIST)/$(TARGET)-linux-x86_64

linux-arm:
	@mkdir -p $(DIST)
	$(ZIG) c++ -target aarch64-linux-musl $(CXXFLAGS) -s $(SOURCES) -o $(DIST)/$(TARGET)-linux-arm64

# Windows (64-bit, statically linked).
windows:
	@mkdir -p $(DIST)
	$(ZIG) c++ -target x86_64-windows-gnu $(CXXFLAGS) -s $(SOURCES) -o $(DIST)/$(TARGET)-windows-x86_64.exe

clean:
	rm -f $(TARGET)
	rm -rf $(DIST)
