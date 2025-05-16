#pragma once

#include <stdint.h>
#include <stddef.h>
#include "ff.h"

#define MAX_LINES 2000 
#define MAX_LENGTH 255

const size_t c_fileNameLength = 255;
namespace picostation {
class FileSystem {
  public:
    enum Enum { IDLE, GETDIRECTORY };

    struct Status {
        uint8_t currentState;
        uint16_t totalItems;
        char cwd[c_fileNameLength];
    };

    void init();
    int getNumberofFileEntries(const char *dir);
    void readDirectoryToBuffer(void *buffer, const char *path, const size_t offset, const unsigned int bufferSize);
    void setDirectory(const char *dir);
    
  private:
    int m_fileCount = -1;
    char m_cwdbuf[c_fileNameLength] = "/";

    Status m_status;
};
}