# PNG Scraper and Concatenator
These are all the versions of my PNG Scaper and Concatenator. I built this to dive into the world of systems programming, which I find really interesting :)

Here is an overview of each of the versions:

## Version 1
The `findpng` command Looks through a given directory and all its subdirectories for PNG image files. The `catpng` program verifies a given list of file paths are PNGs and then vertically concatenates them. Key Features:

- **Signature Validation**: It checks the file type and the first chunk of memory for the PNG signature `89 50 4E 47 0D 0A 1A 0A`
- **Manual PNG Chunk Handling**: PNG files have 3 chunks: `IHDR` for metadata, `IDAT` for the image body, `IEND` for the image crc (checksum). The command manually creates new metadata; decodes, merges, re-encodes the body; and computes the new crc. The final image is saved to `all.png`

## Version 2
The `paster` utility retrieves fragmented PNG images from a web server, assembles them in the correct order, and stitches them together to create a complete image. Key features include:

- **Multi-threaded downloading**: Uses concurrent threads to efficiently download image fragments
- **Automatic fragment ordering**: Correctly orders image fragments based on sequence numbers
- **Thread synchronization**: Uses mutexes to safely handle shared data across threads
- **HTTP data retrieval**: Uses libcurl to fetch image fragments from a remote server
- **PNG assembly**: Reconstructs the complete image from fragments using PNG manipulation utilities
