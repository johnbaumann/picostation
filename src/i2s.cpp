#include "i2s.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <array>

#include "cmd.h"
#include "disc_image.h"
#include "drive_mechanics.h"
#include "f_util.h"
#include "ff.h"
#include "filesystem.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "hw_config.h"
#include "logging.h"
#include "main.pio.h"
#include "modchip.h"
#include "pico/stdlib.h"
#include "picostation.h"
#include "pseudo_atomics.h"
#include "subq.h"
#include "values.h"

#if DEBUG_I2S
#define DEBUG_PRINT(...) printf(__VA_ARGS__)
#else
#define DEBUG_PRINT(...) while (0)
#endif

//const size_t c_fileNameLength = 255;

TCHAR target_Cues[255][c_fileNameLength];

pseudoatomic<int> g_imageIndex;  // To-do: Implement a console side menu to select the cue file
pseudoatomic<int> g_listingMode;

picostation::DiscImage::DataLocation s_dataLocation = picostation::DiscImage::DataLocation::RAM;
static FATFS s_fatFS;

constexpr std::array<uint16_t, 1176> picostation::I2S::generateScramblingLUT() {
    std::array<uint16_t, 1176> cdScramblingLUT = {0};
    int shift = 1;

    for (size_t i = 6; i < 1176; i++) {
        uint8_t upper = shift & 0xFF;
        for (size_t j = 0; j < 8; j++) {
            unsigned bit = ((shift & 1) ^ ((shift & 2) >> 1)) << 15;
            shift = (bit | shift) >> 1;
        }

        uint8_t lower = shift & 0xFF;

        cdScramblingLUT[i] = (lower << 8) | upper;

        for (size_t j = 0; j < 8; j++) {
            unsigned bit = ((shift & 1) ^ ((shift & 2) >> 1)) << 15;
            shift = (bit | shift) >> 1;
        }
    }

    return cdScramblingLUT;
}

void picostation::I2S::mountSDCard() {
    FRESULT fr = f_mount(&s_fatFS, "", 1);
    if (FR_OK != fr) {
        panic("f_mount error: %s (%d)\n", FRESULT_str(fr), fr);
    }
}
const unsigned int c_userDataSize = 2324;
uint8_t directoryListing[2352] = {0};
uint8_t directoryBuffer[2352] = {0};
uint8_t filteredCues[2352] = {0};
picostation::FileSystem fileSystem;

#define MAX_LINES 2000 
#define MAX_LENGTH 255

void parseLines(char *dataBuffer,char *filteredCues, TCHAR lines[MAX_LINES][MAX_LENGTH], int *lineCount) {
    if (!dataBuffer) {
        return;
    }

    *lineCount = 0;  
    char *start = dataBuffer;
    char *end = dataBuffer;

    while (*start != '\0' && *lineCount < MAX_LINES) {
        // Find the end of the line
        while (*end != '\n' && *end != '\0') {
            end++;
        }

        // Calculate length
        int length = end - start;
        if (length >= MAX_LENGTH) {
            length = MAX_LENGTH - 1;
        }
        char cueBuffer[5] = "euc.";
        

            char temp[MAX_LENGTH];
            for (int i = 0; i < length; i++) {
                temp[i] = start[length - 1 - i];
            }
            temp[length] = '\0';
        if (temp[0] == cueBuffer[0] && temp[1] == cueBuffer[1] && temp[2] == cueBuffer[2] && temp[3] == cueBuffer[3]){
            
            for (int i = 0; i < length; i++) {
                lines[*lineCount+1][i] = start[i];

            }
            strncat(filteredCues, start, length);
            strcat(filteredCues, "\n");

            lines[*lineCount+1][length] = '\0'; // Null-terminator add
            printf("cue dosyası!= %s\n", lines[*lineCount+1]);
            (*lineCount)++;
        }
        

        

        // Next line 
        if (*end == '\n') {
            end++;
        }
        start = end;
    }
    printf("filteredCues!=\n %s\n", filteredCues);
} 

int picostation::I2S::initDMA(const volatile void *read_addr, unsigned int transfer_count) {
    int channel = dma_claim_unused_channel(true);
    dma_channel_config c = dma_channel_get_default_config(channel);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    const unsigned int i2sDREQ = PIOInstance::I2S_DATA == pio0 ? DREQ_PIO0_TX0 : DREQ_PIO1_TX0;
    channel_config_set_dreq(&c, i2sDREQ);
    dma_channel_configure(channel, &c, &PIOInstance::I2S_DATA->txf[SM::I2S_DATA], read_addr, transfer_count, false);

    return channel;
}

[[noreturn]] void __time_critical_func(picostation::I2S::start)(MechCommand &mechCommand) {
    picostation::ModChip modChip;

    static constexpr size_t c_sectorCacheSize = 50;
    int cachedSectors[c_sectorCacheSize];
    int roundRobinCacheIndex = 0;
    static uint16_t cdSamples[c_sectorCacheSize][c_cdSamplesBytes / sizeof(uint16_t)];  // Make static to move off stack
    static uint32_t pioSamples[2][(c_cdSamplesBytes * 2) / sizeof(uint32_t)];
    static constexpr auto cdScramblingLUT = generateScramblingLUT();

    int bufferForDMA = 1;
    int bufferForSDRead = 0;
    int loadedSector[2];
    int currentSector = -1;
    m_sectorSending = -1;
    int loadedImageIndex = -1;
    int filesinDir = 0;

    g_imageIndex = 0;
    g_listingMode = 0;

    mountSDCard();

    int dmaChannel = initDMA(pioSamples[0], c_cdSamplesSize * 2);

    g_coreReady[1] = true;          // Core 1 is ready
    while (!g_coreReady[0].Load())  // Wait for Core 0 to be ready
    {
        tight_loop_contents();
    }

    modChip.init();

#if DEBUG_I2S
    uint64_t startTime = time_us_64();
    uint64_t endTime;
    uint64_t totalTime = 0;
    uint64_t shortestTime = UINT64_MAX;
    uint64_t longestTime = 0;
    unsigned sectorCount = 0;
    unsigned cacheHitCount = 0;
#endif
    fileSystem.readDirectoryToBuffer(directoryListing, "/", 0, c_userDataSize);
    printf("directorylisting:\n%s", directoryListing);

    char lines[MAX_LINES][MAX_LENGTH];
    int lineCount = 0;
    parseLines((char *)directoryListing, (char *)filteredCues, target_Cues, &lineCount);


    int firstboot = 1;
    while (true) {
        // Update latching, output SENS

        // Sector could change during the loop, so we need to keep track of it
        currentSector = g_driveMechanics.getSector();

        modChip.sendLicenseString(currentSector, mechCommand);

        // Load the disc image if it has changed
        const int imageIndex = g_imageIndex.Load();

        // Hacky load image from target data location
        
        if (loadedImageIndex != imageIndex) {
            if(firstboot == 1){
                printf("first boot!\n");
                firstboot = 0;
            } else {
                printf("change to SD!\n");
                s_dataLocation = picostation::DiscImage::DataLocation::SDCard;
            }
            printf("image changed! %d\n",loadedImageIndex);
            if (s_dataLocation == picostation::DiscImage::DataLocation::SDCard) {
                g_discImage.load(target_Cues[imageIndex]);
                printf("get from SD!\n");
            } else if (s_dataLocation == picostation::DiscImage::DataLocation::RAM) {
                g_discImage.makeDummyCue();
                printf("get from ram!\n");
                
            }

            loadedImageIndex = imageIndex;

            // Reset cache and loaded sectors
            loadedSector[0] = -1;
            loadedSector[1] = -1;
            roundRobinCacheIndex = 0;
            bufferForDMA = 1;
            bufferForSDRead = 0;
            memset(cachedSectors, -1, sizeof(cachedSectors));
            memset(cdSamples, 0, sizeof(cdSamples));
            memset(pioSamples, 0, sizeof(pioSamples));
        }

        // Data sent via DMA, load the next sector
        if (bufferForDMA != bufferForSDRead) {
#if DEBUG_I2S
            startTime = time_us_64();
#endif

            // Load the next sector
            // Sector cache lookup/update
            int cache_hit = -1;
            for (size_t i = 0; i < c_sectorCacheSize; i++) {
                if (cachedSectors[i] == currentSector) {
                    cache_hit = i;
#if DEBUG_I2S
                    cacheHitCount++;
#endif
                    break;
                }
            }
            

            if (cache_hit == -1) {
                g_discImage.readSector(cdSamples[roundRobinCacheIndex], currentSector - c_leadIn, s_dataLocation);
                cachedSectors[roundRobinCacheIndex] = currentSector;
                cache_hit = roundRobinCacheIndex;
                roundRobinCacheIndex = (roundRobinCacheIndex + 1) % c_sectorCacheSize;
            }


                        // Copy CD samples to PIO buffer
                if((currentSector - c_leadIn - c_preGap == 100) && g_listingMode.Load() == 1){
                    g_listingMode = 0;

                    g_discImage.buildSector(currentSector-c_leadIn,directoryBuffer ,filteredCues);
                    printf("Sector 100 load\n");
                    printf("currentSector: %i\n", currentSector);
                    printf("c_leadin: %i\n", c_leadIn);
                    printf("c_preGap: %i\n", c_preGap);
                    printf("c_sectorCacheSize: %i\n",c_sectorCacheSize);
                    //printf("%.*s\n", 2324, (char*)(directoryBuffer + 24));

                    memcpy(&cdSamples[cache_hit], &directoryBuffer, 2352);
                }
                int16_t const *sectorData = reinterpret_cast<int16_t *>(cdSamples[cache_hit]);

            // Copy CD samples to PIO buffer
            for (size_t i = 0; i < c_cdSamplesSize * 2; i++) {
                uint32_t i2sData;

                if (g_discImage.isCurrentTrackData()) {
                    // Scramble the data
                    i2sData = (sectorData[i] ^ cdScramblingLUT[i]) << 8;
                } else {
                    // Audio track, just copy the data
                    i2sData = (sectorData[i]) << 8;
                }

                if (i2sData & 0x100) {
                    i2sData |= 0xFF;
                }

                pioSamples[bufferForSDRead][i] = i2sData;
            }

#if DEBUG_I2S
            loadedSector[bufferForSDRead] = currentSector;
            bufferForSDRead = (bufferForSDRead + 1) % 2;
            endTime = time_us_64();
            totalTime = endTime - startTime;
            if (totalTime < shortestTime) {
                shortestTime = totalTime;
            }
            if (totalTime > longestTime) {
                longestTime = totalTime;
            }
            sectorCount++;
#endif
        }

        // Start the next transfer if the DMA channel is not busy
        if (!dma_channel_is_busy(dmaChannel)) {
            bufferForDMA = (bufferForDMA + 1) % 2;
            m_sectorSending = loadedSector[bufferForDMA];
            m_lastSectorTime = time_us_64();

            dma_hw->ch[dmaChannel].read_addr = (uint32_t)pioSamples[bufferForDMA];

            // Sync with the I2S clock
            while (gpio_get(Pin::LRCK) == 1) {
                tight_loop_contents();
            }
            while (gpio_get(Pin::LRCK) == 0) {
                tight_loop_contents();
            }

            dma_channel_start(dmaChannel);
        }

#if DEBUG_I2S
        if (sectorCount >= 100) {
            DEBUG_PRINT("min: %lluus, max: %lluus cache hits: %u/%u\n", shortestTime, longestTime, cacheHitCount,
                        sectorCount);
            sectorCount = 0;
            shortestTime = UINT64_MAX;
            longestTime = 0;
            cacheHitCount = 0;
        }
#endif
    }
    __builtin_unreachable();
}
