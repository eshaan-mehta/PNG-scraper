# PNG Scraper and Concatenator
These are all the versions of my PNG Scaper and Concatenator. I built this to dive into the world of systems programming, which I find really interesting :)

Here is an overview of each of the versions:

## Version 1
The `findpng` command Looks through a given directory and all its subdirectories for PNG image files. The `catpng` program verifies a given list of file paths are PNGs and then vertically concatenates them. Key Features:

- **Signature Validation**: It checks the file type and the first chunk of memory for the PNG signature `89 50 4E 47 0D 0A 1A 0A`
- **Manual PNG Chunk Handling**: PNG files have 3 chunks: `IHDR` for metadata, `IDAT` for the image body, `IEND` for the image crc (checksum). The command manually creates new metadata; decodes, merges, re-encodes the body; and computes the new crc. The final image is saved to `all.png`
