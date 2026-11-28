#include "game_image.h"

#include <fcntl.h>
#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <cstring>
#include <memory>
#include <mutex>
#include <vector>

#include <nx/runtime.h>
#include <nx/game.h>
#include "platform.h"

namespace nx {

namespace {

std::unique_ptr<nxi::GameImage> g_image;
std::once_flag g_once;

int open_read(const char* path) {
#if defined(_WIN32)
    return _open(path, _O_RDONLY | _O_BINARY | _O_NOINHERIT);
#else
    return ::open(path, O_RDONLY | O_CLOEXEC);
#endif
}

void close_fd(int fd) {
#if defined(_WIN32)
    _close(fd);
#else
    ::close(fd);
#endif
}

std::string base_name(const std::string& p) {
    size_t s = p.find_last_of("/\\");
    return s == std::string::npos ? p : p.substr(s + 1);
}

void open_image() {
    if (game_files_root() != plat::data_root()) {
        log("game: using installed data at %s (NSP/XCI not required)", game_files_root().c_str());
        return;
    }
    const char* fd = std::getenv("NX_GAME_FD");
    const char* path = std::getenv("NX_GAME_NSP");
    if (!(fd && *fd) && !(path && *path)) return;
    std::vector<nxi::InputFile> in;
    std::string shown;
    if (fd && *fd) {
        const char* name = std::getenv("NX_GAME_NAME");
        shown = name && *name ? name : "game.nsp";
        in.push_back({shown, std::atoi(fd)});
        if (const char* update = std::getenv("NX_GAME_UPDATE_FD")) {
            const char* update_name = std::getenv("NX_GAME_UPDATE_NAME");
            in.push_back({update_name ? update_name : "update.nsp", std::atoi(update)});
        }
    } else {
        shown = base_name(path);
        int f = open_read(path);
        if (f < 0) fatal("game: cannot open %s", path);
        in.push_back({shown, f});
    }
    std::vector<int> keys;  // read while opening, then closed
    const char* k = std::getenv("NX_KEYS");
    if (!k || !*k) fatal("game: NX_KEYS (the prod.keys file) is not set");
    if ((keys.emplace_back(open_read(k))) < 0) fatal("game: cannot open the keys file");
    in.push_back({"prod.keys", keys.back()});
    if (const char* t = std::getenv("NX_TITLE_KEYS"); t && *t) {
        int f = open_read(t);
        if (f >= 0) keys.push_back(f), in.push_back({"title.keys", f});
    }
    std::string err;
    g_image = nxi::GameImage::open(in, game::kInfo.program_id, err, game::kInfo.alternate_program_id);
    for (int f : keys) close_fd(f);
    if (!g_image) fatal("game: %s could not be read: %s", shown.c_str(), err.c_str());
    log("game: %s read in place (title %016llx): %zu RomFS files, ExeFS %zu files", shown.c_str(),
        (unsigned long long)g_image->title_id(), g_image->file_count(), g_image->exefs().size());
}

}  // namespace

std::string game_files_root() {
    static const bool installed = [] {
        const char* e = std::getenv("NX_DANGANRONPA_INSTALLED");
        return std::strcmp(game::kInfo.id, "danganronpa") == 0 && e && e[0] == '1';
    }();
    if (installed) {
        const char* root = std::getenv("NX_DANGANRONPA_GAME_ROOT");
        if (root && root[0] == '/') return root;  // Android's own external-files/game directory.
        return plat::data_root() + "/game";  // Existing PC/legacy installed-mode launchers.
    }
    return plat::data_root();
}

const nxi::GameImage* game_image() {
    std::call_once(g_once, open_image);
    return g_image.get();
}

uint64_t active_program_id() {
    static const uint64_t id = [] {
        if (const auto* image = game_image()) return image->title_id();
        if (game::kInfo.alternate_program_id && game_files_root() != plat::data_root()) {
            std::ifstream manifest(game_files_root() + "/installed.tsv");
            std::string format, title;
            std::ostringstream expected;
            // installed.tsv stores all 16 hex digits, including the leading zero.
            expected << "title\t" << std::hex << std::setfill('0') << std::setw(16) << game::kInfo.alternate_program_id;
            if (std::getline(manifest, format) && std::getline(manifest, title) && title == expected.str())
                return game::kInfo.alternate_program_id;
        }
        return game::kInfo.program_id;
    }();
    return id;
}

uint64_t process_system_resource_size() {
    static const uint64_t size = []() -> uint64_t {
        uint8_t header[0x18]{};
        bool available = false;
        if (const auto* image = game_image()) {
            const auto& files = image->exefs();
            auto it = files.find("main.npdm");
            if (it != files.end() && it->second.size() >= sizeof header) {
                std::memcpy(header, it->second.data(), sizeof header);
                available = true;
            }
        } else {
            std::ifstream npdm(game_files_root() + "/exefs/main.npdm", std::ios::binary);
            available = static_cast<bool>(npdm.read(reinterpret_cast<char*>(header), sizeof header));
        }
        if (!available || std::memcmp(header, "META", 4) != 0) {
            log("process: main.npdm header unavailable; using profile system resource size %llu",
                static_cast<unsigned long long>(game::kInfo.system_resource_size));
            return game::kInfo.system_resource_size;
        }
        const uint32_t bytes = uint32_t(header[0x14]) | (uint32_t(header[0x15]) << 8) |
                (uint32_t(header[0x16]) << 16) | (uint32_t(header[0x17]) << 24);
        log("process: main.npdm system resource size %u bytes", bytes);
        return bytes;
    }();
    return size;
}

std::string alternate_build_id() {
    if (!game::kInfo.alternate_program_id || active_program_id() != game::kInfo.alternate_program_id) return {};
    if (const auto* image = game_image()) return image->main_build_id();
    std::ifstream marker(game_files_root() + "/.import/usa-build");
    std::string id;
    std::getline(marker, id);
    return id;
}

std::string game_image_path(const std::string& path) {
    std::string out, part;
    auto flush = [&] {
        if (!part.empty() && part != ".") out += (out.empty() ? "" : "/") + part;
        part.clear();
    };
    for (char ch : path) {
        if (ch == '/' || ch == '\\') flush();
        else part += ch;
    }
    flush();
    return out;
}

}  // namespace nx
