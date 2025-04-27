# Compiler and flags
CC = g++
CFLAGS = -Wall -Wextra -std=c++17

# Source and target
TARGET = lained
SRC = lained.cpp

# Default target
all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET).exe $(SRC)

# New run target: auto execute and then delete the binary
run: $(TARGET)
	$(TARGET).exe
	del $(TARGET).exe

# Clean generated files
clean:
	del $(TARGET).exe

.PHONY: all run clean
