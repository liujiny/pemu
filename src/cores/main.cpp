//
// Created by cpasjuste on 19/09/23.
//

#include "main.h"
#include <cstdio>

#ifdef __VITA__
#include <psp2/power.h>
#endif

using namespace c2d;
using namespace pemu;

PEMUUiMain *pemu_ui;

#if defined(__PS5__) || defined(__PROSPERO__)
// The native CRT provides this optional startup trace; other launch paths do not.
extern "C" void pemu_boot_mark(const char *) __attribute__((weak));
extern "C" void pemu_native_input_probe(unsigned) __attribute__((weak));
static void boot_mark(const char *stage) {
    if (pemu_boot_mark) pemu_boot_mark(stage);
}
#else
static void boot_mark(const char *) {}
#endif

#if (defined(__PS5__) || defined(__PROSPERO__)) && defined(__SDL2__) && defined(__GLAD__)
#define PEMU_NATIVE_SDL_TRACE 1
extern "C" const char *pemu_native_data_path() __attribute__((weak));

static void SDLCALL boot_sdl_log(void *, int category, SDL_LogPriority priority,
                                const char *message) {
    char line[192];
    std::snprintf(line, sizeof(line), "SDL[%d/%d] %s", category,
                  static_cast<int>(priority), message ? message : "<null>");
    boot_mark(line);
}

static bool boot_renderer_ready(PEMUUiMain *ui) {
    // Capture the constructor's error before another SDL query can replace it.
    char line[192];
    const char *error = SDL_GetError();
    std::snprintf(line, sizeof(line), "SDL error: %s", error && *error ? error : "<none>");
    boot_mark(line);

    SDL_Window *window = ui->getWindow();
    SDL_GLContext context = ui->getContext();
    const char *driver = SDL_GetCurrentVideoDriver();
    std::snprintf(line, sizeof(line), "SDL driver=%s available=%d window=%p context=%p",
                  driver ? driver : "<none>", ui->available ? 1 : 0,
                  static_cast<void *>(window), context);
    boot_mark(line);
    std::snprintf(line, sizeof(line), "GL entrypoints glGenTextures=%p glGetString=%p",
                  reinterpret_cast<void *>(glad_glGenTextures),
                  reinterpret_cast<void *>(glad_glGetString));
    boot_mark(line);
    // Inspect pointers only: an unavailable context must not receive GL calls.
    return ui->available && window && context && glad_glGenTextures && glad_glGetString;
}
#endif

int main(int argc, char **argv) {
    boot_mark("frontend main body");

#ifdef __VITA__
    // Vita CPU overclock for emulator performance
    scePowerSetArmClockFrequency(444);
#endif
    // command line game info
    Game game;
    boot_mark("game object ok");

    // custom io
    boot_mark("io create begin");
    const auto io = new PEMUIo();
    boot_mark("io create ok");

#if defined(PEMU_NATIVE_SDL_TRACE)
    const bool native_startup = pemu_native_data_path != nullptr;
    SDL_LogOutputFunction previous_sdl_log = nullptr;
    void *previous_sdl_log_data = nullptr;
    if (native_startup) {
        SDL_LogGetOutputFunction(&previous_sdl_log, &previous_sdl_log_data);
        SDL_LogSetOutputFunction(boot_sdl_log, nullptr);
    }
#endif

    // create main ui/renderer
    // NOTE: size must stay {0, 0} on non-switch platforms (PS4, PS5, Vita, Linux, Windows...).
    // libcross2d's SDL2Renderer only sets SDL_WINDOW_FULLSCREEN_DESKTOP when the requested
    // size is <= 0 (see SDL2Renderer::SDL2Renderer in libcross2d/source/platforms/sdl2/sdl2_renderer.cpp).
    // Passing a fixed size like {1280, 720} here creates a small, non-fullscreen SDL window,
    // which on PS4 (no windowing system / no desktop compositor) never actually gets
    // presented to the TV output -> black screen, even though audio keeps working fine
    // since it's an independent subsystem. Switch ignores this parameter entirely and
    // forces its own {1280, 720} internally (see UiMain's __SWITCH__ constructor), so it's
    // safe to leave this at {0, 0} for every platform.
    boot_mark("ui create begin");
#if defined(__LINUX__) || defined(__WINDOWS__) || defined(__SWITCH__)
    pemu_ui = new PEMUUiMain(Vector2f{1280, 720});
#else
    pemu_ui = new PEMUUiMain(Vector2f{0, 0});
#endif
#if defined(PEMU_NATIVE_SDL_TRACE)
    if (native_startup) {
        const bool ready = boot_renderer_ready(pemu_ui);
        SDL_LogSetOutputFunction(previous_sdl_log, previous_sdl_log_data);
        if (!ready) {
            boot_mark("frontend stopped: renderer unavailable; skin not created");
            // A partial renderer's destructor may also call missing GL entry
            // points. Leave these startup objects for process-exit cleanup.
            return 1;
        }
    }
#endif
    boot_mark("ui create ok");
    pemu_ui->setIo(io);
    boot_mark("io attach ok");

    // load configuration
    boot_mark("config create begin");
    constexpr int version = (__PEMU_VERSION_MAJOR__ * 100) + __PEMU_VERSION_MINOR__;
    const auto cfg = new PEMUConfig(pemu_ui, version);
    pemu_ui->setConfig(cfg);
    boot_mark("config create ok");

    // load skin configuration
    boot_mark("skin create begin");
    const auto skin = new PEMUSkin(pemu_ui);
    pemu_ui->setSkin(skin);
    boot_mark("skin create ok");

    // parse command line
    if (argc > 1) {
        if (io->exist(argv[1])) {
            game.path = Utility::baseName(argv[1]);
            game.name = Utility::removeExt(game.path);
            game.romsPath = Utility::remove(argv[1], game.path);
        } else {
            printf("main: file provided as console argument does not exist (%s)\n", argv[1]);
            delete (skin);
            delete (cfg);
            delete (pemu_ui);
            return 1;
        }
    }

    // ui
    boot_mark("rom list create begin");
    const auto romList = new PEMURomList(pemu_ui, cfg->getCoreVersion(), cfg->getCoreSupportedExt());
    if (game.path.empty()) {
        romList->build();
        romList->initFav();
    } else {
        delete (romList->rect);
    }
    const auto uiRomList = new PEMUUiRomList(pemu_ui, romList, pemu_ui->getSize());
    const auto uiMenu = new PEMUUiMenu(pemu_ui);
    const auto uiEmu = new PEMUUiEmu(pemu_ui);
    const auto uiState = new PEMUUiMenuState(pemu_ui);
    pemu_ui->init(uiRomList, uiMenu, uiEmu, uiState);
    boot_mark("ui init ok");

    // load specified game from command line if requested
    if (!game.path.empty()) {
        uiRomList->setVisibility(Visibility::Hidden);
        uiRomList->setGames({game});
        cfg->loadGame(game);
        uiEmu->setExitOnStop(true);
        uiEmu->load(game);
    }

    boot_mark("main loop begin");
    while (!pemu_ui->done) {
#if defined(__PS5__) || defined(__PROSPERO__)
        if (pemu_native_input_probe) pemu_native_input_probe(0);
#endif
        pemu_ui->flip();
#if defined(__PS5__) || defined(__PROSPERO__)
        if (pemu_native_input_probe) pemu_native_input_probe(1);
#endif
    }

    delete (skin);
    delete (cfg);
    delete (pemu_ui);

#ifdef  __PS4__
    sceSystemServiceLoadExec((char *) "exit", nullptr);
    while (true) {
    }
#endif

    return 0;
}
