//
// Created by cpasjuste on 28/09/18.
//

#include "skeleton/pemu.h"
#include "pfbneo_ui_menu_state.h"
#include <cstdio>
#include <cerrno>
#include <string>
#if defined(__PS5__)
extern "C" void pemu_boot_mark(const char *) __attribute__((weak));
static void stateMark(const char *text) { if (pemu_boot_mark) pemu_boot_mark(text); }
#else
static void stateMark(const char *) {}
#endif

extern int BurnStateLoad(char *szName, int bAll, int (*pLoadGame)());

extern int BurnStateSave(char *szName, int bAll);

extern int DrvInitCallback();

PFBAUIStateMenu::PFBAUIStateMenu(pemu::UiMain *ui) : pemu::UiMenuState(ui) {

}

bool PFBAUIStateMenu::loadStateCore(const char *path) {
    stateMark("STATE load begin");
    bool ok = BurnStateLoad((char *) path, 1, &DrvInitCallback) == 0;
    stateMark(ok ? "STATE load ok" : "STATE load failed");
    return ok;
}

bool PFBAUIStateMenu::saveStateCore(const char *path) {
    stateMark("STATE save begin");
    // Complete a sibling file before replacing the existing slot. Allocation,
    // write and close failures must leave the previous save usable.
    const std::string temporary = std::string(path) + ".tmp";
    if (std::remove(temporary.c_str()) != 0 && errno != ENOENT) {
        stateMark("STATE save failed; temporary file unavailable");
        return false;
    }
    bool ok = BurnStateSave((char *) temporary.c_str(), 1) == 0;
    if (ok) ok = std::rename(temporary.c_str(), path) == 0;
    if (!ok) std::remove(temporary.c_str());
    stateMark(ok ? "STATE save ok" : "STATE save failed; previous slot retained");
    return ok;
}
