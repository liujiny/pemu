//
// Created by cpasjuste on 01/06/18.
//

#include "burner.h"
#include "burnint.h"

#include "skeleton/pemu.h"
#include "pfbneo_ui_menu.h"
#include "pfbneo_ui_emu.h"
#include "pfbneo_ui_video.h"
#include "pfbneo_utility.h"
#include "retro_input_wrapper.h"

using namespace c2d;
using namespace pemu;

int nVidFullscreen = 0;
INT32 bVidUseHardwareGamma = 1;
bool g_video_needs_reinit = false;

UINT32 (__cdecl *VidHighCol)(INT32 r, INT32 g, INT32 b, INT32 i);
INT32 VidRecalcPal() { return BurnRecalcPal(); }

#ifdef __PFBA_ARM__
extern int nSekCpuCore;
static bool isHardware(int hardware, int type) {
    return (((hardware | HARDWARE_PREFIX_CARTRIDGE) ^ HARDWARE_PREFIX_CARTRIDGE)
            & 0xff000000) == (unsigned int) type;
}
#endif

static UINT32 myHighCol16(int r, int g, int b, int /* i */) {
    UINT32 t;
    t = (r << 8) & 0xf800;
    t |= (g << 3) & 0x07e0;
    t |= (b >> 3) & 0x001f;
    return t;
}

static UiMain *uiInstance;

PFBAUiEmu::PFBAUiEmu(UiMain *ui) : UiEmu(ui) {
    uiInstance = ui;
}

#ifdef __PFBA_ARM__
int PFBAUiEmu::getSekCpuCore() {
    int sekCpuCore = 0;
    std::vector<std::string> zipList;
    int hardware = BurnDrvGetHardwareCode();
    std::string bios = pMain->getConfig()->get(PEMUConfig::OptId::EMU_NEOBIOS, true)->getString();
    if (isHardware(hardware, HARDWARE_PREFIX_SNK) && Utility::contains(bios, "UNIBIOS")) sekCpuCore = 1;
    if (isHardware(hardware, HARDWARE_PREFIX_SEGA_MEGADRIVE)) sekCpuCore = 1;
    else if (isHardware(hardware, HARDWARE_PREFIX_SEGA)) {
        if (hardware & HARDWARE_SEGA_FD1089A_ENC || hardware & HARDWARE_SEGA_FD1089B_ENC ||
            hardware & HARDWARE_SEGA_MC8123_ENC || hardware & HARDWARE_SEGA_FD1094_ENC ||
            hardware & HARDWARE_SEGA_FD1094_ENC_CPU2) sekCpuCore = 1;
    } else if (isHardware(hardware, HARDWARE_PREFIX_TOAPLAN)) {
        zipList.emplace_back("batrider"); zipList.emplace_back("bbakraid"); zipList.emplace_back("bgaregga");
    } else if (isHardware(hardware, HARDWARE_PREFIX_SNK)) {
        zipList.emplace_back("kof97"); zipList.emplace_back("kof98"); zipList.emplace_back("kof99");
        zipList.emplace_back("kof2000"); zipList.emplace_back("kof2001"); zipList.emplace_back("kof2002");
        zipList.emplace_back("kf2k3pcb");
    }
    std::string zip = BurnDrvGetTextA(DRV_NAME);
    for (unsigned int i = 0; i < zipList.size(); i++) {
        if (zipList[i].compare(0, zip.length(), zip) == 0) {
            sekCpuCore = 1;
            break;
        }
    }
    zipList.clear();
    return sekCpuCore;
}
#endif

int PFBAUiEmu::load(const ss_api::Game &game) {
    currentGame = game;
    PFBNEOUtility::setDriverActive(game);
    if (nBurnDrvActive >= nBurnDrvCount) {
        pMain->getUiProgressBox()->setVisibility(Visibility::Hidden);
        pMain->getUiMessageBox()->show("ERROR", "THIS GAME IS NOT SUPPORTED BY FBNEO...", "OK");
        return -1;
    }
#ifdef __PFBA_ARM__
    nSekCpuCore = getSekCpuCore();
#endif
    int audio_freq = pMain->getConfig()->get(PEMUConfig::OptId::EMU_AUDIO_FREQ, true)->getInteger();
    nInterpolation = pMain->getConfig()->get(PEMUConfig::OptId::EMU_AUDIO_INTERPOLATION, true)->getInteger();
    nFMInterpolation = pMain->getConfig()->get(PEMUConfig::OptId::EMU_AUDIO_FMINTERPOLATION, true)->getInteger();
    bForce60Hz = pMain->getConfig()->get(PEMUConfig::OptId::EMU_FORCE_60HZ, true)->getInteger();
    if (bForce60Hz) nBurnFPS = 6000;

    EnableHiscores = 1;
    auto *aud = new Audio(audio_freq);
    nBurnSoundRate = aud->getSampleRate();
    nBurnSoundLen = aud->getSamples();
    pBurnSoundOut = (INT16 *) malloc(aud->getSamplesSize());
    if (DrvInit((int) nBurnDrvActive, false) != 0) {
        delete (aud);
        pMain->getUiProgressBox()->setVisibility(Visibility::Hidden);
        pMain->getUiMessageBox()->show("ERROR", "DRIVER INIT FAILED", "OK");
        stop();
        return -1;
    } else {
        // [作弊初始化 / Cheat Initialization] 驱动成功后加载作弊系统
        CheatInit();
    }
    delete (aud);
    free(pBurnSoundOut);
    nFramesEmulated = 0; nFramesRendered = 0; nCurrentFrame = 0;

    addAudio(audio_freq, Audio::toSamples(audio_freq, (float) nBurnFPS / 100.0f));
    if (audio->isAvailable()) {
        nBurnSoundRate = audio->getSampleRate();
        nBurnSoundLen = audio->getSamples();
        pBurnSoundOut = (INT16 *) malloc(audio->getSamplesSize());
    }
    audio_sync = !bForce60Hz;
    targetFps = (float) nBurnFPS / 100.0f;

    Vector2i size, aspect;
    BurnDrvGetFullSize(&size.x, &size.y);
    BurnDrvGetAspect(&aspect.x, &aspect.y);
    
    nBurnBpp = 2;
    BurnHighCol = myHighCol16;
    BurnRecalcPal();
    
    if (video) {
        delete video;
        video = nullptr;
    }
    auto v = new PFBAVideo(pMain, &pBurnDraw, &nBurnPitch, size, aspect);
    addVideo(v);

    return UiEmu::load(game);
}

void Reinitialise(void) {
    g_video_needs_reinit = true;
}

void PFBAUiEmu::stop() {
    // [作弊清理 / Cheat Cleanup] 游戏结束时清理作弊
    CheatExit();
    DrvExit();
    if (pBurnSoundOut) free(pBurnSoundOut);
    UiEmu::stop();
}

bool PFBAUiEmu::onInput(c2d::Input::Player *players) {
    if (pMain->getUiMenu()->isVisible() || pMain->getUiStateMenu()->isVisible()) {
        pMain->getInput()->setRotation(Input::Rotation::R0, Input::Rotation::R0);
        return UiEmu::onInput(players);
    }
    int rotation = getUi()->getConfig()->get(PEMUConfig::OptId::EMU_ROTATION, true)->getArrayIndex();
    if (BurnDrvGetFlags() & BDF_ORIENTATION_VERTICAL) {
        if (rotation == 0) pMain->getInput()->setRotation(Input::Rotation::R90, Input::Rotation::R0);
        else if (rotation == 1) pMain->getInput()->setRotation(Input::Rotation::R0, Input::Rotation::R0);
        else if (rotation == 2) pMain->getInput()->setRotation(Input::Rotation::R270, Input::Rotation::R0);
        else pMain->getInput()->setRotation(Input::Rotation::R270, Input::Rotation::R270);
    }
    return UiEmu::onInput(players);
}

void PFBAUiEmu::onUpdate() {
    if (isPaused()) return;

    if (g_video_needs_reinit) {
        Vector2i size, aspect;
        BurnDrvGetFullSize(&size.x, &size.y);
        BurnDrvGetAspect(&aspect.x, &aspect.y);
        if (size.x > 0 && size.y > 0) {
            if (video) {
                delete video;
                video = nullptr;
            }
            auto v = new PFBAVideo(pMain, &pBurnDraw, &nBurnPitch, size, aspect);
            addVideo(v);
        }
        g_video_needs_reinit = false;
    }

    InputMake(true);
    unsigned int buttons = pMain->getInput()->getButtons();
    if (buttons & Input::Button::Select) {
        if (clock.getElapsedTime().asSeconds() > 2) {
            if (pgi_reset) {
                pMain->getUiStatusBox()->show("TIPS: PRESS START BUTTON 2 SECONDS FOR DIAG MENU...");
                pgi_reset->Input.nVal = 1;
                *(pgi_reset->Input.pVal) = pgi_reset->Input.nVal;
            }
            nCurrentFrame = 0; nFramesEmulated = 0; clock.restart();
        }
    } else if (buttons & Input::Button::Start) {
        if (clock.getElapsedTime().asSeconds() > 2) {
            if (pgi_diag) {
                pMain->getUiStatusBox()->show("TIPS: PRESS COIN BUTTON 2 SECONDS TO RESET CURRENT GAME...");
                pgi_diag->Input.nVal = 1;
                *(pgi_diag->Input.pVal) = pgi_diag->Input.nVal;
            }
            clock.restart();
        }
    } else {
        clock.restart();
    }

#ifdef __VITA__
    int skip = pMain->getConfig()->get(PEMUConfig::OptId::EMU_FRAMESKIP, true)->getInteger();
#else
    int skip = 0;
#endif

    pBurnDraw = nullptr;
    frameskip++;

    if (frameskip > skip) {
        if (video && video->getTexture()) {
            video->getTexture()->lock(&pBurnDraw, &nBurnPitch, video->getTextureRect());
        }
        nFramesRendered++;
    }

    // [作弊每帧更新 / Cheat Per-Frame Update] 每一帧实时刷新作弊码内存修改
    CheatUpdate();

    BurnDrvFrame();
    nCurrentFrame++;

    if (frameskip > skip) {
        if (video && video->getTexture()) {
            video->getTexture()->unlock();
        }
        frameskip = 0;
    }

    if (audio) {
        audio->play(pBurnSoundOut, audio->getSamples(), audio_sync ? Audio::SyncMode::LowLatency : Audio::SyncMode::None);
    }
    UiEmu::onUpdate();
}
// [菜单选项隐藏判定 / Menu Option Hidden Check]
bool PFBAGuiMenu::isOptionHidden(c2d::config::Option *option) {
    // 调用父类 UiMenu 的默认过滤逻辑
    return UiMenu::isOptionHidden(option);
}