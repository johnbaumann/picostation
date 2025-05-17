#pragma once

#include <stddef.h>
#include <stdint.h>

#include "hardware/pio.h"

#include "picostation_pinout.h"

// GPIO pinouts
namespace Pin {
enum : unsigned int {
    XLAT = GPIO_XLAT,
    SQCK = GPIO_SQCK,
    LMTSW = GPIO_LMTSW,
    SCEX_DATA = GPIO_SCEX_DATA,  // UART1 TX
    DOOR = GPIO_DOOR,
    RESET = GPIO_RESET,
    SD_MISO = GPIO_SD_MISO,   // SPI1 RX
    SD_CS = GPIO_SD_CS,     // SPI1 CSn
    SD_SCK = GPIO_SD_SCK,   // SPI1 SCK
    SD_MOSI = GPIO_SD_MOSI,  // SPI1 TX
    SENS = GPIO_SENS,
    DA15 = GPIO_DA15,
    DA16 = GPIO_DA16,
    LRCK = GPIO_LRCK,
    SCOR = GPIO_SCOR,
    SQSO = GPIO_SQSO,
    CLK = GPIO_CLK,
    LED = GPIO_LED,
    CMD_DATA = GPIO_CMD_DATA,
    CMD_CK = GPIO_CMD_CK
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
constexpr unsigned int c_MaxSubqDelayTime = 5410;  // uS

constexpr size_t c_cdSamplesSize = 588;
constexpr size_t c_cdSamplesBytes = c_cdSamplesSize * 2 * 2;  // 2352
