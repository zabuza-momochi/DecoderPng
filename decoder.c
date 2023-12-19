// Include declaration ----------------------------------------------------
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <zlib.h>
#include <SDL.h>  
// ------------------------------------------------------------------------ 

// Define declaration -----------------------------------------------------
#define WINDOW_W 512
#define WINDOW_H 512
#define IMG_W 512
#define IMG_H 512 
// ------------------------------------------------------------------------ 

// Struct declaration 
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
// ------------------------------------------------------------------------ 

// Function declaration ---------------------------------------------------
uint32_t reverse_endian(uint32_t value)
{
    return ((value & 0xFF) << 24) |
           ((value & 0xFF00) << 8) |
           ((value & 0xFF0000) >> 8) |
           ((value & 0xFF000000) >> 24);
}

unsigned char *Recon;  // Assuming Recon is a global variable

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

unsigned char Recon_a(int r, int c, int stride, int bytesPerPixel) 
{
    return (c >= bytesPerPixel) ? Recon[r * stride + c - bytesPerPixel] : 0;
}

unsigned char Recon_b(int r, int c, int stride) 
{
    return (r > 0) ? Recon[(r - 1) * stride + c] : 0;
}

unsigned char Recon_c(int r, int c, int stride, int bytesPerPixel) 
{
    return (r > 0 && c >= bytesPerPixel) ? Recon[(r - 1) * stride + c - bytesPerPixel] : 0;
}

void get_array_buffer(unsigned char *IDAT_data, unsigned char *Recon, int width, int height) 
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
                Recon_x = Filt_x + Recon_a(r, c, stride, bytesPerPixel);
            } else if (filter_type == 2) 
            {  // Up
                Recon_x = Filt_x + Recon_b(r, c, stride);
            } 
            else if (filter_type == 3) 
            {  // Average
                Recon_x = Filt_x + (Recon_a(r, c, stride, bytesPerPixel) + Recon_b(r, c, stride)) / 2;
            } 
            else if (filter_type == 4) 
            {  // Paeth
                Recon_x = Filt_x + PaethPredictor(Recon_a(r, c, stride, bytesPerPixel),
                                                  Recon_b(r, c, stride),
                                                  Recon_c(r, c, stride, bytesPerPixel));
            } 
            else 
            {
                printf("Unknown filter type: %d\n", filter_type);
                exit(1);
            }

            Recon[r * stride + c] = Recon_x & 0xFF;  // Truncation to byte
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

void get_chunks(FILE* file, chunks **my_chunks, size_t *counter_IDAT, size_t *counter_CHUNKS)
{
    // Index of chunks
    size_t index = 0;

    // Initialize memory
    *my_chunks = NULL;
    *counter_CHUNKS = 0;
    *counter_IDAT = 0;

    while (1)
    {
        // Reallocate memory for a new chunk
        *my_chunks = realloc(*my_chunks, (index + 1) * sizeof(chunks));

        if (*my_chunks == NULL)
        {
            printf("Failed to allocate memory for chunks\n");
            return;
        }

        // CHUNK LENGTH ----------------------------------------------------------------------------------------
        // -----------------------------------------------------------------------------------------------------
        if (fread(&((*my_chunks + index)->chunk_length), sizeof(uint32_t), 1, file) != 1) {

            printf("Failed to read chunk length\n");

            // Delete unused memory
            free(*my_chunks);

            // Set null
            *my_chunks = NULL;

            return;
        }

        (*my_chunks + index)->chunk_length = reverse_endian((*my_chunks + index)->chunk_length);

        printf("---------------------------------\n");
        printf("CHUNK LENGTH: %llu\n", (*my_chunks + index)->chunk_length);
        // -----------------------------------------------------------------------------------------------------
        // -----------------------------------------------------------------------------------------------------

        // CHUNK TYPE ------------------------------------------------------------------------------------------
        // -----------------------------------------------------------------------------------------------------
        if (fread((*my_chunks + index)->chunk_type, sizeof(char), 4, file) != 4) {

            printf("Failed to read chunk type\n");

            // Delete unused memory
            free(*my_chunks);

            // Set null
            *my_chunks = NULL;

            return;
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

            return;
        }

        if (fread((*my_chunks + index)->chunk_data, sizeof(Byte), (*my_chunks + index)->chunk_length, file) !=
            (*my_chunks + index)->chunk_length)
        {
            printf("Failed to read chunk data\n");

            // Delete unused memory
            free((*my_chunks + index)->chunk_data);
            free(*my_chunks);

            return;
        }

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

            // Delete unused memory
            free((*my_chunks + index)->chunk_data);
            free(*my_chunks);

            return;
        }

        (*my_chunks + index)->chunk_crc = reverse_endian((*my_chunks + index)->chunk_crc);

        printf("CHUNCK CRC: %02X\n", (*my_chunks + index)->chunk_crc);

        if ((*my_chunks + index)->chunk_crc != checksum)
        {
            printf("Chunk checksum failed: %u != %u\n", (*my_chunks + index)->chunk_crc, checksum);

            // Delete unused memory
            free((*my_chunks + index)->chunk_data);
            free(*my_chunks);
            return;
        }
        // -----------------------------------------------------------------------------------------------------
        // -----------------------------------------------------------------------------------------------------

        (*counter_CHUNKS)++;  // Aggiorna il numero totale di chunk

        if (strcmp((*my_chunks + index)->chunk_type, "IEND") == 0)
        {
            printf("---------------------------------\n");
            return;
        }

        if (strcmp((*my_chunks + index)->chunk_type, "IDAT") == 0)
        {
            (*counter_IDAT)++;  // Aggiorna il numero totale di chunk
        }

        index++;
    }

    printf("---------------------------------\n");
}
// ------------------------------------------------------------------------ 

// Entry point ------------------------------------------------------------
int main(int argc, char* args[])
{
    // Specify the path to your PNG file
    const char* filePath = "basn6a08.png";

    printf("---------------------------------\n");

    // Open the file in binary mode using fopen_s
    FILE* file;

    if (fopen_s(&file, filePath, "rb") != 0) 
    {
        printf("Failed to open file: %s\n", filePath);
        SDL_Quit();

        return 1;
    }

    // Read the first 8 bytes
    char header[8];

    if (fread(header, sizeof(char), sizeof(header), file) != sizeof(header))
    {
        printf("Failed to read file header\n");
        fclose(file);
        SDL_Quit();

        return -1;
    }

    // PNG signature as a pointer
    const char* pngSign = "\x89PNG\r\n\x1a\n";

    // Check if the read header matches the PNG signature
    if (memcmp(header, pngSign, 8) == 0) 
    {
        printf("File is a PNG with correct signature!\n");
    } 
    else 
    {
        printf("File is not a PNG or has an incorrect signature.\n");
    }

    // Reset file pointer to the beginning of the file
    //fseek(file, 0, SEEK_SET);

    // Declare chunks
    chunks *my_chunks;
    int counter_IDAT;
    int counter_CHUNKS;

    get_chunks(file, &my_chunks, &counter_IDAT, &counter_CHUNKS);
    
    // Close the file and quit SDL
    fclose(file);
    
    printf("TOTAL PNG CHUNKS: %llu\n",counter_CHUNKS);
    printf("TOTAL IDAT CHUNKS: %llu\n", counter_IDAT);
    printf("---------------------------------\n");

    //Init IHDR
    IHDRchunk IHDR_data;

    if (strcmp(my_chunks[0].chunk_type, "IHDR") == 0)
    {
        memcpy(&IHDR_data, my_chunks[0].chunk_data, sizeof(IHDRchunk));

        IHDR_data.width = reverse_endian(IHDR_data.width);
        IHDR_data.height = reverse_endian(IHDR_data.height);

        // Print extracted chunk's info
        printf("CHUNK IHDR ----------------------\n");
        printf("Width: %u\n", IHDR_data.width);
        printf("Height: %u\n", IHDR_data.height);
        printf("Bit Depth: %u\n", IHDR_data.bitd);
        printf("Color Type: %u\n", IHDR_data.colort);
        printf("Compression Method: %u\n", IHDR_data.compm);
        printf("Filter Method: %u\n", IHDR_data.filterm);
        printf("Interlace Method: %u\n", IHDR_data.interlacem);
    
        if (IHDR_data.compm != 0 || IHDR_data.filterm != 0 || IHDR_data.colort != 6 || IHDR_data.bitd != 8 || IHDR_data.interlacem != 0)
        {
            printf("PNG format not supported!\n");
        }
    }
  
    // Concatenate IDAT
    size_t concatenated_size;
    Byte* concatenated_data = concatenate_data(my_chunks, counter_CHUNKS, &concatenated_size);

    printf("---------------------------------\n");
    printf("IDAT SIZE COMPRESSED: %llu (BYTES)\n", concatenated_size); 

    if (concatenated_data != NULL) 
    {
        // Print data
        printf("IDAT DATA: ");
        for (size_t i = 0; i < 8; ++i) {
            printf("%02X", concatenated_data[i]);
        }
        printf("[...]\n");
    }

    // Deflate IDAT
    unsigned long uncompressed_size = concatenated_size * 100;
    unsigned char* IDAT_data = malloc(uncompressed_size);

    int result = uncompress(IDAT_data, &uncompressed_size, concatenated_data, concatenated_size);
    
    if (result != Z_OK)
    {
        printf("unable to uncompress: error %d\n", result);

        free(my_chunks); // and other struct insied
        free(IDAT_data);
        free(concatenated_data);
        return -1;
    }

    // Free unused memory
    free(concatenated_data);

    printf("---------------------------------\n");
    printf("IDAT SIZE DECOMPRESSED: %llu (BYTES)\n", uncompressed_size); 

    // Esempio di stampa dei byte
    printf("IDAT DATA: ");
    for (size_t i = 0; i < 8; ++i) 
    {
        printf("%02X", IDAT_data[i]);
    }
    printf("[...]\n");
    printf("---------------------------------\n");

    // Example usage
    printf ("START PROCESS IMAGE\n");

    // Calculate stride and allocate memory for Recon
    int bytesPerPixel = 4;
    int stride = IHDR_data.width * bytesPerPixel;
    Recon = (unsigned char *)malloc(IHDR_data.height * stride * sizeof(unsigned char));

    // Call the processing function
    get_array_buffer(IDAT_data, Recon, IHDR_data.width, IHDR_data.height);
    printf("END PROCESS IMAGE\n");
    printf("---------------------------------\n");

    // Free unused memory
    free(IDAT_data);

    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) != 0) 
    {
        fprintf("SDL_Init Error: %s\n", SDL_GetError());
        return -1;
    }

    // Create a window and renderer
    SDL_Window *window = SDL_CreateWindow("Decoder PNG", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_W, WINDOW_H, 0); 
    if (window == NULL) 
    {
        printf("SDL_CreateWindow Error: %s\n", SDL_GetError());
        SDL_Quit();
        return -1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == NULL) 
    {
        printf("SDL_CreateRenderer Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    // Create a texture from the recon buffer
    SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, IHDR_data.width, IHDR_data.height);
    if (texture == NULL) 
    {
        printf("SDL_CreateTexture Error: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    if (SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND) != 0) {
        printf("SDL_SetTextureBlendMode Error: %s\n", SDL_GetError());
        return -1;
    }

    // Update the texture with the recon buffer
    if (SDL_UpdateTexture(texture, NULL, Recon,  IHDR_data.width * 4) != 0)
    {
        printf("SDL_CreateTexture Error: %s\n", SDL_GetError());
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    // Main loop
    SDL_Event event;
    int quit = 0;

    SDL_Rect dest_container = {0,0, WINDOW_W, WINDOW_H};

    while (!quit) 
    {
        while (SDL_PollEvent(&event)) 
        {
            if (event.type == SDL_QUIT) 
            {
                quit = 1;
            }
        }

        // Clear the renderer
        SDL_RenderClear(renderer);

        // Copy the texture to the renderer
        SDL_RenderCopy(renderer, texture, NULL, &dest_container);

        // Present the renderer
        SDL_RenderPresent(renderer);

        // Delay to control the frame rate (adjust as needed)
        SDL_Delay(16);
    }

    // Clean up resources
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_free(Recon);

    SDL_Quit();

    return 0;
}