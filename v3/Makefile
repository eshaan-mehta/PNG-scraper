# Compiler
CC = gcc

# Compiler flags
CFLAGS = -Wall -Wextra -std=c11 -O2

# Libraries to link against
LIBS = -lcurl -lpthread -lz

# Subdirectory containing additional source files
SUBDIR = ./png_util3

# Source files
SRCS = paster2.c $(SUBDIR)/lab_png.c $(SUBDIR)/crc.c $(SUBDIR)/zutil.c

# Object files
OBJS = $(SRCS:.c=.o)

# Executable name
TARGET = paster2

# Default target
all: $(TARGET)

# Rule to build the executable
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

# Rule to build object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up build files
clean:
	rm -f $(OBJS) $(TARGET)

# Phony targets
.PHONY: all clean