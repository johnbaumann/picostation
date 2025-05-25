#include "filesystem.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "f_util.h"
#include "ff.h"
#include <ctype.h>

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

void picostation::FileSystem::readCuesToBuffer(void *buffer, const char *path, const size_t offset, const unsigned int bufferSize) {
    FRESULT res;
    DIR dir;
    FILINFO fno;

    char *buf_ptr = (char *)buffer;
    int remainingSize = bufferSize;
    size_t skippedBytes = 0;
    size_t targetSkip = offset * bufferSize; // kaç byte atlanmalı

    char gListEntry[4]; // 'cue' + null

    res = f_opendir(&dir, path);
    if (res != FR_OK) {
        DEBUG_PRINT("Failed to open \"%s\". (%u)\n", path, res);
        return;
    }

    while (true) {
        res = f_readdir(&dir, &fno);
        if (res != FR_OK || fno.fname[0] == 0)
            break;

        // Sadece dosyaları (klasör değil) ve küçük boyutlu dosyaları al
        if (fno.fsize < 10000 && !(fno.fattrib & AM_DIR)) {
            const char* extension = strrchr(fno.fname, '.');
            if (!extension || strnlen(extension, 5) != 4)
                continue;

            gListEntry[0] = tolower(extension[1]);
            gListEntry[1] = tolower(extension[2]);
            gListEntry[2] = tolower(extension[3]);
            gListEntry[3] = '\0';

            if (strcmp(gListEntry, "cue") == 0) {
                size_t lineSize = strlen(fno.fname) + 1; // sadece \n, null terminator'a gerek yok çünkü buffer null'la başlıyor

                if (skippedBytes < targetSkip) {
                    skippedBytes += lineSize;
                    continue;
                }

                if ((size_t)remainingSize < lineSize + 1)
                    break; // buffer doldu

                int written = snprintf(buf_ptr, remainingSize, "%s\n", fno.fname);
                if (written <= 0 || written >= remainingSize)
                    break;

                buf_ptr += written;
                remainingSize -= written;
            }
        }
    }

    f_closedir(&dir);
}

void picostation::FileSystem::getCueName(const char *path,TCHAR lines[MAX_LENGTH], int imageIndex, const size_t offset, const unsigned int bufferSize){
    FRESULT res;
    DIR dir;
    FILINFO fno;
    int remainingSize = bufferSize;
    int lineCount = 1;  
    char gListEntry[5];
    res = f_opendir(&dir, path); // Open the directory
    int loopCounter = 1;
    if (res == FR_OK) {

            for (;;) {
                res = f_readdir(&dir, &fno); // Read a directory item
                if (res != FR_OK || fno.fname[0] == 0 || strlen(fno.fname) > remainingSize) {
                    break;
                } // Error or end of dir
                    if (fno.fsize < 10000 && !(fno.fattrib & AM_DIR) ) {
                        const char* extension = strrchr( fno.fname, '.' ); // Find the last occurrence of '.'
                        if ( !extension || strnlen( extension, 5 ) != 4 ) { // ex cdrom.ps-exe  extlen: 5
                            continue;
                        } 
                        else {
                            gListEntry[ 0 ] = tolower( extension[ 1 ] );
                            gListEntry[ 1 ] = tolower( extension[ 2 ] );
                            gListEntry[ 2 ] = tolower( extension[ 3 ] );
                            gListEntry[ 3 ] = '\0';
                            if ((gListEntry[0] == 'c' && gListEntry[1] == 'u' && gListEntry[2] == 'e')){
                                if (imageIndex == loopCounter) {
                                    strncpy(lines, fno.fname, 255);
                                    break;
                                }
                                loopCounter++;
                            } 
                        }
                    }
            }
        
        f_closedir(&dir);
    } else {
        DEBUG_PRINT("Failed to open \"%s\". (%u)\n", path, res);
    }
}


void picostation::FileSystem::readDirsToBuffer(void *buffer, const char *path, const size_t offset, const unsigned int bufferSize) {
    FRESULT res;
    DIR dir;
    FILINFO fno;

    char *buf_ptr = (char *)buffer;
    int remainingSize = bufferSize;
    size_t skippedBytes = 0;
    size_t targetSkip = offset * bufferSize;

    res = f_opendir(&dir, path);
    if (res != FR_OK) {
        return;
    }

    while (true) {
        res = f_readdir(&dir, &fno);
        if (res != FR_OK) {
            break;
        }
        if (fno.fname[0] == 0) {
            break;
        }


        if ((fno.fattrib & AM_DIR) && !(fno.fattrib & AM_HID) && !(fno.fattrib & AM_SYS)) {
            int lineSize = snprintf(nullptr, 0, "%s\n", fno.fname);

            if (skippedBytes < targetSkip) {
                skippedBytes += lineSize;
                continue;
            }

            if ((size_t)remainingSize < lineSize + 1) {
                break;
            }

            int written = snprintf(buf_ptr, remainingSize, "%s\n", fno.fname);
            if (written <= 0 || written > remainingSize) {
                break;
            }


            buf_ptr += written;
            remainingSize -= written;
        }
    }

    f_closedir(&dir);
}


void picostation::FileSystem::getDirName(const char *path,TCHAR lines[MAX_LENGTH], int directoryIndex, const size_t offset, const unsigned int bufferSize){
    FRESULT res;
    DIR dir;
    FILINFO fno;
    int remainingSize = bufferSize;
    int lineCount = 1;  
    char gListEntry[5];
    res = f_opendir(&dir, path); // Open the directory
    int loopCounter = 1;
    if (res == FR_OK) {
        if (offset > 0) {
            for (int i = 0; i < offset; i++) {
                res = f_readdir(&dir, &fno);
                if (res != FR_OK || fno.fname[0] == 0) {
                    break;
                }
            }
        }//
        if (res == FR_OK) {
            for (;;) {
                res = f_readdir(&dir, &fno); // Read a directory item
                if (res != FR_OK || fno.fname[0] == 0 || strlen(fno.fname) > remainingSize) {
                    break;
                } // Error or end of dir
                int length = strlen(fno.fname);
                    if ((fno.fattrib & AM_DIR) && !(fno.fattrib &AM_HID) && !(fno.fattrib & AM_SYS)) {
                        if (directoryIndex == loopCounter) {
                            strncpy(lines,path,255);
                            strncat(lines, "/",255);
                            strncat(lines,fno.fname,255);
                            break;
                        }
                        loopCounter++;
                    }
            }
        }
        f_closedir(&dir);
    } else {
        DEBUG_PRINT("Failed to open \"%s\". (%u)\n", path, res);
    }
}