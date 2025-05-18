#include "./starter/png_util/lab_png.h"
#include "./starter/png_util/zutil.h"  
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>

int catpng(int num_images, char **paths)
{
    U32 width = 0, new_height = 0;
    for (int i = 0; i < num_images; i++) {
        // Open the file in binary mode
        FILE *file = fopen(paths[i], "rb");
        if (file == NULL) {
            perror("Error opening file");
            return -1;
        }

        // Allocate memory for the header buffer
        U8 *header_buffer = malloc(8 * sizeof(U8)); 
        if (header_buffer == NULL) {
            perror("Error allocating memory");
            fclose(file);
            return -1;
        }
        // Check if the file is a PNG file
        fread(header_buffer, 1, 8, file);
        if (!is_png(header_buffer, 8)) {
            perror("Not a PNG file");
            free(header_buffer);
            fclose(file);
            return -1;
        }

        // Get the width and height of the image
        struct data_IHDR curr_ihdr;
        if (get_png_data_IHDR(&curr_ihdr, file, 8, SEEK_SET) != 0) {
            perror("Error getting IHDR chunk data");
            free(header_buffer);
            fclose(file);
            return -1;
        }

        // Check if the images have the same width
        if (i == 0) {
            width = get_png_width(&curr_ihdr);
        } else if (get_png_width(&curr_ihdr) != width) {
            perror("Images have different widths");
            free(header_buffer);
            fclose(file);
            return -1;
        }
        
        new_height += (U32)get_png_height(&curr_ihdr);

        // free memory
        free(header_buffer);
        fclose(file);
    }
    
    // Buffers for IDAT data
    U8* inf_IDAT_buffer = malloc(new_height * (width*4 + 1)); 
    U8* def_IDAT_buffer = malloc(new_height * (width*4 + 1)); 
    U64 inf_IDAT_buffer_length = 0;
    U64 def_IDAT_buffer_length = 0;

    for (int i = 0; i < num_images; i++) {
        // Open the file in binary mode
        FILE *file = fopen(paths[i], "rb");
        if (file == NULL) {
            perror("Error opening file");
            free(inf_IDAT_buffer);
            free(def_IDAT_buffer);
            return -1;
        }

        // Get the current image
        simple_PNG_p curr = mallocPNG();
        if (curr == NULL) {
            perror("Error allocating memory");
            free(inf_IDAT_buffer);
            free(def_IDAT_buffer);
            fclose(file);
            return -1;
        }

        // Extract image chunks
        if (get_png_chunks(curr, file, 8, SEEK_SET) != 0) {
            perror("Error getting PNG chunks\n");
            free(inf_IDAT_buffer);
            free(def_IDAT_buffer);
            free_png(curr);
            fclose(file);
            return -1;
        }

        // expand the current IDAT buffer onto the final buffer
        U64 curr_inf_IDAT_buffer_length = 0;
        int ret = mem_inf(inf_IDAT_buffer + inf_IDAT_buffer_length, &curr_inf_IDAT_buffer_length, curr->p_IDAT->p_data, (U64)curr->p_IDAT->length);
        if (ret != 0) {
            perror("Error inflating IDAT data");
            free(inf_IDAT_buffer);
            free(def_IDAT_buffer);
            free_png(curr);
            fclose(file);
            return -1;
        }
        inf_IDAT_buffer_length += curr_inf_IDAT_buffer_length;

        // free memory
        free_png(curr);
        fclose(file);
    }

    // create new PNG
    struct simple_PNG ans;
    ans.p_IHDR = malloc(sizeof(struct chunk));
    ans.p_IDAT = malloc(sizeof(struct chunk));
    ans.p_IEND = malloc(sizeof(struct chunk));
    if (ans.p_IHDR == NULL || ans.p_IDAT == NULL || ans.p_IEND == NULL) {
        perror("Error allocating memory");
        free(inf_IDAT_buffer);
        free(def_IDAT_buffer);
        free_chunk(ans.p_IHDR);
        free_chunk(ans.p_IDAT);
        free(ans.p_IEND);
        return -1;
    }

    // create new IHDR chunk
    ans.p_IHDR->length = DATA_IHDR_SIZE;
    strncpy(ans.p_IHDR->type, "IHDR", 4);
    ans.p_IHDR->p_data = malloc(DATA_IHDR_SIZE);
    if (ans.p_IHDR->p_data == NULL) {
        perror("Error allocating memory IHDR data memory");
        free(inf_IDAT_buffer);
        free(def_IDAT_buffer);
        free_chunk(ans.p_IHDR);
        free_chunk(ans.p_IDAT);
        free(ans.p_IEND);
        return -1;
    }

    // Populate the IHDR chunk
    ans.p_IHDR->length = DATA_IHDR_SIZE;
    *(U32*)(ans.p_IHDR->p_data) = htonl(width); // width
    *(U32*)(ans.p_IHDR->p_data + 4) = htonl(new_height); // height
    *(ans.p_IHDR->p_data + 8) = 8; // bit depth
    *(ans.p_IHDR->p_data + 9) = 6; // color type
    *(ans.p_IHDR->p_data + 10) = 0; // compression
    *(ans.p_IHDR->p_data + 11) = 0; // filter
    *(ans.p_IHDR->p_data + 12) = 0; // interlace
    ans.p_IHDR->crc = htonl(calculate_chunk_crc(ans.p_IHDR)); 

    strncpy(ans.p_IDAT->type, "IDAT", 4);

    // Deflate IDAT for chunk
    int ret = mem_def(def_IDAT_buffer, &def_IDAT_buffer_length, inf_IDAT_buffer, inf_IDAT_buffer_length, Z_DEFAULT_COMPRESSION);
    if (ret != 0) {
        perror("Error deflating IDAT data");
        free(inf_IDAT_buffer);
        free(def_IDAT_buffer);
        free_chunk(ans.p_IHDR);
        free_chunk(ans.p_IDAT);
        free(ans.p_IEND);
        return -1;
    }

    // Populate the IDAT chunk
    ans.p_IDAT->p_data = malloc(def_IDAT_buffer_length);
    if (ans.p_IDAT->p_data == NULL) {
        perror("Error allocating memory IDAT data memory");
        free(inf_IDAT_buffer);
        free(def_IDAT_buffer);
        free_chunk(ans.p_IHDR);
        free_chunk(ans.p_IDAT);
        free(ans.p_IEND);
        return -1;
    }
    ans.p_IDAT->length = (U32)def_IDAT_buffer_length;
    memcpy(ans.p_IDAT->p_data, def_IDAT_buffer, def_IDAT_buffer_length);
    ans.p_IDAT->crc = htonl(calculate_chunk_crc(ans.p_IDAT)); // Convert CRC to big-endian


    // create new IEND chunk
    ans.p_IEND->length = 0;
    strncpy(ans.p_IEND->type, "IEND", 4);
    ans.p_IEND->crc = htonl(calculate_chunk_crc(ans.p_IEND)); // Convert CRC to big-endian

    write_PNG("all.png", &ans);
    printf("Concatenated strips into all.png\n");

    // free memory
    free(inf_IDAT_buffer);
    free(def_IDAT_buffer);
    free_chunk(ans.p_IHDR);
    free_chunk(ans.p_IDAT);
    free(ans.p_IEND);

}


int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Not enough arguments.\n");
        return -1;
    }
    
    return catpng(argc - 1, argv + 1);
}

