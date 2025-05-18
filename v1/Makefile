# Compiler and flags
CC = gcc
CFLAGS = -Wall -g

# Executable names
TARGETS = catpng findpng

# Source files for each target
CATPNG_SRCS = catpng.c ./starter/png_util/lab_png.c ./starter/png_util/crc.c ./starter/png_util/zutil.c
CATPNG_OBJS = $(CATPNG_SRCS:.c=.o)

FINDPNG_SRCS = findpng.c ./starter/png_util/lab_png.c ./starter/png_util/crc.c
FINDPNG_OBJS = $(FINDPNG_SRCS:.c=.o)

# Libraries to link
LIBS = -lz

# Default target to build all programs
all: $(TARGETS)

# Rule to build the catpng executable
catpng: $(CATPNG_OBJS)
	$(CC) $(CFLAGS) -o catpng $(CATPNG_OBJS) $(LIBS)

# Rule to build the findpng executable
findpng: $(FINDPNG_OBJS)
	$(CC) $(CFLAGS) -o findpng $(FINDPNG_OBJS) $(LIBS)

# Rule to build object files from source files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean target to remove compiled files
clean:
	rm -f $(CATPNG_OBJS) $(FINDPNG_OBJS) $(TARGETS)

# Phony targets
.PHONY: all clean
