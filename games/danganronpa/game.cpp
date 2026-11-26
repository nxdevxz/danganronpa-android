
#include <nx/game.h>
#include "profile.h"
#include <nx/runtime.h>
#include <cstdlib>
#include <atomic>
#include <cstring>

namespace nx {

const game::Info game::kInfo = {
    "danganronpa",
    "Danganronpa THH",
    danganronpa::profile::program_id,
    "io.github.nxdevxz.danganronpa",
    "private/danganronpa",
    0,
    danganronpa::profile::base_build,
    danganronpa::profile::update_build,
    danganronpa::profile::usa_program_id
};

#if defined(__ANDROID__)
namespace {
uint64_t original_sniff;
std::atomic<bool> rejected{false};

void probe_opening_movie(CPU& c) {
    if (!rejected.exchange(true)) {
        log("movie probe: rejecting first stream before format detection");
        c.x[0] = 0;  // DataSource::sniff returns bool.
        return;
    }
    call(c, original_sniff);
}
}
#endif

void game::after_init() {

#if defined(__ANDROID__)
    auto default_setting = [](const char* name, const char* value) {
        if (!std::getenv(name)) setenv(name, value, 0);
    };
    if (const char* flag = std::getenv("NX_PROBE_OPENING_MOVIE"); flag && std::strcmp(flag, "1") == 0) {
        const auto addr = find_symbol("_ZN7android10DataSource5sniffEPNS_7String8EPfPNS_2spINS_8AMessageEEE");
        if (addr) original_sniff = hook_function(addr, probe_opening_movie);
        log("movie probe: %s", original_sniff ? "armed" : "stream detector unavailable");
    }
    // The US release ships English and Japanese text.
    default_setting("NX_GAME_LANGUAGES", "en-us ja");
    // Capture a bounded startup draw trace when the player enables diagnostics.
    if (const char* trace = std::getenv("NX_TRACE_PERF"); trace && trace[0] == '1') {
        default_setting("NX_TRACE_DRAWS", "0");
        default_setting("NX_TRACE_COUNT", "24");
        // Correlate the loading scene's asset requests with shader pairs that
        // actually cover pixels while the window is black.
        default_setting("NX_TRACE_FS", "1");
        default_setting("NX_ZERO_DRAWS", "1");
    }
    default_setting("NX_PIPE_PREWARM", "0");
    default_setting("NX_AFFINITY", "0");
    // The game asks for 16x anisotropic filtering on its ground and walls; on phone GPUs (Adreno 6xx, Mali-G5x/G7x)
    // every pixel of the 3D pass pays for it. 4x is hard to tell apart at 720p and saves fragment/texture time.
    // NX_MAX_ANISO=16 in config.txt restores the game's own value.
    default_setting("NX_MAX_ANISO", "4");
    // 0.4.5 bounded vertex uploads scan every index of every indexed draw on the replay thread; in the logs it trimmed
    // 0 bindings ("bound" always equals "needed"), so it only cost CPU. 0.4.2 did not have it. NX_VERTEX_UPLOAD_RANGES=1
    // turns it back on.
    default_setting("NX_VERTEX_UPLOAD_RANGES", "0");
#endif

    log("danganronpa: runtime initialized");
    log("danganronpa: beginning game-specific initialization");
}

void game::renderer_start() {
    log("danganronpa: renderer started");
}

void game::first_frame() {
    log("danganronpa: first frame presented");
}

} // namespace nx
