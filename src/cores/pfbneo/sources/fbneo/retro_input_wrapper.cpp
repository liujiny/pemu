//
// Created by cpasjuste on 24/02/2022.
//

#include "retro_input.h"
#include "skeleton/pemu.h" // 引入 pemu 配置支持

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

// 连发帧计数器
static unsigned int s_autofire_counter = 0;

int16_t input_cb(unsigned port, unsigned device, unsigned index, unsigned id) {
    if (port < PLAYER_MAX) {
        unsigned int buttons = c2d_renderer->getInput()->getPlayer((int) port)->buttons;

        if (device == RETRO_DEVICE_JOYPAD) {
            // 动态读取配置中的连发开关状态（传入 true 以正确读取当前游戏配置）
            bool autofire_enabled = false;
            if (c2d_renderer) {
                auto *ui = dynamic_cast<pemu::UiMain*>(c2d_renderer);
                if (ui && ui->getConfig()) {
                    // 注意这里的第二个参数 true 非常关键！
                    autofire_enabled = ui->getConfig()->get(pemu::PEMUConfig::OptId::EMU_AUTOFIRE, true)->getInteger() > 0;
                }
            }

            // 如果在配置中开启了连发，且当前按下的是 A 键
            if (autofire_enabled && id == RETRO_DEVICE_ID_JOYPAD_B) {
                s_autofire_counter++;
                // 每 4 帧中前 2 帧有效，后 2 帧模拟松开
                if ((s_autofire_counter % 4) >= 2 && (buttons & id)) {
                    return 0; 
                }
            }

            return buttons & id ? 1 : 0;
        }
    }

    return 0;
}
void poll_cb() {}

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