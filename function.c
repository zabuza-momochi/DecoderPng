// Include declaration ----------------------------------------------------
#include "decoder.h"
// ------------------------------------------------------------------------ 

// Function declaration ---------------------------------------------------
uint32_t reverse_endian(uint32_t value)
{
    // Return reversed bytes
    return ((value & 0xFF) << 24) |
           ((value & 0xFF00) << 8) |
           ((value & 0xFF0000) >> 8) |
           ((value & 0xFF000000) >> 24);
}

int PaethPredictor(int a, int b, int c) 
{
    int p = a + b - c;
    int pa = abs(p - a);
    int pb = abs(p - b);
    int pc = abs(p - c);

    if (pa <= pb && pa <= pc) 
    {
        return a;
    } 
    else if (pb <= pc) 
    {
        return b;
    } 
    else 
    {
        return c;
    }
}

unsigned char Recon_a(int r, int c, int stride, int bytesPerPixel, unsigned char *buffer) 
{
    return (c >= bytesPerPixel) ? buffer[r * stride + c - bytesPerPixel] : 0;
}

unsigned char Recon_b(int r, int c, int stride, unsigned char *buffer) 
{
    return (r > 0) ? buffer[(r - 1) * stride + c] : 0;
}

unsigned char Recon_c(int r, int c, int stride, int bytesPerPixel, unsigned char *buffer) 
{
    return (r > 0 && c >= bytesPerPixel) ? buffer[(r - 1) * stride + c - bytesPerPixel] : 0;
}

void get_array_buffer(unsigned char *IDAT_data, unsigned char *buffer, int width, int height) 
{
    int bytesPerPixel = 4;
    int stride = width * bytesPerPixel;
    int i = 0;

    for (int r = 0; r < height; r++) 
    {
        unsigned char filter_type = IDAT_data[i++];

        for (int c = 0; c < stride; c++) 
        {
            unsigned char Filt_x = IDAT_data[i++];
            unsigned char Recon_x;

            if (filter_type == 0) 
            {  // None
                Recon_x = Filt_x;
            } 
            else if (filter_type == 1) 
            {  // Sub
                Recon_x = Filt_x + Recon_a(r, c, stride, bytesPerPixel, buffer);
            } else if (filter_type == 2) 
            {  // Up
                Recon_x = Filt_x + Recon_b(r, c, stride, buffer);
            } 
            else if (filter_type == 3) 
            {  // Average
                Recon_x = Filt_x + (Recon_a(r, c, stride, bytesPerPixel, buffer) + Recon_b(r, c, stride, buffer)) / 2;
            } 
            else if (filter_type == 4) 
            {  // Paeth
                Recon_x = Filt_x + PaethPredictor(Recon_a(r, c, stride, bytesPerPixel, buffer),
                                                  Recon_b(r, c, stride, buffer),
                                                  Recon_c(r, c, stride, bytesPerPixel, buffer));
            } 
            else 
            {
                printf("Unknown filter type: %d\n", filter_type);
                return;
            }

            buffer[r * stride + c] = Recon_x & 0xFF;  // Truncation to byte
        }
    }
}

Byte* concatenate_data(chunks* my_chunks, size_t num_chunks, size_t* concatenated_size) 
{
    // IDAT bytes size
    size_t total_IDAT_size = 0;

    for (size_t i = 0; i < num_chunks; ++i) 
    {
        if (strcmp(my_chunks[i].chunk_type, "IDAT") == 0) 
        {
            total_IDAT_size += my_chunks[i].chunk_length;
        }
    }

    // Allocate memory
    Byte* concatenated_data = (Byte*)malloc(total_IDAT_size);

    if (concatenated_data == NULL) 
    {
        printf("Failed to allocate memory for concatenated data\n");

        // Free unused memory
        free(my_chunks);

        return NULL;
    }

    // Concatenate data bytes
    size_t offset = 0;

    for (size_t i = 0; i < num_chunks; ++i) 
    {
        if (strcmp(my_chunks[i].chunk_type, "IDAT") == 0) 
        {
            memcpy(concatenated_data + offset, my_chunks[i].chunk_data, my_chunks[i].chunk_length);
            offset += my_chunks[i].chunk_length;
        }
    }

    // Set IDAT bytes size
    *concatenated_size = total_IDAT_size;

    return concatenated_data;
}

int get_chunks(FILE* file, chunks **my_chunks, size_t *counter_IDAT, size_t *counter_CHUNKS)
{
    // Index of chunks
    size_t index = 0;

    // Initialize memory
    *my_chunks = NULL;

    // Set counters
    *counter_CHUNKS = 0;
    *counter_IDAT = 0;

    while (1)
    {
        // Reallocate memory for a new chunk
        *my_chunks = realloc(*my_chunks, (index + 1) * sizeof(chunks));

        if (*my_chunks == NULL)
        {
            printf("Failed to allocate memory for chunks\n");
            
            // Free unused memory
            free(*my_chunks);
            
            return -1;
        }

        // CHUNK LENGTH ----------------------------------------------------------------------------------------
        // -----------------------------------------------------------------------------------------------------
        if (fread(&((*my_chunks + index)->chunk_length), sizeof(uint32_t), 1, file) != 1) {

            printf("Failed to read chunk length\n");

            // Free unused memory
            free(*my_chunks);

            return -1;
        }

        (*my_chunks + index)->chunk_length = reverse_endian((*my_chunks + index)->chunk_length);

        printf("---------------------------------\n");
        printf("CHUNK LENGTH: %u\n", (*my_chunks + index)->chunk_length);
        // -----------------------------------------------------------------------------------------------------
        // -----------------------------------------------------------------------------------------------------

        // CHUNK TYPE ------------------------------------------------------------------------------------------
        // -----------------------------------------------------------------------------------------------------
        if (fread((*my_chunks + index)->chunk_type, sizeof(char), 4, file) != 4) {

            printf("Failed to read chunk type\n");

            // Free unused memory
            free(*my_chunks);

            return -1;
        }

        // Adjust the size based on the expected length of the uint32_t
        (*my_chunks + index)->chunk_type[4] = '\0';

        printf("CHUNK TYPE: %s\n", (*my_chunks + index)->chunk_type);
        // -----------------------------------------------------------------------------------------------------
        // -----------------------------------------------------------------------------------------------------

        // CHUNK DATA ------------------------------------------------------------------------------------------
        // -----------------------------------------------------------------------------------------------------
        (*my_chunks + index)->chunk_data = (Byte*)malloc((*my_chunks + index)->chunk_length);

        if ((*my_chunks + index)->chunk_data == NULL)
        {
            printf("Failed to allocate memory for chunk data\n");

            // Free unused memory
            free(*my_chunks);

            return -1;
        }

        if (fread((*my_chunks + index)->chunk_data, sizeof(Byte), (*my_chunks + index)->chunk_length, file) !=
            (*my_chunks + index)->chunk_length)
        {
            printf("Failed to read chunk data\n");

            // Free unused memory
            free(*my_chunks);

            return -1;
        }

        // Get size
        Byte *start_ptr = (*my_chunks + index)->chunk_data;
        Byte *end_ptr = start_ptr + (*my_chunks + index)->chunk_length;
        size_t size_in_bytes = end_ptr - start_ptr;

        printf("CHUNK DATA: %zu\n", size_in_bytes);

        // -----------------------------------------------------------------------------------------------------
        // -----------------------------------------------------------------------------------------------------

        // CHUNK CRC -------------------------------------------------------------------------------------------
        // -----------------------------------------------------------------------------------------------------
        uint32_t checksum = crc32(0L, Z_NULL, 0);

        checksum = crc32(checksum, (const Byte*)((*my_chunks + index)->chunk_type), 4);
        checksum = crc32(checksum, (const Byte*)((*my_chunks + index)->chunk_data), (*my_chunks + index)->chunk_length);

        if (fread(&((*my_chunks + index)->chunk_crc), sizeof(uint32_t), 1, file) != 1)
        {
            printf("Failed to read chunk CRC\n");

            // Free unused memory
            free(*my_chunks);

            return -1;
        }

        (*my_chunks + index)->chunk_crc = reverse_endian((*my_chunks + index)->chunk_crc);

        printf("CHUNCK CRC: %02X\n", (*my_chunks + index)->chunk_crc);

        if ((*my_chunks + index)->chunk_crc != checksum)
        {
            printf("Chunk checksum failed: %u != %u\n", (*my_chunks + index)->chunk_crc, checksum);

            // Free unused memory
            free(*my_chunks);

            return -1;
        }
        // -----------------------------------------------------------------------------------------------------
        // -----------------------------------------------------------------------------------------------------

        // Update total chunks
        (*counter_CHUNKS)++;  

        if (strcmp((*my_chunks + index)->chunk_type, "IEND") == 0)
        {
            printf("---------------------------------\n");
            return 0;
        }

        if (strcmp((*my_chunks + index)->chunk_type, "IDAT") == 0)
        {
            // Update total IDAT chunk
            (*counter_IDAT)++;  
        }

        // Update counter to next chunk
        index++;
    }

    printf("---------------------------------\n");
}
// ------------------------------------------------------------------------ 