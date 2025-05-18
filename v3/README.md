# PNG Concatenator Version 3
This version introduces a multi-process image retrieval and assembly tool: `./paster2`.

## Overview
The `paster2` utility retrieves fragmented PNG images from a web server using a producer-consumer architecture with shared memory. It efficiently assembles image strips in the correct order, and stitches them together to create a complete image. Key features include:

- **Multi-process architecture**: Uses separate producer and consumer processes
- **Producer-Consumer pattern**: Implements a bounded buffer using shared memory
- **Process synchronization**: Uses semaphores and mutexes for coordination
- **Ring buffer implementation**: Efficient shared memory data structure for strip storage
- **Configurable parameters**: Customize buffer size, process counts, and processing delay
- **HTTP data retrieval**: Uses libcurl to fetch image fragments from a remote server
- **PNG assembly**: Reconstructs the complete image from fragments using PNG manipulation utilities

## How to run
Please note this has been tested on Unix-based systems. Windows compatibility may vary.

1. CD into `/v3/png_util3`.
2. Run the `make` command to ensure everything is compiled for your OS.
3. CD back out into `/v3`
4. Run the `make` command.

## Usage
`./paster2 <b> <p> <c> <x> <n>`

Arguments
- `<b>` Bounded Buffer Size. How many image strips can the buffer hold. Range [1,50]
- `<p>` Number of Producer processes. Range [1,20]
- `<c>` Number of Consumer processes. Range [1,20]
- `<x>` Consumer delay. Time in miliseconds after receiving a file, the consumer will start processing it. Range [1,1000]
- `<n>` Image to fetch from the server [1,3]
