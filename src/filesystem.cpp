#include "filesystem.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "f_util.h"
#include "ff.h"

#if DEBUG_I2S
#define DEBUG_PRINT(...) printf(__VA_ARGS__)
#else
#define DEBUG_PRINT(...) while (0)
#endif

int getNumberofFileEntries(const char *dir) {
    int count = 0;
    DIR dirObj;
    FILINFO fileInfo;
    FRESULT fr = f_opendir(&dirObj, dir);
    if (FR_OK != fr) {
        DEBUG_PRINT("f_opendir error: %s (%d)\n", FRESULT_str(fr), fr);
        return -1;
    }
    while (f_readdir(&dirObj, &fileInfo) == FR_OK && fileInfo.fname[0]) {
        count++;
    }
    f_closedir(&dirObj);
    return count;
}

void picostation::FileSystem::readDirectoryToBuffer(void *buffer, const char *path, const size_t offset, const unsigned int bufferSize) {
    FRESULT res;
    DIR dir;
    FILINFO fno;

    char *buf_ptr = (char *)buffer;
    int remainingSize = bufferSize;

    res = f_opendir(&dir, path); // Open the directory
    if (res == FR_OK) {
        if (offset > 0) {
            for (int i = 0; i < offset; i++) {
                res = f_readdir(&dir, &fno);
                if (res != FR_OK || fno.fname[0] == 0) {
                    break;
                }
            }
        }
        if (res == FR_OK) {
            for (;;) {
                res = f_readdir(&dir, &fno); // Read a directory item
                if (res != FR_OK || fno.fname[0] == 0 || strlen(fno.fname) > remainingSize) {
                    break;
                } // Error or end of dir
                const int written = snprintf(buf_ptr, remainingSize, "%s\n", fno.fname);
                buf_ptr += written;
                remainingSize -= written;
            }
        }
        f_closedir(&dir);
    } else {
        DEBUG_PRINT("Failed to open \"%s\". (%u)\n", path, res);
    }
}
