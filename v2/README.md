# PNG Concatenator Version 2
This version introduces a new multi-threaded image retrieval and assembly tool: `./paster`.

## Overview
The `paster` utility retrieves fragmented PNG images from a web server, assembles them in the correct order, and stitches them together to create a complete image. Key features include:

- **Multi-threaded downloading**: Uses concurrent threads to efficiently download image fragments
- **Automatic fragment ordering**: Correctly orders image fragments based on sequence numbers
- **Thread synchronization**: Uses mutexes to safely handle shared data across threads
- **HTTP data retrieval**: Uses libcurl to fetch image fragments from a remote server
- **PNG assembly**: Reconstructs the complete image from fragments using PNG manipulation utilities

## How to run
Please note this has been tested on Unix-based systems. Windows compatibility may vary.

1. CD into `/v2/png_util2`.
2. Run the `make` command to ensure everything is compiled for your OS.
3. CD back out into `/v2`
4. Run the `make` command.

## Usage