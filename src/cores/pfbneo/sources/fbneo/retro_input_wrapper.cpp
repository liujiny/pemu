//
// Created by cpasjuste on 24/02/2022.
//

#include "retro_input.h"
#include "skeleton/pemu.h"

unsigned nGameType = 0;
bool bIsNeogeoCartGame = false;
bool bLightgunHideCrosshairEnabled = true;
struct GameInp *pgi_reset = nullptr;
struct GameInp *pgi_diag = nullptr;
UINT8 *diag_input = nullptr;
struct GameInp *GameInp = nullptr;
UINT32 nGameInpCount = 0;
INT32 nAnalogSpeed = 0x0100;
INT32 nFireButtons = 0;
static int nDIPOffset;

// 帧同步连发计数器（每帧在 poll_cb 中递增）
static unsigned int s_frame_pulse_counter = 0;

int16_t input_cb(unsigned port, unsigned device, unsigned index, unsigned id) {
    if (port < PLAYER_MAX) {
        unsigned int buttons = c2d_renderer->getInput()->getPlayer((int) port)->buttons;

        if (device == RETRO_DEVICE_JOYPAD) {
            // 动态读取配置中 autofireA 和 autofireB 的开关状态
            bool autofire_a = false;
            bool autofire_b = false;
            if (c2d_renderer) {
                auto *ui = dynamic_cast<pemu::UiMain*>(c2d_renderer);
                if (ui && ui->getConfig()) {
                    autofire_a = ui->getConfig()->get(pemu::PEMUConfig::OptId::EMU_AUTOFIRE_A, true)->getInteger() > 0;
                    autofire_b = ui->getConfig()->get(pemu::PEMUConfig::OptId::EMU_AUTOFIRE_B, true)->getInteger() > 0;
                }
            }

            // 产生稳定的 2 帧通 / 2 帧断的高频脉冲（15Hz 最佳打击频率）
            bool is_pulse_off = ((s_frame_pulse_counter % 4) >= 2);

            // 1. autofireA 开启且检测 A 键 (RETRO_DEVICE_ID_JOYPAD_B)
            if (autofire_a && id == RETRO_DEVICE_ID_JOYPAD_B) {
                if (is_pulse_off && (buttons & id)) {
                    return 0; // 模拟松开
                }
            }

            // 2. autofireB 开启且检测 B 键 (RETRO_DEVICE_ID_JOYPAD_A)
            if (autofire_b && id == RETRO_DEVICE_ID_JOYPAD_A) {
                if (is_pulse_off && (buttons & id)) {
                    return 0; // 模拟松开
                }
            }

            return (buttons & id) ? 1 : 0;
        }
    }

    return 0;
}

void poll_cb() {
    // 确保脉冲计数器每帧只递增一次，不会因多按键轮询而错乱
    s_frame_pulse_counter++;
}

static void InpDIPSWGetOffset() {
    BurnDIPInfo bdi{};
    nDIPOffset = 0;

    for (int i = 0; BurnDrvGetDIPInfo(&bdi, i) == 0; i++) {
        if (bdi.nFlags == 0xF0) {
            nDIPOffset = bdi.nInput;
            HandleMessage(RETRO_LOG_INFO, "DIP switches offset: %d.\n", bdi.nInput);
            break;
        }
    }
}

void InpDIPSWResetDIPs() {
    int i = 0;
    BurnDIPInfo bdi{};
    struct GameInp *pgi;

    InpDIPSWGetOffset();

    while (BurnDrvGetDIPInfo(&bdi, i) == 0) {
        if (bdi.nFlags == 0xFF) {
            pgi = GameInp + bdi.nInput + nDIPOffset;
            if (pgi) {
                pgi->Input.Constant.nConst = pgi->Input.nVal =
                        (pgi->Input.Constant.nConst & ~bdi.nMask) | (bdi.nSetting & bdi.nMask);
                if (pgi->Input.pVal) {
                    *(pgi->Input.pVal) = pgi->Input.nVal;
                }
            }
        }
        i++;
    }
}