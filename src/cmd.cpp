#include "cmd.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "drive_mechanics.h"
#include "hardware/pio.h"
#include "logging.h"
#include "main.pio.h"
#include "pico/bootrom.h"
#include "picostation.h"
#include "pseudo_atomics.h"
#include "values.h"

#if DEBUG_CMD
#define DEBUG_PRINT(...) printf(__VA_ARGS__)
#else
#define DEBUG_PRINT(...) while (0)
#endif

extern pseudoatomic<int> g_imageIndex;
extern pseudoatomic<int> g_directoryIndex;
extern pseudoatomic<int> g_imageChangeFlag;
extern pseudoatomic<int> g_dirChangeFlag;
extern pseudoatomic<int> g_gameListingMode;
extern pseudoatomic<int> g_dirListingMode;
extern pseudoatomic<int> g_goBack;


inline void picostation::MechCommand::audioControl(const uint32_t latched) {
    const uint32_t pct2_bit = (1 << 14);
    const uint32_t pct1_bit = (1 << 15);
    const uint32_t mute_bit = (1 << 17);

    if (latched & mute_bit) {
        // g_audioCtrlMode = 0;
        DEBUG_PRINT("Mute\n");
        return;
    }

    g_audioCtrlMode = (latched & (pct1_bit | pct2_bit)) >> 14;
    /*switch (g_audioCtrlMode) {
        case audioControlModes::NORMAL:
        case audioControlModes::ALTNORMAL:
            g_audioPeak = 0;
            DEBUG_PRINT("NORMAL\n");
            break;

        case audioControlModes::LEVELMETER:
            DEBUG_PRINT("LEVELMETER\n");
            break;

        case audioControlModes::PEAKMETER:
            DEBUG_PRINT("PEAKMETER\n");
            break;
    }*/
}

inline void picostation::MechCommand::autoSequence(const uint32_t latched)  // $4X
{
    const uint32_t subCommand = (latched & 0x0F0000) >> 16;
    const bool reverseJump = subCommand & 0x1;
    // const uint32_t timer_range = (latched & 0x8) >> 3;
    // const uint32_t cancel_timer = (latched & 0xF) >> 4;

    int tracks_to_move = 0;

    m_sensData[SENS::XBUSY] = (subCommand != 0);
    const int track = g_driveMechanics.getTrack();

    if (subCommand == 0x7)  // Focus-On
    {
        DEBUG_PRINT("Focus-On\n");
        return;
    }

    switch (subCommand & 0xe) {
        case 0x0:  // Cancel
            //DEBUG_PRINT("Cancel\n"); // Commenting this out, too spammy
            return;

        case 0x4:  // Fine search
            tracks_to_move = m_jumpTrack;
            DEBUG_PRINT("Fine search%d\n", track);
            break;

        case 0x8:  // 1 Track Jump
            tracks_to_move = 1;
            DEBUG_PRINT("1 Track Jump%d\n", track);
            break;

        case 0xA:  // 10 Track Jump
            tracks_to_move = 10;
            DEBUG_PRINT("10 Track Jump%d\n", track);
            break;

        case 0xC:  // 2N Track Jump
            tracks_to_move = (2 * m_jumpTrack);
            DEBUG_PRINT("2N Track Jump%d\n", track);
            break;

        case 0xE:  // M Track Move
            tracks_to_move = m_jumpTrack;
            DEBUG_PRINT("M Track Move%d\n", track);
            break;

        default:
            DEBUG_PRINT("Unsupported command: %x\n", subCommand);
            break;
    }

    if (reverseJump) {
        m_autoSeqTrack = track - tracks_to_move;
    } else {
        m_autoSeqTrack = track + tracks_to_move;
    }

    if (!m_autoSeqAlarmID) {
        m_autoSeqAlarmID = add_alarm_in_ms(
            15,
            [](alarm_id_t id, void *user_data) -> int64_t {
                picostation::MechCommand *mechCommand = static_cast<picostation::MechCommand *>(user_data);

                mechCommand->updateAutoSeqTrack();

                return 0;
            },
            this, true);
    }
}

inline void picostation::MechCommand::customCommand(const uint32_t latched) {
    const uint32_t subCommand = (latched & 0x0F0000) >> 16;
    const uint32_t arg = (latched & 0xFFFF);
    //printf("Custom command: %x %x\n", subCommand, arg);
    switch (subCommand) {
        case 0x0:
            printf("directory change: %x %x\n", subCommand, arg);
            g_directoryIndex = arg;
            g_dirChangeFlag = 1;
            break;
        case 0x1:
            printf("Game listing %x %x\n");
            g_gameListingMode = 1;
            break;
        case 0x2:
            printf("disc image change: %x %x\n", subCommand, arg);
            g_imageIndex = arg;
            g_imageChangeFlag = 1;
            break;
        case 0x3:
            printf("Dir listing: %x %x\n", subCommand, arg);
            g_dirListingMode = 1;
            break;
        case 0x4:
            printf("Go back directory: %x %x\n", subCommand, arg);
            g_goBack = 1;
            break;
        case 0xa:
            if (arg == 0xBEEF) {
                // Restart into bootloader
                rom_reset_usb_boot_extra(Pin::LED, 0, false);
            }
            break;
    }
}

inline void picostation::MechCommand::funcSpec(const uint32_t latched)  // $9X
{
    // const bool flfc = latched & (1 << 13);
    // const bool biligl_sub = latched & (1 << 14);
    // const bool biligl_main = latched & (1 << 15);
    // const bool dpll = latched & (1 << 16);
    // const bool aseq = latched & (1 << 17);
    const bool dspb = latched & (1 << 18);
    // const bool dclv = latched & (1 << 19);

    if (!dspb)  // DSPB = 0 Normal-speed playback, DSPB = 1 Double-speed playback
    {
        g_targetPlaybackSpeed = 1;
    } else {
        g_targetPlaybackSpeed = 2;
    }
}

inline void picostation::MechCommand::modeSpec(const uint32_t latched)  // $8X
{
    const bool soct = latched & (1 << 13);
    // const bool ashs = latched & (1 << 14);
    // const bool vco_sel = latched & (1 << 15);
    // const bool wsel = latched & (1 << 16);
    // const bool dout_mutef = latched & (1 << 17);
    // const bool dout_mute = latched & (1 << 18);
    // const bool cdrom = latched & (1 << 19);

    if (soct) {
        m_soctEnabled = true;
        pio_sm_set_enabled(PIOInstance::SUBQ, SM::SUBQ, false);
        soct_program_init(PIOInstance::SOCT, SM::SOCT, g_soctOffset, Pin::SQSO, Pin::SQCK);
        pio_sm_set_enabled(PIOInstance::SOCT, SM::SOCT, true);
        pio_sm_put_blocking(PIOInstance::SOCT, SM::SOCT, 0xFFFFFFF);
    }
}

inline void picostation::MechCommand::trackingMode(const uint32_t latched)  // $2X
{
    const uint32_t subcommand_tracking = (latched & 0x0C0000) >> 16;
    switch (subcommand_tracking)  // Tracking servo
    {
        case 8:  // Forward track jump
            g_driveMechanics.moveTrack(1);
            break;
        case 0xC:  // Reverse track jump
            g_driveMechanics.moveTrack(-1);
            break;
    }

    const uint32_t subcommand_sled = (latched & 0x030000) >> 16;
    switch (subcommand_sled)  // Sled servo
    {
        case 2:  // Forward sled move
            g_driveMechanics.setSledMoveDirection(SledMove::FORWARD);
            break;

        case 3:  // Reverse sled move
            g_driveMechanics.setSledMoveDirection(SledMove::REVERSE);
            break;

        default:  // case 0: case 1: // sled servo off/on
            g_driveMechanics.setSledMoveDirection(SledMove::STOP);
            return;
    }
}

inline void picostation::MechCommand::spindleControl(const uint32_t latched) {
    const uint32_t subCommand = (latched & 0x0F0000) >> 16;

    m_sensData[SENS::GFS] = (subCommand == SpindleCommands::CLVA);
    if (!m_sensData[SENS::GFS]) {
        g_subqDelay = false;
        pio_sm_clear_fifos(PIOInstance::SUBQ, SM::SUBQ);
    }

    /*switch (subCommand)
    {
    case SpindleCommands::STOP: // 0
        DEBUG_PRINT("Stop spindleControl\n");
        break;

    case SpindleCommands::KICK:                // 8
        DEBUG_PRINT("Kick spindle\n"); // Forward rotation
        break;

    case SpindleCommands::BRAKE:                // A
        DEBUG_PRINT("Brake spindle\n"); // Reverse rotation
        break;

    case SpindleCommands::CLVS:        // E
        DEBUG_PRINT("CLVS\n"); // Rough servo mode
        break;

    case SpindleCommands::CLVH:        // C
        DEBUG_PRINT("CLVH\n"); // ?
        break;

    case SpindleCommands::CLVP:        // F
        DEBUG_PRINT("CLVP\n"); // PLL servo mode
        break;

    case SpindleCommands::CLVA: // 6
        // DEBUG_PRINT("CLVA\n"); // Automatic CLVS/CLVP switching mode
        break;
    }*/
}

void __time_critical_func(picostation::MechCommand::processLatchedCommand)() {
    const uint32_t latched = m_latched;
    const uint32_t command = (latched & 0xF00000) >> 20;
    m_latched = 0;

    switch (command) {
        case TopLevelCommands::TRACKING_MODE:  // $2X commands - Tracking and sled servo control
            trackingMode(latched);
            break;

        case TopLevelCommands::AUTO_SEQUENCE:  // $4X commands
            autoSequence(latched);
            break;

        case TopLevelCommands::JUMP_COUNT:  // $7X commands - Auto sequence track jump count setting
            m_jumpTrack = (latched & 0xFFFF0) >> 4;
            DEBUG_PRINT("jump: %d\n", m_jumpTrack);
            break;

        case TopLevelCommands::MODE_SPEC:  // $8X commands - MODE specification
            modeSpec(latched);
            break;
        case TopLevelCommands::FUNC_SPEC:  // $9X commands - Function specification
            funcSpec(latched);
            break;

        case 0xA:  // $AX commands - Audio CTRL
            audioControl(latched);
            break;

        case TopLevelCommands::MONITOR_COUNT:  // $BX commands - This command sets the traverse monitor count.
            g_driveMechanics.setCountTrack((latched & 0xFFFF0) >> 4);
            break;
        case TopLevelCommands::SPINDLE:  // $EX commands - Spindle motor control
            spindleControl(latched);
            break;

            /*
            case TopLevelCommands::FOCUS_CONTROL: // $0X commands - Focus control
            case 0x1:
            case 0x3:
            case 0x5: // Blind/brake
            case 0x6: // Kick
                break;*/

        case TopLevelCommands::CUSTOM:  // picostation
            customCommand(latched);
            break;
    }
}

bool __time_critical_func(picostation::MechCommand::getSens)(const size_t what) const { return m_sensData[what]; }

void __time_critical_func(picostation::MechCommand::setSens)(const size_t what, const bool new_value) {
    m_sensData[what] = new_value;
    if (what == m_currentSens) {
        gpio_put(Pin::SENS, new_value);
    }
}

bool picostation::MechCommand::getSoct() { return m_soctEnabled.Load(); }
void picostation::MechCommand::setSoct(const bool new_value) { m_soctEnabled = new_value; }

void picostation::MechCommand::updateAutoSeqTrack() {
    m_autoSeqAlarmID = 0;
    g_driveMechanics.setTrack(m_autoSeqTrack);
    m_sensData[SENS::XBUSY] = 0;
}

void __time_critical_func(picostation::MechCommand::updateMechSens)() {
    while (!pio_sm_is_rx_fifo_empty(PIOInstance::MECHACON, SM::MECHACON)) {
        const uint32_t c = pio_sm_get_blocking(PIOInstance::MECHACON, SM::MECHACON) >> 24;
        m_latched = m_latched >> 8;
        m_latched = m_latched | (c << 16);
        m_currentSens = c >> 4;
    }
    gpio_put(Pin::SENS, m_sensData[m_currentSens]);
}