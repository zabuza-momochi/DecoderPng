// Include declaration ----------------------------------------------------
#include "decoder.h"
// ------------------------------------------------------------------------ 

// Define declaration -----------------------------------------------------
#define WINDOW_W 512
#define WINDOW_H 512
#define IMG_W 512
#define IMG_H 512 
// ------------------------------------------------------------------------ 

// Var declaration --------------------------------------------------------
unsigned char *buffer;
// ------------------------------------------------------------------------ 

// Entry point ------------------------------------------------------------
int main(int argc, char* args[])
{
    // Specify path of PNG file
    const char* filePath = "basn6a08.png";

    printf("---------------------------------\n");

    // Open file in binary mode using fopen_s
    FILE* file;

    if (fopen_s(&file, filePath, "rb") != 0) 
    {
        printf("Failed to open file: %s\n", filePath);
        SDL_Quit();

        return -1;
    }

    // Read first 8 bytes
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

    // Check if the header matches the PNG signature
    if (memcmp(header, pngSign, 8) == 0) 
    {
        printf("File is a PNG with correct signature!\n");
    } 
    else 
    {
        printf("File is not a PNG or has an incorrect signature.\n");
        return -1;
    }

    // Reset file pointer to the beginning of the file
    //fseek(file, 0, SEEK_SET);

    // Init  chunks
    chunks *my_chunks;
    size_t counter_IDAT;
    size_t counter_CHUNKS;

    // Read chunks from PNG and fill chunks array

    if(get_chunks(file, &my_chunks, &counter_IDAT, &counter_CHUNKS))
    {
        printf("Failed to fill chunk array!\n");
        return -1;
    }
    
    // Close the file
    fclose(file);
    
    printf("TOTAL PNG CHUNKS: %llu\n",counter_CHUNKS);
    printf("TOTAL IDAT CHUNKS: %llu\n", counter_IDAT);
    printf("---------------------------------\n");

    // Init IHDR chunk
    IHDRchunk IHDR_data;

    // Check if correct chunk
    if (strcmp(my_chunks[0].chunk_type, "IHDR") == 0)
    {
        // Copy from chunk to struct IHDR
        memcpy(&IHDR_data, my_chunks[0].chunk_data, sizeof(IHDRchunk));

        // Reverse byte
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
            return -1;
        }
    }
    else
    {
        printf("Failed to read IHDR chunk!\n");
        return -1;
    }
  
    // Collect all IDAT chunks to single buffer
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
    else
    {
        printf("Failed to concatenate IDAT chunks\n");

        // Free unused memory
        free(my_chunks);
        free(concatenated_data);
    }

    // Deflate IDAT
    unsigned long uncompressed_size = concatenated_size * 100;
    unsigned char* IDAT_data = malloc(uncompressed_size);

    // Get uncompressed data
    int result = uncompress(IDAT_data, &uncompressed_size, concatenated_data, concatenated_size);
    
    if (result != Z_OK)
    {
        printf("Failed to decompress data: error %d\n", result);

        // Free unused memory
        free(my_chunks); 
        free(IDAT_data);
        free(concatenated_data);

        return -1;
    }

    // Free unused memory
    free(concatenated_data);

    printf("---------------------------------\n");
    printf("IDAT SIZE DECOMPRESSED: %lu (BYTES)\n", uncompressed_size); 

    // Print first 8 byte info
    printf("IDAT DATA: ");
    for (size_t i = 0; i < 8; ++i) 
    {
        printf("%02X", IDAT_data[i]);
    }
    printf("[...]\n");
    printf("---------------------------------\n");

    // Process image
    printf ("START PROCESS IMAGE\n");

    // Calculate stride and allocate memory for array
    int bytesPerPixel = 4;
    int stride = IHDR_data.width * bytesPerPixel;
    buffer = (unsigned char *)malloc(IHDR_data.height * stride * sizeof(unsigned char));

    // Get array buffer from uncompressed data
    get_array_buffer(IDAT_data, buffer, IHDR_data.width, IHDR_data.height);

    if (buffer == NULL)
    {
        printf("Failed to get buffer from IDAT\n");

        // Free unused memory
        free(my_chunks); 
        free(IDAT_data);
        free(concatenated_data);
        free(buffer);

        return -1;
    }

    printf("END PROCESS IMAGE\n");
    printf("---------------------------------\n");

    // Free unused memory
    free(IDAT_data);

    // Resize my_chunks to keep only IHDR chunks and free unused memory
    my_chunks = realloc(my_chunks, sizeof(chunks));

    // Init SDL system
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        printf("SDL_Init Error: %s\n", SDL_GetError());
        return -1;
    }

    // Init window
    SDL_Window *window = SDL_CreateWindow("Decoder PNG", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_W, WINDOW_H, 0); 
    if (window == NULL) 
    {
        printf("SDL_CreateWindow Error: %s\n", SDL_GetError());
        SDL_Quit();
        return -1;
    }

    // Init renderer
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

    // Set texture with BLENDMODE (process alpha info)
    if (SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND) != 0) {
        printf("SDL_SetTextureBlendMode Error: %s\n", SDL_GetError());
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    // Update texture with recon buffer
    if (SDL_UpdateTexture(texture, NULL, buffer,  IHDR_data.width * 4) != 0)
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

    // Init texture container
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

        // Clear renderer
        SDL_RenderClear(renderer);

        // Copy texture to the renderer
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
    
    // Free unused memory
    free(buffer);
    free(my_chunks);

    // Close window and quit
    SDL_Quit();

    return 0;
}