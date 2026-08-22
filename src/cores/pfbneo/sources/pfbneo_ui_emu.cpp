//
// Created by cpasjuste on 01/06/18.
//

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <string>
#include <vector>

#include "burner.h"
#include "burnint.h"

#include "skeleton/pemu.h"
#include "pfbneo_ui_emu.h"
#include "pfbneo_ui_video.h"
#include "pfbneo_utility.h"
#include "retro_input_wrapper.h"

using namespace c2d;
using namespace pemu;

TCHAR szAppCheatsPath[MAX_PATH] = {0};

// Vita uses pemu's own video frontend and deliberately does not compile
// FBNeo's vid_interface.cpp.  Provide the small compatibility surface that
// burner/misc.cpp expects without pulling the SDL video plugins back in.
#ifdef __VITA__
INT32 nVidFullscreen = 0;
INT32 bVidUseHardwareGamma = 1;
UINT32 (__cdecl *VidHighCol)(INT32 r, INT32 g, INT32 b, INT32 i) = nullptr;

INT32 VidRecalcPal() {
    // FBNeo's BurnRecalcPal() calls this hook before selecting VidHighCol.
    // pemu does not need a separate frontend palette cache on Vita.
    return 0;
}
#endif

bool g_video_needs_reinit = false;

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

// =================== [ 作弊子菜单 UI 管理 / Cheat Menu UI ] ===================
static const int MAX_VISIBLE_CHEATS = 12;
static RectangleShape *cheatMenuBg = nullptr;
static Text *cheatMenuTitle = nullptr;
static Text *cheatMenuFooter = nullptr;
static std::vector<Text*> cheatMenuLines;
static bool cheatMenuVisible = false;
static int cheatMenuSelected = 0;
static int cheatMenuScroll = 0;
static int totalCheatsCount = 0;
static C2DClock inputDelayClock;

static int GetCheatCount() {
    int count = 0;
    CheatInfo *pci = pCheatInfo;
    while (pci) {
        count++;
        pci = pci->pNext;
    }
    return count;
}

static CheatInfo* GetCheatInfoByIndex(int index) {
    int idx = 0;
    CheatInfo *pci = pCheatInfo;
    while (pci) {
        if (idx == index) return pci;
        idx++;
        pci = pci->pNext;
    }
    return nullptr;
}

// [安全可靠的作弊项状态切换函数 / Safe Cheat Option Switcher]
static void SetCheatOption(int nCheat, int nOption) {
    CheatInfo *pci = GetCheatInfoByIndex(nCheat);
    if (!pci) return;

    bCheatsAllowed = 1;
    pci->bRestoreOnDisable = 1;

    if (nOption == 0) {
        pci->nDefault = 0;
        CheatEnable(nCheat, 0);
        pci->nCurrent = 0;
        pci->nStatus = 0;
    } else {
        pci->nDefault = 0;
        if (pci->nCurrent == nOption) {
            pci->nCurrent = 0;
        }
        CheatEnable(nCheat, nOption);
        pci->nCurrent = nOption;
        pci->nStatus = 2;
    }

    CheatUpdate();
}

static void UpdateCheatMenuUI() {
    if (!cheatMenuVisible || !cheatMenuBg) return;

    totalCheatsCount = GetCheatCount();
    if (totalCheatsCount == 0) {
        cheatMenuTitle->setString("CHEAT MENU: NO CHEATS FOUND");
        for (int i = 0; i < MAX_VISIBLE_CHEATS; i++) {
            cheatMenuLines[i]->setVisibility(Visibility::Hidden);
        }
        return;
    }

    std::string romName = BurnDrvGetTextA(DRV_NAME);
    char titleBuf[128];
    snprintf(titleBuf, sizeof(titleBuf), "CHEAT MENU [%s] (%d/%d)", romName.c_str(), cheatMenuSelected + 1, totalCheatsCount);
    cheatMenuTitle->setString(titleBuf);

    if (cheatMenuSelected < cheatMenuScroll) {
        cheatMenuScroll = cheatMenuSelected;
    }
    if (cheatMenuSelected >= cheatMenuScroll + MAX_VISIBLE_CHEATS) {
        cheatMenuScroll = cheatMenuSelected - MAX_VISIBLE_CHEATS + 1;
    }

    for (int i = 0; i < MAX_VISIBLE_CHEATS; i++) {
        int itemIndex = cheatMenuScroll + i;
        if (itemIndex < totalCheatsCount) {
            CheatInfo *pci = GetCheatInfoByIndex(itemIndex);
            if (pci) {
                std::string optName = "DISABLED";
		if (pci->pOption[pci->nCurrent] != nullptr && strlen(pci->pOption[pci->nCurrent]->szOptionName) > 0) {
    		    optName = pci->pOption[pci->nCurrent]->szOptionName;
		} else if (pci->pOption[0] != nullptr && strlen(pci->pOption[0]->szOptionName) > 0) {
    		    optName = pci->pOption[0]->szOptionName;
		} else {
    		    optName = (pci->nCurrent > 0) ? "ENABLED" : "DISABLED";
		}

                char lineBuf[256];
                if (itemIndex == cheatMenuSelected) {
                    snprintf(lineBuf, sizeof(lineBuf), " > %-32s : [ < %s > ]", pci->szCheatName, optName.c_str());
                    cheatMenuLines[i]->setFillColor(Color::Yellow);
                } else {
                    snprintf(lineBuf, sizeof(lineBuf), "   %-32s : %s", pci->szCheatName, optName.c_str());
                    if (pci->nCurrent > 0) {
                        cheatMenuLines[i]->setFillColor(Color(80, 230, 80, 255));
                    } else {
                        cheatMenuLines[i]->setFillColor(Color(160, 160, 160, 255));
                    }
                }
                cheatMenuLines[i]->setString(lineBuf);
                cheatMenuLines[i]->setVisibility(Visibility::Visible);
            }
        } else {
            cheatMenuLines[i]->setVisibility(Visibility::Hidden);
        }
    }
}

static void CreateCheatMenu(UiMain *ui) {
    if (cheatMenuBg != nullptr) return;

    Vector2f winSize = ui->getSize();
    float w = 1100.0f;
    float h = 680.0f;
    float x = (winSize.x - w) / 2.0f;
    float y = (winSize.y - h) / 2.0f;

    cheatMenuBg = new RectangleShape(Vector2f(w, h));
    cheatMenuBg->setPosition(Vector2f(x, y));
    cheatMenuBg->setFillColor(Color(15, 15, 25, 240));
    cheatMenuBg->setOutlineColor(Color(0, 140, 255, 255));
    cheatMenuBg->setOutlineThickness(3.0f);
    cheatMenuBg->setVisibility(Visibility::Hidden);

    cheatMenuTitle = new Text("CHEAT MENU", 26, ui->getFont());
    cheatMenuTitle->setPosition(Vector2f(30.0f, 25.0f));
    cheatMenuTitle->setFillColor(Color::Yellow);
    cheatMenuBg->add(cheatMenuTitle);

    for (int i = 0; i < MAX_VISIBLE_CHEATS; i++) {
        auto *line = new Text("", 20, ui->getFont());
        line->setPosition(Vector2f(35.0f, 80.0f + (float)i * 42.0f));
        cheatMenuBg->add(line);
        cheatMenuLines.push_back(line);
    }

    cheatMenuFooter = new Text("[UP/DOWN] Select   [LEFT/RIGHT] Change   [SQUARE] Toggle All   [CROSS/CIRCLE/SELECT+TRIANGLE] Close", 18, ui->getFont());
    cheatMenuFooter->setPosition(Vector2f(30.0f, h - 45.0f));
    cheatMenuFooter->setFillColor(Color(180, 180, 180, 255));
    cheatMenuBg->add(cheatMenuFooter);

    ui->add(cheatMenuBg);
}
// ==============================================================================

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

// [FBNeo 作弊 INI 解析器 / FBNeo Cheat INI Parser]
static int LoadCheatIni(const std::string &filePath) {
    FILE *f = fopen(filePath.c_str(), "r");
    if (!f) return 0;

    CheatInfo *pCurrent = nullptr;
    CheatInfo *pLast = nullptr;
    char line[1024];
    int totalCheats = 0;

    while (fgets(line, sizeof(line), f)) {
        char *p = line;
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
        if (*p == '\0' || *p == ';' || *p == '#' || (*p == '/' && *(p + 1) == '/')) {
            continue;
        }

        char *end = p + strlen(p) - 1;
        while (end > p && (*end == '\r' || *end == '\n' || *end == ' ' || *end == '\t')) {
            *end = '\0';
            end--;
        }

        // 在 LoadCheatIni 函数处理 cheat 声明处：
if (strncmp(p, "cheat", 5) == 0 && (p[5] == ' ' || p[5] == '\t' || p[5] == '\"')) {
    char *nameStart = strchr(p, '\"');
    if (nameStart) {
        nameStart++;
        char *nameEnd = strchr(nameStart, '\"');
        if (nameEnd) {
            *nameEnd = '\0';
        }
        
        // 过滤空名称、纯空格或带有装饰符 (如 >>>> ---- ====) 的注释行
        std::string rawName = nameStart;
        while (!rawName.empty() && (rawName.front() == ' ' || rawName.front() == '\t')) rawName.erase(rawName.begin());
        while (!rawName.empty() && (rawName.back() == ' ' || rawName.back() == '\t')) rawName.pop_back();

        if (rawName.empty() || rawName[0] == '>' || rawName[0] == '-' || rawName[0] == '=') {
            pCurrent = nullptr;
            continue;
        }

        auto *pNew = (CheatInfo *) calloc(1, sizeof(CheatInfo) + sizeof(CheatOption *) * CHEAT_MAX_OPTIONS);
        strncpy(pNew->szCheatName, rawName.c_str(), sizeof(pNew->szCheatName) - 1);
        pNew->nDefault = 0;
        pNew->nCurrent = 0;
        pNew->nStatus = 0;
        pNew->nType = 0;
        pNew->bRestoreOnDisable = 1;

        auto *pOpt0 = (CheatOption *) calloc(1, sizeof(CheatOption) + sizeof(CheatAddressInfo) * 2);
        strncpy(pOpt0->szOptionName, "Disabled", sizeof(pOpt0->szOptionName) - 1);
        pOpt0->AddressInfo[0].nAddress = 0;
        pNew->pOption[0] = pOpt0;

        if (!pCheatInfo) {
            pCheatInfo = pNew;
        } else if (pLast) {
            pLast->pNext = pNew;
            pNew->pPrevious = pLast;
        }
        pLast = pNew;
        pCurrent = pNew;
        totalCheats++;
    }
    continue;
}

        if (!pCurrent) continue;

        if (strncmp(p, "default", 7) == 0) {
            int defVal = 0;
            if (sscanf(p + 7, "%d", &defVal) == 1) {
                pCurrent->nDefault = defVal;
            }
            continue;
        }

        if (isdigit(*p)) {
            char *optNameStart = nullptr;
            int optNum = (int) strtol(p, &optNameStart, 10);
            if (optNum >= 0 && optNum < CHEAT_MAX_OPTIONS) {
                while (*optNameStart == ' ' || *optNameStart == '\t') optNameStart++;
                if (*optNameStart == '\"') {
                    optNameStart++;
                    char *optNameEnd = strchr(optNameStart, '\"');
                    if (optNameEnd) {
                        *optNameEnd = '\0';
                        char *params = optNameEnd + 1;

                        if (pCurrent->pOption[optNum]) {
                            free(pCurrent->pOption[optNum]);
                        }

                        auto *pOption = (CheatOption *) calloc(1, sizeof(CheatOption) + sizeof(CheatAddressInfo) * 16);
                        strncpy(pOption->szOptionName, optNameStart, sizeof(pOption->szOptionName) - 1);

                        char *token = strtok(params, ", \t");
                        int count = 0, cpu = 0;
                        unsigned int addr = 0, val = 0;
                        int addrIdx = 0;

                        while (token && addrIdx < 15) {
                            if (count % 3 == 0) {
                                cpu = (int) strtol(token, nullptr, 0);
                            } else if (count % 3 == 1) {
                                addr = (unsigned int) strtoul(token, nullptr, 0);
                            } else if (count % 3 == 2) {
                                val = (unsigned int) strtoul(token, nullptr, 0);
                                pOption->AddressInfo[addrIdx].nCPU = cpu;
                                pOption->AddressInfo[addrIdx].nAddress = addr;
                                pOption->AddressInfo[addrIdx].nValue = val;
                                addrIdx++;
                            }
                            count++;
                            token = strtok(nullptr, ", \t");
                        }

                        pOption->AddressInfo[addrIdx].nAddress = 0;
                        pCurrent->pOption[optNum] = pOption;
                    }
                }
            }
        }
    }

    fclose(f);
    return totalCheats;
}

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

    bCheatsAllowed = pMain->getConfig()->get(PEMUConfig::OptId::EMU_CHEATS, true)->getInteger();
    CheatInit();

    if (DrvInit((int) nBurnDrvActive, false) != 0) {
        delete (aud);
        pMain->getUiProgressBox()->setVisibility(Visibility::Hidden);
        pMain->getUiMessageBox()->show("ERROR", "DRIVER INIT FAILED", "OK");
        stop();
        return -1;
    } else {
        std::string dataPath = pMain->getIo()->getDataPath();
        std::string cheatDir = dataPath + "cheats/";
        std::string romName = BurnDrvGetTextA(DRV_NAME);
        std::string cheatFile = cheatDir + romName + ".ini";

        int cheatCount = 0;
        if (bCheatsAllowed) {
            cheatCount = LoadCheatIni(cheatFile);
            int idx = 0;
            CheatInfo *pci = pCheatInfo;
            while (pci) {
                if (pci->nDefault > 0) {
                    SetCheatOption(idx, pci->nDefault);
                }
                idx++;
                pci = pci->pNext;
            }
        }
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

    CreateCheatMenu(pMain);
    cheatMenuVisible = false;
    cheatMenuSelected = 0;
    cheatMenuScroll = 0;
    if (cheatMenuBg) {
        cheatMenuBg->setVisibility(Visibility::Hidden);
    }

    return UiEmu::load(game);
}

void Reinitialise(void) {
    g_video_needs_reinit = true;
}

void PFBAUiEmu::stop() {
    cheatMenuVisible = false;
    if (cheatMenuBg) {
        cheatMenuBg->setVisibility(Visibility::Hidden);
    }

    DrvExit();
    if (pBurnSoundOut) {
        free(pBurnSoundOut);
        pBurnSoundOut = nullptr;
    }
    UiEmu::stop();
}

bool PFBAUiEmu::onInput(c2d::Input::Player *players) {
    if (pMain->getUiMenu()->isVisible() || pMain->getUiStateMenu()->isVisible()) {
        pMain->getInput()->setRotation(Input::Rotation::R0, Input::Rotation::R0);
        return UiEmu::onInput(players);
    }

    unsigned int buttons = pMain->getInput()->getButtons();

    // [快捷键] 按下 Select + △（Button::Y）呼出/关闭作弊子菜单
    bool toggleShortcut = (buttons & Input::Button::Select) && (buttons & Input::Button::Y);

    if (toggleShortcut && inputDelayClock.getElapsedTime().asMilliseconds() > 300) {
        cheatMenuVisible = !cheatMenuVisible;
        if (cheatMenuVisible) {
            pause();
            cheatMenuBg->setVisibility(Visibility::Visible);
            UpdateCheatMenuUI();
        } else {
            cheatMenuBg->setVisibility(Visibility::Hidden);
            resume();
        }
        inputDelayClock.restart();
        return true;
    }

    // 作弊子菜单开启时的交互逻辑
    if (cheatMenuVisible) {
        if (inputDelayClock.getElapsedTime().asMilliseconds() > 140) {
            if (buttons & Input::Button::Up) {
                if (cheatMenuSelected > 0) {
                    cheatMenuSelected--;
                    UpdateCheatMenuUI();
                }
                inputDelayClock.restart();
            } else if (buttons & Input::Button::Down) {
                if (cheatMenuSelected < totalCheatsCount - 1) {
                    cheatMenuSelected++;
                    UpdateCheatMenuUI();
                }
                inputDelayClock.restart();
            } else if (buttons & (Input::Button::Left | Input::Button::Right)) {
                CheatInfo *pci = GetCheatInfoByIndex(cheatMenuSelected);
                if (pci) {
                    int maxOpt = 0;
                    for (int o = 0; o < CHEAT_MAX_OPTIONS; o++) {
                        if (pci->pOption[o] != nullptr) maxOpt = o + 1;
                    }
                    if (maxOpt <= 1) maxOpt = 2;

                    int nextOpt = pci->nCurrent;
                    if (buttons & Input::Button::Right) {
                        nextOpt = (nextOpt + 1) % maxOpt;
                    } else {
                        nextOpt = (nextOpt - 1 + maxOpt) % maxOpt;
                    }

                    SetCheatOption(cheatMenuSelected, nextOpt);
                    UpdateCheatMenuUI();
                }
                inputDelayClock.restart();
            } else if (buttons & Input::Button::X) {
                CheatInfo *first = pCheatInfo;
                int targetState = (first && first->nCurrent > 0) ? 0 : 1;
                int idx = 0;
                CheatInfo *pci = pCheatInfo;
                while (pci) {
                    SetCheatOption(idx, targetState);
                    idx++;
                    pci = pci->pNext;
                }
                UpdateCheatMenuUI();
                inputDelayClock.restart();
            } else if (buttons & (Input::Button::A | Input::Button::B | Input::Button::Start)) {
                cheatMenuVisible = false;
                cheatMenuBg->setVisibility(Visibility::Hidden);
                resume();
                inputDelayClock.restart();
            }
        }
        return true;
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
    if (isPaused()) {
        UiEmu::onUpdate();
        return;
    }

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

    bool cheatsOpt = pMain->getConfig()->get(PEMUConfig::OptId::EMU_CHEATS, true)->getInteger() != 0;
    if (bCheatsAllowed != (int)cheatsOpt) {
        bCheatsAllowed = cheatsOpt ? 1 : 0;
        int idx = 0;
        CheatInfo *pci = pCheatInfo;
        while (pci) {
            if (!bCheatsAllowed) {
                if (pci->nCurrent > 0) {
                    SetCheatOption(idx, 0);
                }
            } else {
                if (pci->nDefault > 0) {
                    SetCheatOption(idx, pci->nDefault);
                }
            }
            idx++;
            pci = pci->pNext;
        }
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

    if (bCheatsAllowed) {
        CheatApply();
    }

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

// 兼容 FBNeo 最新内核的动态视频重置接口 (Dynamic Video Reinit Interface)
void ReinitialiseVideo(void) {
    Reinitialise();
}
