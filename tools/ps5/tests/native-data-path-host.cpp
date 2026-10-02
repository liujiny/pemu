/* Exercise the production PFBAIo constructor and POSIX filesystem code.
 * Only the native mount provider and emulator lifecycle are substituted.
 * Build again with PEMU_TEST_NO_NATIVE_ROOT to leave the weak helper absent.
 */
#define __PS5__ 1
#define __LINUX__ 1
// Use the real IO headers without bringing in the aggregate rendering header.
#define __C2D_H__ 1
#define C2DIo POSIXIo
#define MAX_PATH 512

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "cross2d/platforms/posix/posix_io.h"
using namespace c2d;
// burner.h mentions this unused display-interface pointer type.
struct RECT;
#include "pfbneo_io.h"

// Compile the actual implementations, including the real BurnPathsInit.
#include "../../../external/libcross2d/source/platforms/posix/posix_io.cpp"
#include "../../../external/libcross2d/source/skeleton/utility.cpp"
#include "../../../src/cores/pfbneo/sources/fbneo/paths.cpp"

namespace fs = std::filesystem;
static std::string title_root;
static std::string expected_path;
static unsigned getcwd_calls, init_calls, exit_calls;

#ifndef PEMU_TEST_NO_NATIVE_ROOT
extern "C" const char *pemu_native_data_path()
{
    return title_root.c_str();
}
#endif

extern "C" char *__wrap_getcwd(char *, size_t)
{
    ++getcwd_calls;
    errno = EPERM;
    return nullptr;
}

static void assert_core_paths()
{
    assert(std::string(szAppHomePath) == expected_path);
    assert(std::string(szAppRomPath) == expected_path + "arcade/");
    assert(std::string(szAppConfigPath) == expected_path + "configs/");
    assert(std::string(szAppSavePath) == expected_path + "saves/");
    assert(std::string(szAppSamplesPath) == expected_path + "samples/");
    assert(std::string(szAppIconPath) == expected_path + "icons/");
    assert(std::string(szAppBlendPath) == expected_path + "blend/");
    assert(std::string(szAppHDDPath) == expected_path + "hdd/");
    assert(std::string(szAppEEPROMPath) == expected_path + "eeproms/");
    assert(std::string(szAppHiscorePath) == expected_path + "hiscores/");
}

extern "C" INT32 BurnLibInit()
{
    ++init_calls;
    // Called by the production constructor immediately after BurnPathsInit.
    assert_core_paths();
#ifndef PEMU_TEST_NO_NATIVE_ROOT
    assert(getcwd_calls == 0);
#else
    assert(getcwd_calls > 0);
#endif
    return 0;
}

extern "C" INT32 BurnLibExit()
{
    ++exit_calls;
    return 0;
}

static void write_fixture(const fs::path &path, const char *contents)
{
    fs::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary);
    stream << contents;
    assert(stream.good());
}

static std::string read_file(const fs::path &path)
{
    std::ifstream stream(path, std::ios::binary);
    assert(stream.good());
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

int main()
{
    const int original_cwd = open(".", O_RDONLY | O_DIRECTORY);
    assert(original_cwd >= 0);
    char temporary[] = "/tmp/pemu-native-path-XXXXXX";
    assert(mkdtemp(temporary));
    const fs::path sandbox(temporary);
    const fs::path mount = sandbox / "mock-app0";
    const fs::path other = sandbox / "unrelated-cwd";
    fs::create_directories(mount);
    fs::create_directories(other);
    title_root = mount.string() + "/";
#ifdef PEMU_TEST_NO_NATIVE_ROOT
    assert(!pemu_native_data_path);
    expected_path = "./";
    const fs::path active = other;
#else
    expected_path = title_root;
    const fs::path active = mount;
#endif
    write_fixture(active / "data_romfs/hiscores/hiscore.dat", "hiscore fixture\n");
    write_fixture(active / "data_romfs/skins/default/config.cfg", "skin fixture\n");
    write_fixture(active / "data_romfs/skins/default/default.ttf", "font fixture\n");
    assert(chdir(other.c_str()) == 0);
    struct stat before{}, after{};
    assert(stat(".", &before) == 0);
    getcwd_calls = 0;
    {
        PFBAIo io;
        assert(init_calls == 1 && exit_calls == 0);
        assert(io.getDataPath() == expected_path);
        assert(io.getRomFsPath() == expected_path + "data_romfs/");
        assert_core_paths();
        for (const char *name : {"configs", "saves", "arcade", "samples", "icons",
                                 "blend", "hdd", "eeproms", "hiscores"})
            assert(fs::is_directory(active / name));
        assert(read_file(active / "hiscores/hiscore.dat") == "hiscore fixture\n");
        assert(io.exist(io.getRomFsPath() + "skins/default/config.cfg"));
        assert(io.exist(io.getRomFsPath() + "skins/default/default.ttf"));
        assert(io.write(io.getDataPath() + "configs/probe.cfg", "config", 6));
        assert(io.write(io.getDataPath() + "saves/probe.sav", "save", 4));
        assert(read_file(active / "configs/probe.cfg") == "config");
        assert(read_file(active / "saves/probe.sav") == "save");
        assert(stat(".", &after) == 0);
        assert(before.st_dev == after.st_dev && before.st_ino == after.st_ino);
#ifndef PEMU_TEST_NO_NATIVE_ROOT
        assert(getcwd_calls == 0);
        assert(!fs::exists(other / "configs") && !fs::exists(other / "saves"));
        std::puts("PASS native root: real PFBAIo/POSIXIo/BurnPathsInit; getcwd calls=0; absolute paths, resources, copy/write and unchanged cwd");
#else
        assert(getcwd_calls > 0);
        assert(!fs::exists(mount / "configs") && !fs::exists(mount / "saves"));
        std::printf("PASS absent weak helper: existing ./ fallback retained with getcwd=EPERM (%u calls)\n", getcwd_calls);
#endif
    }
    assert(exit_calls == 1);
    assert(fchdir(original_cwd) == 0);
    assert(close(original_cwd) == 0);
    fs::remove_all(sandbox);
}
