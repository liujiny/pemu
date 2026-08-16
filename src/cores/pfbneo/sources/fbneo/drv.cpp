// Driver Init module
#define DR_FLAC_IMPLEMENTATION
#include "dr_flac.h"
#include "skeleton/pemu.h"
#include "burner.h"
#include "retro_input.h"

using namespace pemu;

extern UiMain *pemu_ui;

extern UINT8 NeoSystem;
int bDrvOkay = 0;
int kNetGame = 0;
INT32 nInputIntfMouseDivider = 1;
bool bRunPause;

bool is_netgame_or_recording() {
    return false;
}

static int ProgressCreate();

static UINT8 NeoSystemList[] = {
        0x13, 0x14, 0x15, 0x16, 0x00, 0x01, 0x02, 0x03, 
        0x04, 0x05, 0x08, 0x09, 0x0a, 0x10, 0x0f, 0x0b, 
        0x0c, 0x12, 0x11
};

static UINT32 myHighCol16(int r, int g, int b, int /* i */) {
    UINT32 t;
    t = (r << 8) & 0xf800;
    t |= (g << 3) & 0x07e0;
    t |= (b >> 3) & 0x001f;
    return t;
}

static int DoLibInit() {
    int nRet;
    ProgressCreate();
    nRet = BzipOpen(false);
    if (nRet) {
        BzipClose();
        return 1;
    }
    NeoSystem = NeoSystemList[pemu_ui->getConfig()->get(PEMUConfig::OptId::EMU_NEOBIOS, true)->getArrayIndex()];
    nRet = BurnDrvInit();
    BzipClose();
    if (nRet) {
        BurnDrvExit();
        return 1;
    } else {
        return 0;
    }
}

static int DrvLoadRom(unsigned char *Dest, int *pnWrote, int i) {
    int nRet;
    BzipOpen(false);
    if ((nRet = BurnExtLoadRom(Dest, pnWrote, i)) != 0) {
        char szText[256];
        char *pszFilename;
        BurnDrvGetRomName(&pszFilename, i, 0);
        sprintf(szText, "Error loading %s for %s.\nEmulation will likely have problems.",
                pszFilename, BurnDrvGetTextA(DRV_NAME));
        pemu_ui->getUiMessageBox()->show("ERROR", szText, "OK");
    }
    BzipClose();
    BurnExtLoadRom = DrvLoadRom;
    return nRet;
}

int DrvInit(int nDrvNum, bool bRestore) {
    DrvExit();

    // 关键：在 BurnDrvInit 前设置 Bpp 与颜色映射，避免驱动生成全黑调色板
    nBurnBpp = 2;
    BurnHighCol = myHighCol16;

    nBurnDrvSelect[0] = (UINT32) nDrvNum;
    bIsNeogeoCartGame = ((BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK) == HARDWARE_SNK_NEOGEO);
    nMaxPlayers = BurnDrvGetMaxPlayers();
    SetDefaultDeviceTypes();
    InputInit();
    SetControllerInfo();

    if (DoLibInit()) return 1;

    BurnExtLoadRom = DrvLoadRom;

    char path[1024];
    snprintf(path, 1023, "%s%s.fs", szAppEEPROMPath, BurnDrvGetTextA(DRV_NAME));
    BurnStateLoad(path, 0, nullptr);

    bDrvOkay = 1;
    nBurnLayer = 0xff;
    nSpriteEnable = 0xff;
    return 0;
}

int DrvInitCallback() {
    return DrvInit((int) nBurnDrvSelect[0], false);
}

int DrvExit() {
    if (bDrvOkay) {
        if (nBurnDrvSelect[0] < nBurnDrvCount) {
            char path[1024];
            snprintf(path, 1023, "%s%s.fs", szAppEEPROMPath, BurnDrvGetTextA(DRV_NAME));
            BurnStateSave(path, 0);
            InputExit();
            BurnDrvExit();
        }
    }
    BurnExtLoadRom = nullptr;
    bDrvOkay = 0;
    nBurnDrvSelect[0] = ~0U;
    return 0;
}

static double nProgressPosBurn = 0;

static int ProgressCreate() {
    nProgressPosBurn = 0;
    pemu_ui->getUiProgressBox()->setVisibility(c2d::Visibility::Visible);
    pemu_ui->getUiProgressBox()->setLayer(1000);
    return 0;
}

int ProgressUpdateBurner(double dProgress, const TCHAR *pszText, bool bAbs) {
    pemu_ui->getUiProgressBox()->setTitle(BurnDrvGetTextA(DRV_FULLNAME));
    if (pszText) {
        nProgressPosBurn += dProgress;
        pemu_ui->getUiProgressBox()->setMessage(pszText);
        pemu_ui->getUiProgressBox()->setProgress((float) nProgressPosBurn);
    } else {
        pemu_ui->getUiProgressBox()->setMessage("Please wait...");
    }
    pemu_ui->flip();
    return 0;
}

int AppError(TCHAR *szText, int bWarning) {
    return 1;
}

#ifdef __PFBN_LIGHT__
void nes_add_cheat(char *code) {};
void nes_remove_cheat(char *code) {};
#endif

extern bool g_video_needs_reinit;
void ReinitialiseVideo(void) {
    g_video_needs_reinit = true;
}