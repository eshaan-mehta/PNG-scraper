
#include <stdio.h>    /* for printf(), perror()...   */
#include <stdlib.h>   /* for malloc()                */
#include <errno.h>    /* for errno                   */
#include "crc.h"      /* for crc()                   */
#include "zutil.h"    /* for mem_def() and mem_inf() */
#include "lab_png.h"  /* simple PNG data structures  */

int is_png(U8 *buf) 
{
    // PNG signature: 89 50 4E 47 0D 0A 1A 0A
    const U8 png_signature[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};

    for (int i = 0; i < 8; i++) {
        if (png_signature[i] != buf[i]) {
            printf("index: %d, data: %02X\n", i, buf[i]);
            return 0;
        }
    }
    
    return 1;
}

// offset should point to the start of the IHDR chunk
int get_png_data_IHDR(struct data_IHDR *out, U8 *image_buffer, long offset)
{
    // offset by additional 8 to skip the length and type fields
    U8 *buffer_position = image_buffer + offset + CHUNK_LEN_SIZE + CHUNK_TYPE_SIZE;

    // copy data into output variable
    memcpy(out, buffer_position, DATA_IHDR_SIZE);

    // convert from big endian to little endian
    out->width = ntohl(out->width);
    out->height = ntohl(out->height);
    return 0;
}

int get_png_height(struct data_IHDR *buf)
{
    return buf->height;
}

int get_png_width(struct data_IHDR *buf)
{
    return buf->width;
}

int get_png_chunks(simple_PNG_p out, U8* image_buffer, long offset) {
    U8 *buffer_position = image_buffer + offset;

    out->p_IHDR = get_chunk(&buffer_position);
    if (!out->p_IHDR) {
        perror("Failed to get IHDR chunk");
        return -1;
    }

    out->p_IDAT = get_chunk(&buffer_position);
    if (!out->p_IDAT) {
        perror("Failed to get IDAT chunk");
        return -1;
    }

    out->p_IEND = get_chunk(&buffer_position);
    if (!out->p_IEND) {
        perror("Failed to get IEND chunk");
        return -1;
    }

    return 0;
}

chunk_p get_chunk(U8 **buffer_position)
{
    //extract from file one chunk and populate a struct chunk, converting field to little endian
    chunk_p out = malloc(sizeof(struct chunk));
    if (out == NULL) {
        perror("Error allocating memory");
        return NULL;
    }

    memcpy(&out->length, *buffer_position, CHUNK_LEN_SIZE);
    *buffer_position += CHUNK_LEN_SIZE;
    out->length = ntohl(out->length);

    memcpy(out->type, *buffer_position, CHUNK_TYPE_SIZE);
    *buffer_position += CHUNK_TYPE_SIZE;
    
    out->p_data = malloc(out->length);
    memcpy(out->p_data, *buffer_position, out->length);
    *buffer_position += out->length;

    memcpy(&out->crc, *buffer_position, CHUNK_CRC_SIZE);
    *buffer_position += CHUNK_CRC_SIZE;
    out->crc = ntohl(out->crc);

    return out;
}

U32 get_chunk_crc(chunk_p in)
{
    return in->crc;
}

U32 calculate_chunk_crc(chunk_p in)
{
    // type and data fields may not be inc continuous memory so make a new buffer with 
    U8 *buf = malloc(CHUNK_TYPE_SIZE + in->length);
    memcpy(buf, in->type, CHUNK_TYPE_SIZE);

    if (in->length > 0 && in->p_data != NULL) {
        memcpy(buf + CHUNK_TYPE_SIZE, in->p_data, in->length);
    }
    
    U32 calculated_crc = crc(buf, CHUNK_TYPE_SIZE + in->length);
    free(buf);
    return calculated_crc;
}

simple_PNG_p mallocPNG()
{
    return malloc(sizeof(struct simple_PNG));
}

void free_png(simple_PNG_p in)
{
    if (in == NULL) {
        return;
    }
    
    free_chunk(in->p_IHDR);
    free_chunk(in->p_IDAT);
    free_chunk(in->p_IEND);
    free(in);
}

void free_chunk(chunk_p in)
{
     //free the memory of a struct chunk and inner data buffers
    free(in->p_data);
    free(in);
}

int write_PNG(char* filepath, simple_PNG_p in)
{
    FILE *fp = fopen(filepath, "wb");
    if (fp == NULL) {
        return -1;
    }

    // add png signature
    U8 png_signature[PNG_SIG_SIZE] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    fwrite(png_signature, sizeof(U8), PNG_SIG_SIZE, fp);

    write_chunk(fp, in->p_IHDR);
    write_chunk(fp, in->p_IDAT);
    write_chunk(fp, in->p_IEND);
    fclose(fp);
    return 0;
}

int write_chunk(FILE* fp, chunk_p in)
{
    U32 network_input_length = htonl(in->length);
    fwrite(&network_input_length, sizeof(U32), 1, fp);
    fwrite(in->type, sizeof(U8), CHUNK_TYPE_SIZE, fp);

    if (in->length > 0 && in->p_data != NULL) {
        fwrite(in->p_data, sizeof(U8), in->length, fp);
    }
    
    fwrite(&in->crc, sizeof(U32), 1, fp);
    return 0;
}
