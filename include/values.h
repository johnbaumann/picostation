#pragma once

#include <stddef.h>
#include <stdint.h>

#include "hardware/pio.h"

// GPIO pinouts
namespace Pin {
enum : unsigned int {
    XLAT = 0,
    SQCK = 1,
    LMTSW = 2,
    SCEX_DATA = 4,  // UART1 TX
    DOOR = 6,
    RESET = 7,
    SD_MISO = 8,   // SPI1 RX
    SD_CS = 9,     // SPI1 CSn
    SD_SCK = 10,   // SPI1 SCK
    SD_MOSI = 11,  // SPI1 TX
    SENS = 14,
    DA15 = 15,
    DA16 = 16,
    LRCK = 17,
    SCOR = 18,
    SQSO = 19,
    CLK = 21,
    LED = 25,
    CMD_DATA = 26,
    CMD_CK = 27
};
constexpr unsigned int allPins[] = {XLAT, SQCK, LMTSW, SCEX_DATA, DOOR, RESET,    SENS,  DA15,
                                    DA16, LRCK, SCOR,  SQSO,      CLK,  CMD_DATA, CMD_CK};
};  // namespace Pin
// C2PO, WFCK is always GND

namespace SENS {
enum : unsigned int {
    FZC = 0x0,
    AS = 0x1,
    TZC = 0x2,
    XBUSY = 0x4,
    FOK = 0x5,
    GFS = 0xa,
    COMP = 0xb,
    COUT = 0xc,
    OV64 = 0xe
};
}

namespace SledMove {
enum : int { REVERSE = -1, STOP = 0, FORWARD = 1 };
}

namespace PIOInstance {
PIO const I2S_DATA = pio0;
PIO const MECHACON = pio0;
PIO const SOCT = pio0;
PIO const SUBQ = pio0;
}  // namespace PIOInstance

namespace SM {
// PIO0
constexpr unsigned int I2S_DATA = 0;
constexpr unsigned int MECHACON = 1;
constexpr unsigned int SOCT = 2;
constexpr unsigned int SUBQ = 3;
}  // namespace SM

constexpr int NUM_IMAGES = 3;
constexpr int c_leadIn = 4500;
constexpr int c_preGap = 150;
constexpr int c_licenseSectors = 16; // License sectors in the loader image

constexpr uint32_t c_trackMin = 0;
constexpr uint32_t c_trackMax = 20892;  // 73:59:58
constexpr int c_sectorMin = 0;
constexpr int c_sectorMax = 333000;  // 74:00:00

constexpr unsigned int c_MaxTrackMoveTime = 15;    // uS
constexpr unsigned int c_MaxSubqDelayTime = 3333;  // uS


constexpr size_t c_cdSamplesSize = 588;
constexpr size_t c_cdSamplesBytes = c_cdSamplesSize * 2 * 2;  // 2352
