#include "./png_util2/lab_png.h"
#include "./png_util2/zutil.h"  
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <arpa/inet.h>

typedef struct recv_buf2 {
    char *buf;       /* memory to hold a copy of received data */
    size_t size;     /* size of valid data in buf in bytes*/
    size_t max_size; /* max capacity of buf in bytes*/
    int seq;         /* >=0 sequence number extracted from http header */
                     /* <0 indicates an invalid seq number */
} RECV_BUF;

int catpng(int num_images, RECV_BUF **fragments)
{
    U32 width = 0, new_height = 0;

    for (int i = 0; i < 50; i++) {
        U8 *image_buffer = fragments[i]->buf;
        size_t image_size = fragments[i]->size;

        // // Check if the buffer is a PNG
        // if (!is_png(image_buffer)) {
        //     perror("Not a PNG");
        //     return -1;
        // }

        // Get the width and height of the image
        struct data_IHDR curr_ihdr;
        if (get_png_data_IHDR(&curr_ihdr, image_buffer, 8) != 0) {
            perror("Error getting IHDR chunk data");
            return -1;
        }

        // Assign width
        if (i == 0) {
            width = get_png_width(&curr_ihdr);
        }
        new_height += (U32)get_png_height(&curr_ihdr);
    }
    
    // Buffers for IDAT data
    U8* inf_IDAT_buffer = malloc(new_height * (width*4 + 1)); 
    U8* def_IDAT_buffer = malloc(new_height * (width*4 + 1)); 
    U64 inf_IDAT_buffer_length = 0;
    U64 def_IDAT_buffer_length = 0;

    for (int i = 0; i < 50; i++) {
        U8 *image_buffer = fragments[i]->buf;
        size_t image_size = fragments[i]->size;
        // printf("index: %d, seq: \n", i, fragments[i]->seq);

        // Get the current image
        simple_PNG_p curr = mallocPNG();
        if (curr == NULL) {
            perror("Error allocating memory");
            free(inf_IDAT_buffer);
            free(def_IDAT_buffer);
            return -1;
        }

        // Extract image chunks
        if (get_png_chunks(curr, image_buffer, 8) != 0) {
            perror("Error getting PNG chunks\n");
            free(inf_IDAT_buffer);
            free(def_IDAT_buffer);
            free_png(curr);
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
            return -1;
        }
        inf_IDAT_buffer_length += curr_inf_IDAT_buffer_length;

        // free memory
        free_png(curr);
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

    // free memory
    free(inf_IDAT_buffer);
    free(def_IDAT_buffer);
    free_chunk(ans.p_IHDR);
    free_chunk(ans.p_IDAT);
    free(ans.p_IEND);
    return 0;
}
