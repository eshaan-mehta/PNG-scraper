# PNG Concatenator Version 1
This version has 2 commands: `./findpng` and `./catpng`.

## Overview
The `findpng` command Looks through a given directory and all its subdirectories for PNG image files. The `catpng` program verifies a given list of file paths are PNGs and then vertically concatenates them. Key Features:

- **Signature Validation**: It checks the file type and the first chunk of memory for the PNG signature `89 50 4E 47 0D 0A 1A 0A`
- **Manual PNG Chunk Handling**: PNG files have 3 chunks: `IHDR` for metadata, `IDAT` for the image body, `IEND` for the image crc (checksum). The command manually creates new metadata; decodes, merges, re-encodes the body; and computes the new crc. The final image is saved to `all.png`

## How to run
Please note this has been tested on Unix-based systems. Windows compatibility may vary.

1. CD into `/v1/starter/png_util`.
2. Run the `make` command to ensure everything is compiled for your OS.
3. CD back out into `/v1`
4. Run the `make` command. (Hopefully there are no errors lol)

## Usage
#### findpng
`./findpng <rootdirectory>`

Arguments:
    `<rootdirectory>`    The base directory to search for PNGs. Note this will look through all subdirectories. If you want to use the existing test cases use `./starter/images`.

#### catpng
`./catpng <filepaths...>`

Arguments:
- `<filepaths...>` A space seperated list of exact PNG file paths to concatenate. Need a minimum of 2 paths. (Yes this may be a little tedious)
