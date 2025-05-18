# Compiler
CC = gcc

# Compiler flags
CFLAGS = -Wall -g

# Libraries to link against
LIBS = -lcurl -lpthread -lz

# Subdirectory containing additional source files
SUBDIR = ./png_util2

# Source files
SRCS = paster.c ./catpng2.c $(SUBDIR)/lab_png.c $(SUBDIR)/crc.c $(SUBDIR)/zutil.c

OBJS = $(SRCS:.c=.o)

# Output executable
TARGET = paster

# Rule to build the final executable
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LIBS)

# Rule to build object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@


all: $(TARGET) $(SUBDIR)/lab_png.o $(SUBDIR)/crc.o $(SUBDIR)/zutil.o

# Clean up object files and the executable
.PHONY: clean
clean:
	rm -f $(TARGET) $(OBJS) $(SUBDIR_OBJS)
