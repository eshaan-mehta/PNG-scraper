#include <stdio.h>    /* for printf(), perror()...   */
#include <stdlib.h>   /* for malloc()                */
#include <errno.h>    /* for errno                   */
#include "crc.h"      /* for crc()                   */
#include "zutil.h"    /* for mem_def() and mem_inf() */
#include "lab_png.h"  

int main(int argc, char **argv)
{
    if (argc < 2) {
        printf("No file passed.\n");
        return 1;
    }

    FILE *file = fopen(argv[1], "rb"); // Open the file in binary mode
    if (file == NULL) {
        perror("Error opening file");
        return -1;
    }

    simple_PNG_p png = mallocPNG();
    if (png == NULL) {
        printf("Failed to allocate memory for PNG.\n");
        fclose(file);
        return 1;
    }

    // Extract the PNG chunks. Offset of 8 bytes to skip the PNG signature.
    if (get_png_chunks(png, file, 8, SEEK_SET) == -1) {
        printf("Failed to extract PNG chunks.\n");
        fclose(file);
        return 1;
    }

    // Extract the IHDR chunk data
    struct data_IHDR ihdr;
    if (get_png_data_IHDR(&ihdr, file, 8, SEEK_SET) == -1) {
        printf("Failed to extract IHDR chunk data.\n");
        fclose(file);
        return 1;
    }

    printf("Image dimensions: %d x %d\n", get_png_width(&ihdr), get_png_height(&ihdr));

    U32 crc_IHDR = get_chunk_crc(png->p_IHDR);
    U32 calculated_crc_IHDR = calculate_chunk_crc(png->p_IHDR);
    if (crc_IHDR != calculated_crc_IHDR) {
        printf("IHDR chunk CRC error: computed %x, expected %x\n", calculated_crc_IHDR, crc_IHDR);
    } 

    U32 crc_IDAT = get_chunk_crc(png->p_IDAT);
    U32 calculated_crc_IDAT = calculate_chunk_crc(png->p_IDAT);
    if (crc_IDAT != calculated_crc_IDAT) {
        printf("IDAT chunk CRC error: computed %x, expected %x\n", calculated_crc_IDAT, crc_IDAT);
    } 

    U32 crc_IEND = get_chunk_crc(png->p_IEND);
    U32 calculated_crc_IEND = calculate_chunk_crc(png->p_IEND);
    if (crc_IEND != calculated_crc_IEND) {
        printf("IEND chunk CRC error: computed %x, expected %x\n", calculated_crc_IEND, crc_IEND);
    } 

    // Clean up
    free_png(png);  
    fclose(file);
    return 0;
}