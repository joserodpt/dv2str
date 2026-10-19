CXX ?= c++
CXXFLAGS ?= -std=c++17 -Wall -Wextra

TARGET := dv2str
SOURCES := main.cpp

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $@

run: $(TARGET)
	./$(TARGET) sample/DVsample.23-09-06_17-22-00.avi

clean:
	rm -f $(TARGET)