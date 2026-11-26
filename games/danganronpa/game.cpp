
#include <nx/game.h>
#include "profile.h"
#include <nx/runtime.h>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <array>
#include <algorithm>
#include <atomic>
#include <utility>
#include <mutex>
#include <vector>

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
uint64_t original_stream_open;
uint64_t original_component_open;
uint64_t original_decoder_ctor;

// ---- Software H.264 decoder: colour format bridge ----------------------------------------------------------
// The game's movie::Decoder asks the H.264 component for NV12. Google's software decoder only offers planar
// I420 and answers OMX_ErrorUnsupportedSetting (Decoder::Configure -> 0x1006), so nothing is ever decoded.
// The component is wrapped: a non-I420 output colour format is replaced by I420 in SetParameter, the caller is
// still told the format it asked for in GetParameter, and every decoded picture is turned into NV12 in place
// before the client's FillBufferDone sees it.
constexpr uint32_t kIndexPortDefinition = 0x02000001, kIndexPortFormat = 0x06000001;
constexpr uint32_t kColorI420 = 19;

struct SoftComponent {
    uint64_t handle = 0;
    uint32_t want_color = 0;       // what the client tried to set on the output port
    int32_t width = 0, height = 0, stride = 0, slice = 0;  // the output port as the decoder reports it
};
std::mutex g_soft_lock;
std::vector<SoftComponent> g_soft;
uint64_t g_set_orig, g_get_orig, g_fill_orig, g_event_orig, g_empty_orig;
uint64_t g_set_thunk, g_get_thunk, g_fill_thunk, g_event_thunk, g_empty_thunk;
std::atomic<unsigned> g_omx_log{0};

SoftComponent* soft_find(uint64_t handle) {
    for (auto& k : g_soft) if (k.handle == handle) return &k;
    return nullptr;
}

template <class T> T& field(uint64_t base, size_t off) { return *reinterpret_cast<T*>(base + off); }

// The component's own idea of a port definition (what GetParameter would answer), read on a scratch copy.
bool soft_query(CPU& c, uint64_t handle, uint32_t index, uint64_t params, uint8_t* scratch, size_t cap) {
    const size_t size = std::min<size_t>(field<uint32_t>(params, 0), cap);
    if (size < 96) return false;
    std::memcpy(scratch, reinterpret_cast<void*>(params), size);
    const uint64_t s0 = c.x[0], s1 = c.x[1], s2 = c.x[2];
    c.x[0] = handle, c.x[1] = index, c.x[2] = reinterpret_cast<uint64_t>(scratch);
    call(c, g_get_orig);
    const bool ok = static_cast<uint32_t>(c.x[0]) == 0;
    c.x[0] = s0, c.x[1] = s1, c.x[2] = s2;
    return ok;
}

// OMX_ERRORTYPE SetParameter(OMX_HANDLETYPE, OMX_INDEXTYPE, OMX_PTR)
void soft_set_parameter(CPU& c) {
    const uint64_t handle = c.x[0], params = c.x[2];
    const uint32_t index = static_cast<uint32_t>(c.x[1]);
    uint64_t patched = 0, count_at = 0;
    uint32_t before = 0, count_before = 0, port = 0, actual = 0, min_count = 0, size = 0;
    int32_t width = 0, height = 0;
    bool is_definition = false;
    if (params) {
        bool known;
        {
            std::lock_guard lk(g_soft_lock);
            known = soft_find(handle) != nullptr;
        }
        if (known) {
            if (index == kIndexPortDefinition && field<uint32_t>(params, 36) == 1) {
                is_definition = true;
                port = field<uint32_t>(params, 8), actual = field<uint32_t>(params, 16);
                min_count = field<uint32_t>(params, 20), size = field<uint32_t>(params, 24);
                width = field<int32_t>(params, 56), height = field<int32_t>(params, 60);
                // The software decoder refuses (OMX_ErrorUnsupportedSetting) fewer buffers than it needs.
                uint8_t scratch[256];
                if (soft_query(c, handle, index, params, scratch, sizeof scratch))
                    min_count = std::max(min_count, field<uint32_t>(reinterpret_cast<uint64_t>(scratch), 20));
                if (actual < min_count) {
                    count_at = params + 16, count_before = actual;
                    *reinterpret_cast<uint32_t*>(count_at) = min_count;
                }
            }
            std::lock_guard lk(g_soft_lock);
            if (SoftComponent* k = soft_find(handle)) {
                if (index == kIndexPortFormat && field<uint32_t>(params, 8) == 1 &&
                    field<uint32_t>(params, 20) != kColorI420) {
                    patched = params + 20;
                } else if (is_definition && port == 1 && field<uint32_t>(params, 88) != kColorI420) {
                    patched = params + 88;
                }
                if (patched) {
                    before = *reinterpret_cast<uint32_t*>(patched);
                    k->want_color = before;
                    *reinterpret_cast<uint32_t*>(patched) = kColorI420;
                }
            }
        }
    }
    call(c, g_set_orig);
    const uint32_t result = static_cast<uint32_t>(c.x[0]);
    if (patched) *reinterpret_cast<uint32_t*>(patched) = before;
    if (count_at) *reinterpret_cast<uint32_t*>(count_at) = count_before;
    static std::atomic<unsigned> errors{0};
    if (g_omx_log.fetch_add(1) < 80 || (result != 0 && errors.fetch_add(1) < 40)) {
        if (is_definition)
            log("movie: OMX SetParameter(%llx, port definition) port %u: %dx%d buffers %u (decoder needs %u%s), size %u%s -> 0x%x",
                static_cast<unsigned long long>(handle), port, width, height, actual,
                min_count, count_at ? ", raised" : "", size, patched ? ", colour -> I420" : "", result);
        else
            log("movie: OMX SetParameter(%llx, index 0x%x)%s -> 0x%x", static_cast<unsigned long long>(handle), index,
                patched ? " [colour format changed to I420]" : "", result);
    }
}

// OMX_ERRORTYPE GetParameter(OMX_HANDLETYPE, OMX_INDEXTYPE, OMX_PTR)
void soft_get_parameter(CPU& c) {
    const uint64_t handle = c.x[0], params = c.x[2];
    const uint32_t index = static_cast<uint32_t>(c.x[1]);
    call(c, g_get_orig);
    const uint32_t result = static_cast<uint32_t>(c.x[0]);
    if (result == 0 && params) {
        std::lock_guard lk(g_soft_lock);
        if (SoftComponent* k = soft_find(handle)) {
            if (index == kIndexPortFormat && field<uint32_t>(params, 8) == 1 && k->want_color)
                field<uint32_t>(params, 20) = k->want_color;
            if (index == kIndexPortDefinition && field<uint32_t>(params, 8) == 1 &&
                field<uint32_t>(params, 36) == 1) {
                k->width = field<int32_t>(params, 56), k->height = field<int32_t>(params, 60);
                k->stride = field<int32_t>(params, 64), k->slice = field<int32_t>(params, 68);
                if (k->want_color) field<uint32_t>(params, 88) = k->want_color;
                static std::atomic<unsigned> said{0};
                if (said.fetch_add(1) < 12)
                    log("movie: OMX output port %dx%d stride %d slice %d buffer %u", k->width, k->height, k->stride,
                        k->slice, field<uint32_t>(params, 24));
            }
        }
    }
    if (g_omx_log.fetch_add(1) < 80)
        log("movie: OMX GetParameter(%llx, index 0x%x) -> 0x%x", static_cast<unsigned long long>(handle), index, result);
}

// Planar I420 in one buffer to NV12 in the same buffer (stride x slice luma, then the chroma).
void i420_to_nv12(uint8_t* p, int stride, int slice) {
    const size_t y = size_t(stride) * slice, half = size_t(stride) / 2, rows = size_t(slice) / 2;
    static thread_local std::vector<uint8_t> tmp;
    tmp.assign(p + y, p + y + half * rows * 2);
    const uint8_t* u = tmp.data();
    const uint8_t* v = u + half * rows;
    uint8_t* d = p + y;
    for (size_t r = 0; r < rows; r++)
        for (size_t x = 0; x < half; x++) {
            d[r * stride + 2 * x] = u[r * half + x];
            d[r * stride + 2 * x + 1] = v[r * half + x];
        }
}

// OMX_ERRORTYPE FillBufferDone(OMX_HANDLETYPE, OMX_PTR, OMX_BUFFERHEADERTYPE*)
void soft_fill_buffer_done(CPU& c) {
    const uint64_t handle = c.x[0], header = c.x[2];
    static std::atomic<unsigned> frames{0};
    if (header && field<uint32_t>(header, 100) == 1) {
        const uint32_t filled = field<uint32_t>(header, 20), offset = field<uint32_t>(header, 24);
        const uint64_t buffer = field<uint64_t>(header, 8);
        int stride = 0, slice = 0;
        {
            std::lock_guard lk(g_soft_lock);
            if (SoftComponent* k = soft_find(handle)) {
                stride = k->stride > 0 ? k->stride : k->width;
                slice = k->slice > 0 ? k->slice : k->height;
            }
        }
        const unsigned n = frames.fetch_add(1);
        const bool fits = stride > 0 && slice > 0 && (stride % 2) == 0 && (slice % 2) == 0 &&
                          filled == static_cast<uint32_t>(stride) * slice * 3 / 2;
        if (n < 8 || n % 120 == 0)
            log("movie: decoded picture #%u: %u bytes at +%u, pts %lld us, flags 0x%x, layout %dx%d %s", n, filled,
                offset, static_cast<long long>(field<int64_t>(header, 88)), field<uint32_t>(header, 96), stride,
                slice, fits ? "-> NV12" : "NOT converted");
        if (fits && buffer) i420_to_nv12(reinterpret_cast<uint8_t*>(buffer + offset), stride, slice);
    }
    call(c, g_fill_orig);
}

// OMX_ERRORTYPE EventHandler(OMX_HANDLETYPE, OMX_PTR, OMX_EVENTTYPE, OMX_U32, OMX_U32, OMX_PTR)
void soft_event_handler(CPU& c) {
    static std::atomic<unsigned> n{0};
    if (n.fetch_add(1) < 60)
        log("movie: OMX event %u (data %llx, %llx) from %llx", static_cast<unsigned>(c.x[2]),
            static_cast<unsigned long long>(c.x[3] & 0xFFFFFFFFull), static_cast<unsigned long long>(c.x[4] & 0xFFFFFFFFull),
            static_cast<unsigned long long>(c.x[0]));
    call(c, g_event_orig);
}

// The decoder wants an Annex B byte stream (00 00 00 01 before every NAL). MP4 carries length-prefixed NALs and
// the parameter sets without start codes; four-byte lengths are rewritten in place, a lone SPS/PPS gets a prefix.
const char* to_annex_b(uint64_t header, uint8_t* d, uint32_t len) {
    if (len >= 4 && d[0] == 0 && d[1] == 0 && d[2] == 0 && d[3] == 1) return "annex b";
    if (len >= 3 && d[0] == 0 && d[1] == 0 && d[2] == 1) return "annex b (3)";
    uint32_t pos = 0;
    bool lengths = len > 4;
    while (lengths && pos < len) {
        if (pos + 4 > len) { lengths = false; break; }
        const uint32_t n = (uint32_t(d[pos]) << 24) | (uint32_t(d[pos + 1]) << 16) | (uint32_t(d[pos + 2]) << 8) | d[pos + 3];
        if (n == 0 || n > len - pos - 4) { lengths = false; break; }
        pos += 4 + n;
    }
    if (lengths && pos == len) {
        for (pos = 0; pos < len;) {
            const uint32_t n = (uint32_t(d[pos]) << 24) | (uint32_t(d[pos + 1]) << 16) | (uint32_t(d[pos + 2]) << 8) | d[pos + 3];
            d[pos] = d[pos + 1] = d[pos + 2] = 0, d[pos + 3] = 1;
            pos += 4 + n;
        }
        return "length-prefixed -> annex b";
    }
    const uint32_t type = len ? (d[0] & 0x1f) : 0;
    const uint32_t room = field<uint32_t>(header, 16) - field<uint32_t>(header, 24);
    if ((type == 7 || type == 8) && len + 4 <= room) {
        std::memmove(d + 4, d, len);
        d[0] = d[1] = d[2] = 0, d[3] = 1;
        field<uint32_t>(header, 20) = len + 4;
        return "parameter set + start code";
    }
    return "unknown layout";
}

// OMX_ERRORTYPE EmptyThisBuffer(OMX_HANDLETYPE, OMX_BUFFERHEADERTYPE*)
void soft_empty_this_buffer(CPU& c) {
    const uint64_t header = c.x[1];
    static std::atomic<unsigned> n{0};
    if (header && field<uint32_t>(header, 104) == 0) {
        auto* d = reinterpret_cast<uint8_t*>(field<uint64_t>(header, 8) + field<uint32_t>(header, 24));
        const uint32_t len = field<uint32_t>(header, 20), flags = field<uint32_t>(header, 96);
        const unsigned k = n.fetch_add(1);
        char hex[64] = "";
        for (uint32_t i = 0; i < len && i < 16; i++) std::snprintf(hex + 3 * i, sizeof hex - 3 * i, "%02x ", d[i]);
        const char* how = len ? to_annex_b(header, d, len) : "empty";
        if (k < 14 || k % 200 == 0)
            log("movie: H.264 input #%u: %u bytes, flags 0x%x, pts %lld us, starts %s-> %s", k, len, flags,
                static_cast<long long>(field<int64_t>(header, 88)), hex, how);
    }
    call(c, g_empty_orig);
}

void open_movie_component(CPU& c) {
    const char* name = reinterpret_cast<const char*>(c.x[1]);
    char requested[96] = "(null)";
    if (name) std::snprintf(requested, sizeof requested, "%s", name);
    const uint64_t out = c.x[4];
    const bool swap = name && std::strcmp(name, "OMX.Nvidia.h264.decode") == 0;
    if (swap) {
        static const char software[] = "OMX.google.h264.decoder";
        c.x[1] = reinterpret_cast<uint64_t>(software);
        log("movie: selecting software H.264 decoder");
        // The client's FillBufferDone gets wrapped, through a private copy of the callback table.
        const uint64_t table = c.x[2];
        if (table) {
            auto* copy = new uint64_t[3];
            for (int k = 0; k < 3; k++) copy[k] = reinterpret_cast<uint64_t*>(table)[k];
            if (!g_fill_thunk) g_fill_thunk = hle_address_or("danganronpa_omx_fill_buffer_done", soft_fill_buffer_done);
            if (!g_event_thunk) g_event_thunk = hle_address_or("danganronpa_omx_event", soft_event_handler);
            g_fill_orig = copy[2], g_event_orig = copy[0];
            copy[2] = g_fill_thunk, copy[0] = g_event_thunk;
            c.x[2] = reinterpret_cast<uint64_t>(copy);
        }
    }
    call(c, original_component_open);
    const int32_t result = static_cast<int32_t>(c.x[0]);
    const uint64_t component = (result == 0 && out) ? *reinterpret_cast<uint64_t*>(out) : 0;
    if (swap && component) {
        auto* tbl = reinterpret_cast<uint64_t*>(component);  // OMX_COMPONENTTYPE: GetParameter +40, SetParameter +48
        if (!g_set_thunk) {
            g_get_thunk = hle_address_or("danganronpa_omx_get_parameter", soft_get_parameter);
            g_set_thunk = hle_address_or("danganronpa_omx_set_parameter", soft_set_parameter);
            g_empty_thunk = hle_address_or("danganronpa_omx_empty_this_buffer", soft_empty_this_buffer);
        }
        {
            std::lock_guard lk(g_soft_lock);
            if (!soft_find(component)) g_soft.push_back({component});
            else *soft_find(component) = SoftComponent{component};
        }
        if (tbl[5] != g_get_thunk) g_get_orig = tbl[5], tbl[5] = g_get_thunk;
        if (tbl[6] != g_set_thunk) g_set_orig = tbl[6], tbl[6] = g_set_thunk;
        if (tbl[15] != g_empty_thunk) g_empty_orig = tbl[15], tbl[15] = g_empty_thunk;  // EmptyThisBuffer +120
    }
    log("movie: OMX component '%s'%s -> result %d (0x%x), component %llx",
        requested, swap ? " (software swap)" : "", result, static_cast<unsigned>(result),
        static_cast<unsigned long long>(component));
}

// ---- movie::Decoder trace -------------------------------------------------------------------------------
// The game drives subsdk0's own movie::Decoder. Its vtable slots (same order as the P5R HLE) are hooked once,
// after the first construction, so the log shows how far the video gets and what each call returns.
constexpr int kSlots = 19;
const char* const kSlotName[kSlots] = {
    "~Decoder", "~Decoder(deleting)", "Configure", "RegisterClientEvents", "GetOutputFormat", "Start", "Stop",
    "Reset", "Flush", "AcquireInputBufferIndex", "GetInputBuffer", "SendInputBufferForDecode",
    "AcquireOutputBufferIndex", "GetOutputBuffer", "GetOutputBufferSize", "ReleaseOutputBufferIndex",
    "GetLastError", "GetOutputBuffer(format)", "GetOutputBufferFormat"};
uint64_t g_slot_original[kSlots];
std::atomic<unsigned> g_slot_count[kSlots];

template <int N>
void trace_decoder_slot(CPU& c) {
    const uint64_t self = c.x[0], a1 = c.x[1], a2 = c.x[2], a3 = c.x[3], a4 = c.x[4], a5 = c.x[5];
    call(c, g_slot_original[N]);
    const int32_t result = static_cast<int32_t>(c.x[0]);
    const unsigned n = g_slot_count[N].fetch_add(1, std::memory_order_relaxed);
    if (n >= 30 && n % 300 != 0) return;
    char extra[160] = "";
    if (result == 0) {
        if (N == 9 && a1 && a2)
            std::snprintf(extra, sizeof extra, " [index %d more %d]",
                          *reinterpret_cast<int32_t*>(a1), *reinterpret_cast<int32_t*>(a2));
        if (N == 12 && a1 && a2 && a3 && a4)
            std::snprintf(extra, sizeof extra, " [index %d pts %lld flags %x more %d]",
                          *reinterpret_cast<int32_t*>(a1), static_cast<long long>(*reinterpret_cast<int64_t*>(a2)),
                          *reinterpret_cast<uint32_t*>(a3), *reinterpret_cast<int32_t*>(a4));
        if (N == 14 && a1 && a2)
            std::snprintf(extra, sizeof extra, " [size %d %d]",
                          *reinterpret_cast<int32_t*>(a1), *reinterpret_cast<int32_t*>(a2));
    }
    log("movie: Decoder %llx ::%s(%llx, %llx, %llx, %llx, %llx) #%u -> %d (0x%x)%s",
        static_cast<unsigned long long>(self), kSlotName[N], static_cast<unsigned long long>(a1),
        static_cast<unsigned long long>(a2), static_cast<unsigned long long>(a3),
        static_cast<unsigned long long>(a4), static_cast<unsigned long long>(a5), n, result,
        static_cast<unsigned>(result), extra);
}

template <int... I>
constexpr std::array<HleFn, sizeof...(I)> make_trace(std::integer_sequence<int, I...>) {
    return {{trace_decoder_slot<I>...}};
}
const std::array<HleFn, kSlots> kTrace = make_trace(std::make_integer_sequence<int, kSlots>{});

void open_movie_decoder(CPU& c) {
    const uint64_t self = c.x[0];
    log("movie: Decoder ctor type %d format %d mode %d priority %d (object %llx)",
        static_cast<int>(c.x[1]), static_cast<int>(c.x[2]), static_cast<int>(c.x[3]),
        static_cast<int>(c.x[4]), static_cast<unsigned long long>(self));
    call(c, original_decoder_ctor);
    static std::atomic<bool> hooked{false};
    const uint64_t vtable = self ? *reinterpret_cast<uint64_t*>(self) : 0;
    if (vtable && !hooked.exchange(true)) {
        int ok = 0;
        for (int k = 2; k < kSlots; k++) {
            const uint64_t entry = *reinterpret_cast<uint64_t*>(vtable + 8u * k);
            g_slot_original[k] = entry ? hook_function(entry, kTrace[k]) : 0;
            if (g_slot_original[k]) ok++;
            else log("movie: Decoder slot %d (%s) at %llx not hooked", k, kSlotName[k],
                     static_cast<unsigned long long>(entry));
        }
        log("movie: Decoder vtable %llx traced (%d of %d slots)", static_cast<unsigned long long>(vtable),
            ok, kSlots - 2);
    }
}

void open_local_movie_stream(CPU& c) {
    // CreateCacheConfig mode 0 selects the guest's synchronous FileSource.
    c.x[3] = 0;
    call(c, original_stream_open);
    log("movie: direct stream open returned %d", static_cast<int32_t>(c.x[0]));
}
}
#endif

void game::after_init() {

#if defined(__ANDROID__)
    auto default_setting = [](const char* name, const char* value) {
        if (!std::getenv(name)) setenv(name, value, 0);
    };
    const auto component = find_symbol("_ZN7android9OMXMaster21makeComponentInstanceEPKcPK16OMX_CALLBACKTYPEPvPP17OMX_COMPONENTTYPE");
    if (component) original_component_open = hook_function(component, open_movie_component);
    log("movie: software H.264 selection %s", original_component_open ? "enabled" : "unavailable");
    const auto open_stream = find_symbol("_ZN7android16NuMediaExtractor19setDataSourceStreamEPN5movie18ClientStreamReaderEPKci");
    if (open_stream) original_stream_open = hook_function(open_stream, open_local_movie_stream);
    log("movie: direct local stream reader %s", original_stream_open ? "enabled" : "unavailable");
    const auto decoder = find_symbol("_ZN5movie7DecoderC1ENS_11DecoderTypeENS_19DecoderOutputFormatENS_11DecoderModeEi");
    if (decoder) original_decoder_ctor = hook_function(decoder, open_movie_decoder);
    log("movie: decoder trace %s", original_decoder_ctor ? "enabled" : "unavailable");
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
