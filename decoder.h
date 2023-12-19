// Include declaration ----------------------------------------------------
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <zlib.h>
#include <SDL.h>  
// ------------------------------------------------------------------------ 

// Struct declaration -----------------------------------------------------
#ifndef SETSTRUCT_H
#define SETSTRUCT_H

typedef struct chunks
{
    Byte* chunk_data;           // 8 bytes data pointer
    uint32_t chunk_length;      // 4 bytes uint *
    uint32_t chunk_crc;         // 4 bytes signed integer *
    char chunk_type[5];         // Array 5 bytes
}chunks;

typedef struct IHDRchunk{
    uint32_t width;             // 4 bytes uint *
    uint32_t height;            // 4 bytes uint *
    int8_t bitd;                // 1 byte int
    int8_t colort;              // 1 byte int
    int8_t compm;               // 1 byte int
    int8_t filterm;             // 1 byte int
    int8_t interlacem;          // 1 byte int
} IHDRchunk;
// * (big endian pay attention! must check architecture, in case little reverse byte)
#endif
// ------------------------------------------------------------------------ 

// Function declaration ---------------------------------------------------
uint32_t reverse_endian(uint32_t value);

int PaethPredictor(int a, int b, int c);

unsigned char Recon_a(int r, int c, int stride, int bytesPerPixel, unsigned char *buffer);

unsigned char Recon_b(int r, int c, int stride, unsigned char *buffer) ;

unsigned char Recon_c(int r, int c, int stride, int bytesPerPixel, unsigned char *buffer); 

void get_array_buffer(unsigned char *IDAT_data, unsigned char *buffer, int width, int height); 

Byte* concatenate_data(chunks* my_chunks, size_t num_chunks, size_t* concatenated_size);

int get_chunks(FILE* file, chunks **my_chunks, size_t *counter_IDAT, size_t *counter_CHUNKS);
// ------------------------------------------------------------------------ 
