
#include <nx/game.h>
#include "profile.h"
#include <nx/runtime.h>
#include <cstdlib>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <array>
#include <algorithm>
#include <atomic>
#include <utility>
#include <deque>
#include <memory>
#include <ctime>
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaFormat.h>
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
// The game holds every output buffer it was given until it has enough pictures queued; the software decoder
// offers only 2, the hardware one many more. More are announced so the picture queue can fill.
constexpr uint32_t kOutputBuffers = 8;

struct SoftComponent {
    uint64_t handle = 0;
    uint32_t want_color = 0;       // what the client tried to set on the output port
    int32_t width = 0, height = 0, stride = 0, slice = 0;  // the output port as the decoder reports it
};
std::mutex g_soft_lock;
std::vector<SoftComponent> g_soft;
uint64_t g_set_orig, g_get_orig, g_fill_orig, g_event_orig, g_empty_orig, g_send_orig, g_fill_this_orig, g_deinit_orig;
uint64_t g_set_thunk, g_get_thunk, g_fill_thunk, g_event_thunk, g_empty_thunk, g_send_thunk, g_fill_this_thunk,
    g_deinit_thunk, g_pump_thunk;
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
                if (field<uint32_t>(params, 16) < kOutputBuffers) field<uint32_t>(params, 16) = kOutputBuffers;
                if (field<uint32_t>(params, 20) < kOutputBuffers) field<uint32_t>(params, 20) = kOutputBuffers;
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

// ---- Host H.264 decoder behind the OMX component ----------------------------------------------------------
// Google's software H.264 decoder in the Switch library (the old On2 one) only handles Baseline; the game's
// video is Main profile with CABAC, so it fails on the first picture. The component stays (it does the port,
// buffer and state bookkeeping the client expects) but its data path is replaced: input goes to Android's
// MediaCodec, and decoded NV12 pictures are written into the client's output buffers from a guest thread.
struct VideoProxy {
    struct Input { std::vector<uint8_t> data; int64_t pts = 0; uint32_t flags = 0; };
    struct Plane { int32_t offset = 0, row = 0, col = 0; };
    uint64_t handle = 0, app_data = 0, empty_done = 0, fill_done = 0;
    AMediaCodec* codec = nullptr;
    bool failed = false;
    std::vector<uint8_t> config;                 // SPS/PPS (Annex B) from the codec-config buffers
    std::deque<Input> in;                        // pictures waiting for a MediaCodec input slot
    std::deque<uint64_t> give_back_in, free_out; // headers to return to the client / output headers it lent us
    bool return_out = false, flush_codec = false;
    int32_t color = 0, codec_stride = 0, codec_rows = 0, pic_w = 0, pic_h = 0, left = 0, top = 0;
    bool has_image = false, format_known = false;
    Plane plane[3];
    unsigned frames_in = 0, frames_out = 0, blocked_polls = 0, fills = 0, empties = 0;
    uint64_t last_beat_ns = 0;
    ~VideoProxy() {
        if (codec) { AMediaCodec_stop(codec); AMediaCodec_delete(codec); }
    }
};
std::mutex g_proxy_lock;
std::vector<std::shared_ptr<VideoProxy>> g_proxies;

std::shared_ptr<VideoProxy> proxy_find(uint64_t handle) {
    std::lock_guard lk(g_proxy_lock);
    for (auto& p : g_proxies) if (p->handle == handle) return p;
    return nullptr;
}

void proxy_read_format(VideoProxy& p, AMediaFormat* f) {
    p.color = p.codec_stride = p.codec_rows = p.left = p.top = p.pic_w = p.pic_h = 0;
    AMediaFormat_getInt32(f, "color-format", &p.color);
    AMediaFormat_getInt32(f, "stride", &p.codec_stride);
    AMediaFormat_getInt32(f, "slice-height", &p.codec_rows);
    int32_t w = 0, h = 0, l = 0, t = 0, r = 0, b = 0;
    AMediaFormat_getInt32(f, "width", &w);
    AMediaFormat_getInt32(f, "height", &h);
    p.pic_w = w, p.pic_h = h;
    if (AMediaFormat_getRect(f, "crop", &l, &t, &r, &b)) {
        p.left = l, p.top = t;
        if (r - l + 1 > 0 && b - t + 1 > 0) p.pic_w = r - l + 1, p.pic_h = b - t + 1;
    }
    // MediaImage2: type, planes, width, height, bit depth, allocated depth, then Y, U, V {offset, col inc, row inc, subsampling x2}
    p.has_image = false;
    void* data = nullptr;
    size_t n = 0;
    if (AMediaFormat_getBuffer(f, "image-data", &data, &n) && data && n >= 24 + 3 * 20) {
        uint32_t v[6 + 3 * 5];
        std::memcpy(v, data, sizeof v);
        bool ok = v[0] == 1 && v[1] == 3 && v[4] == 8 && v[5] == 8;
        for (int k = 0; k < 3 && ok; k++) {
            const uint32_t* q = v + 6 + 5 * k;
            const int32_t col = static_cast<int32_t>(q[1]), row = static_cast<int32_t>(q[2]);
            if (col < 1 || row < 1) ok = false;
            p.plane[k] = {static_cast<int32_t>(q[0]), row, col};
        }
        p.has_image = ok;
    }
    p.format_known = true;
    log("movie: host decoder output: color 0x%x, %dx%d (crop at %d,%d), stride %d, slice %d, image-data %d (%s)",
        p.color, p.pic_w, p.pic_h, p.left, p.top, p.codec_stride, p.codec_rows, p.has_image ? 1 : 0,
        AMediaFormat_toString(f));
}

// The decoder's picture into an NV12 output buffer of `stride` x `slice` (black where the picture does not reach).
void proxy_copy_picture(const VideoProxy& p, const uint8_t* src, size_t size, uint8_t* dst, int stride, int slice) {
    const size_t luma = size_t(stride) * slice;
    std::memset(dst, 16, luma);
    std::memset(dst + luma, 128, luma / 2);
    const int w = std::min(p.pic_w, stride) & ~1, h = std::min(p.pic_h, slice) & ~1;
    if (w <= 0 || h <= 0) return;
    uint8_t* uv = dst + luma;
    if (p.has_image) {
        auto reach = [&](const VideoProxy::Plane& q, int pw, int ph) {
            return int64_t(q.offset) + int64_t(ph - 1) * q.row + int64_t(pw - 1) * q.col < int64_t(size);
        };
        const VideoProxy::Plane &Y = p.plane[0], &U = p.plane[1], &V = p.plane[2];
        if (reach(Y, w, h) && reach(U, w / 2, h / 2) && reach(V, w / 2, h / 2)) {
            for (int r = 0; r < h; r++) {
                const uint8_t* s = src + Y.offset + size_t(r) * Y.row;
                if (Y.col == 1) std::memcpy(dst + size_t(r) * stride, s, size_t(w));
                else for (int x = 0; x < w; x++) dst[size_t(r) * stride + x] = s[size_t(x) * Y.col];
            }
            for (int r = 0; r < h / 2; r++) {
                const uint8_t* u = src + U.offset + size_t(r) * U.row;
                const uint8_t* v = src + V.offset + size_t(r) * V.row;
                for (int x = 0; x < w / 2; x++)
                    uv[size_t(r) * stride + 2 * x] = u[size_t(x) * U.col], uv[size_t(r) * stride + 2 * x + 1] = v[size_t(x) * V.col];
            }
            return;
        }
    }
    const int cs = p.codec_stride > 0 ? p.codec_stride : p.pic_w + p.left;
    const int rows = p.codec_rows > 0 ? p.codec_rows : p.pic_h + p.top;
    const size_t y0 = size_t(p.top) * cs + p.left, chroma = size_t(cs) * rows;
    for (int r = 0; r < h; r++) {
        const size_t at = y0 + size_t(r) * cs;
        if (at + w > size) return;
        std::memcpy(dst + size_t(r) * stride, src + at, size_t(w));
    }
    if (p.color == 19) {  // planar I420
        const size_t half = size_t(cs) / 2, plane = half * (size_t(rows) / 2);
        for (int r = 0; r < h / 2; r++) {
            const size_t at = chroma + size_t(p.top / 2 + r) * half + p.left / 2;
            if (at + plane + w / 2 > size) return;
            for (int x = 0; x < w / 2; x++)
                uv[size_t(r) * stride + 2 * x] = src[at + x], uv[size_t(r) * stride + 2 * x + 1] = src[at + plane + x];
        }
        return;
    }
    for (int r = 0; r < h / 2; r++) {  // semi-planar
        const size_t at = chroma + size_t(p.top / 2 + r) * cs + (p.left & ~1);
        if (at + w > size) return;
        std::memcpy(uv + size_t(r) * stride, src + at, size_t(w));
    }
}

bool proxy_open_codec(VideoProxy& p, int width, int height) {
    AMediaCodec* c = AMediaCodec_createDecoderByType("video/avc");
    if (!c) return log("movie: no host H.264 decoder"), false;
    AMediaFormat* f = AMediaFormat_new();
    AMediaFormat_setString(f, AMEDIAFORMAT_KEY_MIME, "video/avc");
    AMediaFormat_setInt32(f, AMEDIAFORMAT_KEY_WIDTH, width);
    AMediaFormat_setInt32(f, AMEDIAFORMAT_KEY_HEIGHT, height);
    AMediaFormat_setInt32(f, AMEDIAFORMAT_KEY_MAX_INPUT_SIZE, 1 << 20);
    AMediaFormat_setInt32(f, AMEDIAFORMAT_KEY_COLOR_FORMAT, 0x7F420888);  // COLOR_FormatYUV420Flexible
    if (!p.config.empty()) AMediaFormat_setBuffer(f, "csd-0", p.config.data(), p.config.size());
    media_status_t st = AMediaCodec_configure(c, f, nullptr, nullptr, 0);
    AMediaFormat_delete(f);
    if (st == AMEDIA_OK) st = AMediaCodec_start(c);
    if (st != AMEDIA_OK) {
        log("movie: host H.264 decoder refused %dx%d (%d)", width, height, static_cast<int>(st));
        AMediaCodec_delete(c);
        return false;
    }
    char* name = nullptr;
    if (AMediaCodec_getName(c, &name) == AMEDIA_OK && name) {
        log("movie: host H.264 decoder %s for %dx%d, %zu bytes of parameter sets", name, width, height, p.config.size());
        AMediaCodec_releaseName(c, name);
    }
    p.codec = c;
    return true;
}

// One round for one decoder: return headers, feed MediaCodec, hand finished pictures to the client.
void proxy_step(CPU& c, VideoProxy& p) {
    std::vector<std::pair<int, uint64_t>> calls;  // (0: EmptyBufferDone, 1: FillBufferDone, header)
    bool flush = false;
    {
        std::lock_guard lk(g_proxy_lock);
        for (uint64_t h : p.give_back_in) calls.push_back({0, h});
        p.give_back_in.clear();
        if (p.return_out) {
            for (uint64_t h : p.free_out) calls.push_back({1, h});
            p.free_out.clear();
            p.return_out = false;
        }
        flush = p.flush_codec;
        p.flush_codec = false;
    }
    if (flush && p.codec) AMediaCodec_flush(p.codec);
    for (auto& [kind, header] : calls) {
        if (kind == 1) field<uint32_t>(header, 20) = 0, field<uint32_t>(header, 96) = 0;
        c.x[0] = p.handle, c.x[1] = p.app_data, c.x[2] = header;
        call(c, kind == 0 ? p.empty_done : p.fill_done);
    }
    calls.clear();

    bool want_codec;
    int width = 0, height = 0;
    {
        std::lock_guard lk(g_proxy_lock);
        want_codec = !p.codec && !p.failed && !p.in.empty();
    }
    if (want_codec) {
        {
            std::lock_guard lk(g_soft_lock);
            if (SoftComponent* k = soft_find(p.handle)) width = k->width, height = k->height;
        }
        if (width <= 0 || height <= 0) width = 960, height = 544;
        if (!proxy_open_codec(p, width, height)) p.failed = true;
    }
    if (!p.codec) {
        if (p.failed) {  // nothing will ever decode: give the queued pictures back as empty
            std::lock_guard lk(g_proxy_lock);
            p.in.clear();
        }
        return;
    }
    for (;;) {  // input
        VideoProxy::Input item;
        {
            std::lock_guard lk(g_proxy_lock);
            if (p.in.empty()) break;
            item = p.in.front();
        }
        const ssize_t idx = AMediaCodec_dequeueInputBuffer(p.codec, 0);
        if (idx < 0) break;
        size_t cap = 0;
        uint8_t* buf = AMediaCodec_getInputBuffer(p.codec, static_cast<size_t>(idx), &cap);
        const size_t n = buf ? std::min(cap, item.data.size()) : 0;
        if (n) std::memcpy(buf, item.data.data(), n);
        uint32_t flags = 0;
        if (item.flags & 0x1) flags |= 4;      // OMX_BUFFERFLAG_EOS -> BUFFER_FLAG_END_OF_STREAM
        if (item.flags & 0x80) flags |= 2;     // OMX_BUFFERFLAG_CODECCONFIG -> BUFFER_FLAG_CODEC_CONFIG
        AMediaCodec_queueInputBuffer(p.codec, static_cast<size_t>(idx), 0, n, static_cast<uint64_t>(item.pts), flags);
        {
            std::lock_guard lk(g_proxy_lock);
            if (!p.in.empty()) p.in.pop_front();
        }
        if (item.flags & 1) log("movie: host decoder: end of stream queued after %u inputs", p.frames_in + 1);
        if (p.frames_in++ < 4) log("movie: host decoder input #%u: %zu bytes, pts %lld us", p.frames_in - 1, n, static_cast<long long>(item.pts));
    }
    for (;;) {  // output
        uint64_t header;
        {
            std::lock_guard lk(g_proxy_lock);
            if (p.free_out.empty()) { p.blocked_polls++; break; }
            header = p.free_out.front();
        }
        AMediaCodecBufferInfo info;
        const ssize_t o = AMediaCodec_dequeueOutputBuffer(p.codec, &info, 0);
        if (o == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
            if (AMediaFormat* f = AMediaCodec_getOutputFormat(p.codec)) proxy_read_format(p, f), AMediaFormat_delete(f);
            continue;
        }
        if (o == AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED) continue;
        if (o < 0) break;
        {
            std::lock_guard lk(g_proxy_lock);
            if (!p.free_out.empty()) p.free_out.pop_front();
        }
        if (!p.format_known)
            if (AMediaFormat* f = AMediaCodec_getOutputFormat(p.codec)) proxy_read_format(p, f), AMediaFormat_delete(f);
        int stride = 0, slice = 0;
        {
            std::lock_guard lk(g_soft_lock);
            if (SoftComponent* k = soft_find(p.handle)) {
                stride = k->stride > 0 ? k->stride : k->width;
                slice = k->slice > 0 ? k->slice : k->height;
            }
        }
        if (stride <= 0 || slice <= 0) stride = (p.pic_w + 15) & ~15, slice = (p.pic_h + 15) & ~15;
        size_t size = 0;
        const uint8_t* src = AMediaCodec_getOutputBuffer(p.codec, static_cast<size_t>(o), &size);
        const uint32_t room = field<uint32_t>(header, 16) - field<uint32_t>(header, 24);
        const size_t need = size_t(stride) * slice * 3 / 2;
        uint32_t filled = 0;
        if (src && info.size > 0 && need <= room) {
            auto* dst = reinterpret_cast<uint8_t*>(field<uint64_t>(header, 8) + field<uint32_t>(header, 24));
            proxy_copy_picture(p, src + info.offset, size - info.offset, dst, stride, slice);
            filled = static_cast<uint32_t>(need);
        }
        if (p.frames_out++ < 6 || p.frames_out % 120 == 0)
            log("movie: host decoder picture #%u: %d bytes, pts %lld us, flags %u -> %u bytes (%dx%d, buffer room %u)",
                p.frames_out - 1, static_cast<int>(info.size), static_cast<long long>(info.presentationTimeUs),
                static_cast<unsigned>(info.flags), filled, stride, slice, room);
        field<uint32_t>(header, 20) = filled;
        field<int64_t>(header, 88) = info.presentationTimeUs;
        field<uint32_t>(header, 96) = (info.flags & 4) ? 1u : 0u;  // END_OF_STREAM -> OMX_BUFFERFLAG_EOS
        if (info.flags & 4) log("movie: host decoder: end of stream picture after %u pictures", p.frames_out);
        AMediaCodec_releaseOutputBuffer(p.codec, static_cast<size_t>(o), false);
        c.x[0] = p.handle, c.x[1] = p.app_data, c.x[2] = header;
        call(c, p.fill_done);
    }
}

// Guest thread body: every 2 ms each proxied decoder gets a round.
void proxy_pump(CPU& c) {
    log("movie: host decoder pump thread running");
    for (;;) {
        timespec ts{0, 2'000'000};
        nanosleep(&ts, nullptr);
        std::vector<std::shared_ptr<VideoProxy>> list;
        {
            std::lock_guard lk(g_proxy_lock);
            list = g_proxies;
        }
        for (auto& p : list) {
            proxy_step(c, *p);
            timespec now{};
            clock_gettime(CLOCK_MONOTONIC, &now);
            const uint64_t t = uint64_t(now.tv_sec) * 1'000'000'000ull + uint64_t(now.tv_nsec);
            if (p->last_beat_ns && t - p->last_beat_ns < 1'000'000'000ull) continue;
            p->last_beat_ns = t;
            size_t queued, lent;
            {
                std::lock_guard lk(g_proxy_lock);
                queued = p->in.size(), lent = p->free_out.size();
            }
            if (p->frames_in || p->frames_out)
                log("movie: host decoder: %u in, %u out; %zu waiting for the codec, %zu output buffers lent (%u lends so far), "
                    "%u polls without a free output buffer", p->frames_in, p->frames_out, queued, lent, p->fills,
                    p->blocked_polls);
        }
    }
}

void start_proxy_pump(CPU& c) {
    static bool started = false;
    if (started) return;
    started = true;
    const uint64_t create = find_symbol("_ZN2nn2os12CreateThreadEPNS0_10ThreadTypeEPFvPvES3_S3_mi");
    const uint64_t start = find_symbol("_ZN2nn2os11StartThreadEPNS0_10ThreadTypeE");
    if (!create || !start) return log("movie: nn::os::CreateThread/StartThread not found, no host decoder");
    if (!g_pump_thunk) g_pump_thunk = hle_address_or("danganronpa_h264_pump", proxy_pump);
    const uint64_t thread = kernel_alloc(0x1000), stack = kernel_alloc(0x20000);
    uint64_t saved[31];
    std::memcpy(saved, c.x, sizeof saved);
    c.x[0] = thread, c.x[1] = g_pump_thunk, c.x[2] = 0, c.x[3] = stack, c.x[4] = 0x20000, c.x[5] = 16;
    call(c, create);
    const uint64_t rc = c.x[0];
    c.x[0] = thread;
    if (rc == 0) call(c, start);
    std::memcpy(c.x, saved, sizeof saved);
    if (rc) log("movie: CreateThread for the host decoder failed (%llx)", static_cast<unsigned long long>(rc));
}

// OMX_ERRORTYPE FillThisBuffer(OMX_HANDLETYPE, OMX_BUFFERHEADERTYPE*): the client lends an output buffer.
void soft_fill_this_buffer(CPU& c) {
    if (auto p = proxy_find(c.x[0])) {
        std::lock_guard lk(g_proxy_lock);
        p->free_out.push_back(c.x[1]);
        p->fills++;
        c.x[0] = 0;
        return;
    }
    call(c, g_fill_this_orig);
}

// OMX_ERRORTYPE SendCommand(OMX_HANDLETYPE, OMX_COMMANDTYPE, OMX_U32, OMX_PTR): going to Idle/Loaded or flushing the
// output port must give the lent buffers back (the component the client believes in would have).
void soft_send_command(CPU& c) {
    if (auto p = proxy_find(c.x[0])) {
        const uint32_t cmd = static_cast<uint32_t>(c.x[1]), param = static_cast<uint32_t>(c.x[2]);
        const bool state_down = cmd == 0 && (param == 1 || param == 2);
        const bool flush_out = cmd == 3 && (param == 1 || param == 0xFFFFFFFFu);
        const bool flush_in = cmd == 3 && (param == 0 || param == 0xFFFFFFFFu);
        if (state_down || flush_out || flush_in) {
            std::lock_guard lk(g_proxy_lock);
            if (state_down || flush_out) p->return_out = true;
            if (state_down || flush_in || flush_out) p->flush_codec = true, p->in.clear();
        }
        log("movie: OMX command %u (%u) on the host-decoded component", cmd, param);
    }
    call(c, g_send_orig);
}

// OMX_ERRORTYPE ComponentDeInit(OMX_HANDLETYPE)
void soft_deinit(CPU& c) {
    const uint64_t handle = c.x[0];
    {
        std::lock_guard lk(g_proxy_lock);
        for (auto it = g_proxies.begin(); it != g_proxies.end(); ++it)
            if ((*it)->handle == handle) { g_proxies.erase(it); break; }
    }
    call(c, g_deinit_orig);
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
    const auto proxy = proxy_find(c.x[0]);
    if (header && field<uint32_t>(header, 104) == 0) {
        auto* d = reinterpret_cast<uint8_t*>(field<uint64_t>(header, 8) + field<uint32_t>(header, 24));
        uint32_t len = field<uint32_t>(header, 20);
        const uint32_t flags = field<uint32_t>(header, 96);
        const unsigned k = n.fetch_add(1);
        char hex[64] = "";
        for (uint32_t i = 0; i < len && i < 16; i++) std::snprintf(hex + 3 * i, sizeof hex - 3 * i, "%02x ", d[i]);
        const char* how = len ? to_annex_b(header, d, len) : "empty";
        len = field<uint32_t>(header, 20);
        if (k < 14 || k % 200 == 0)
            log("movie: H.264 input #%u: %u bytes, flags 0x%x, pts %lld us, starts %s-> %s", k, len, flags,
                static_cast<long long>(field<int64_t>(header, 88)), hex, how);
        if (proxy) {
            std::lock_guard lk(g_proxy_lock);
            if (flags & 0x80) {  // parameter sets: kept for the codec's configuration, nothing to decode
                if (!proxy->codec) proxy->config.insert(proxy->config.end(), d, d + len);
                else proxy->in.push_back({std::vector<uint8_t>(d, d + len), 0, flags});
            } else if (len || (flags & 0x1)) {
                proxy->in.push_back({std::vector<uint8_t>(d, d + len), field<int64_t>(header, 88), flags});
            }
            proxy->give_back_in.push_back(header);
            c.x[0] = 0;
            return;
        }
    }
    call(c, g_empty_orig);
}

// The AAC decoder (not replaced): what it hands out, to tell silence from sound.
uint64_t g_aac_fill_orig, g_aac_fill_thunk;
void aac_fill_buffer_done(CPU& c) {
    const uint64_t header = c.x[2];
    static std::atomic<unsigned> n{0};
    if (header && field<uint32_t>(header, 100) == 1) {
        const unsigned k = n.fetch_add(1);
        if (k < 6 || k % 100 == 0) {
            const auto* pcm = reinterpret_cast<const int16_t*>(field<uint64_t>(header, 8) + field<uint32_t>(header, 24));
            const uint32_t bytes = field<uint32_t>(header, 20);
            int peak = 0;
            for (uint32_t i = 0; i < bytes / 2; i++) peak = std::max(peak, std::abs(int(pcm[i])));
            log("movie: AAC picture #%u: %u bytes, peak %d, pts %lld us, flags 0x%x", k, bytes, peak,
                static_cast<long long>(field<int64_t>(header, 88)), field<uint32_t>(header, 96));
        }
    }
    call(c, g_aac_fill_orig);
}

void open_movie_component(CPU& c) {
    const char* name = reinterpret_cast<const char*>(c.x[1]);
    char requested[96] = "(null)";
    if (name) std::snprintf(requested, sizeof requested, "%s", name);
    const uint64_t out = c.x[4];
    uint64_t client_empty_done = 0;
    if (name && std::strcmp(name, "OMX.google.aac.decoder") == 0 && c.x[2]) {
        auto* copy = new uint64_t[3];
        for (int k = 0; k < 3; k++) copy[k] = reinterpret_cast<uint64_t*>(c.x[2])[k];
        if (!g_aac_fill_thunk) g_aac_fill_thunk = hle_address_or("danganronpa_aac_fill_buffer_done", aac_fill_buffer_done);
        g_aac_fill_orig = copy[2];
        copy[2] = g_aac_fill_thunk;
        c.x[2] = reinterpret_cast<uint64_t>(copy);
    }
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
            g_fill_orig = copy[2], g_event_orig = copy[0], client_empty_done = copy[1];
            copy[2] = g_fill_thunk, copy[0] = g_event_thunk;
            c.x[2] = reinterpret_cast<uint64_t>(copy);
        }
    }
    call(c, original_component_open);
    const int32_t result = static_cast<int32_t>(c.x[0]);
    const uint64_t component = (result == 0 && out) ? *reinterpret_cast<uint64_t*>(out) : 0;
    if (swap && component) {
        auto* tbl = reinterpret_cast<uint64_t*>(component);  // OMX_COMPONENTTYPE: SendCommand +32 ... ComponentDeInit +144
        if (!g_set_thunk) {
            g_get_thunk = hle_address_or("danganronpa_omx_get_parameter", soft_get_parameter);
            g_set_thunk = hle_address_or("danganronpa_omx_set_parameter", soft_set_parameter);
            g_empty_thunk = hle_address_or("danganronpa_omx_empty_this_buffer", soft_empty_this_buffer);
            g_send_thunk = hle_address_or("danganronpa_omx_send_command", soft_send_command);
            g_fill_this_thunk = hle_address_or("danganronpa_omx_fill_this_buffer", soft_fill_this_buffer);
            g_deinit_thunk = hle_address_or("danganronpa_omx_deinit", soft_deinit);
        }
        {
            std::lock_guard lk(g_soft_lock);
            if (!soft_find(component)) g_soft.push_back({component});
            else *soft_find(component) = SoftComponent{component};
        }
        auto proxy = std::make_shared<VideoProxy>();
        proxy->handle = component, proxy->app_data = tbl[2];
        proxy->empty_done = client_empty_done, proxy->fill_done = g_fill_orig;
        {
            std::lock_guard lk(g_proxy_lock);
            for (auto it = g_proxies.begin(); it != g_proxies.end();)
                it = ((*it)->handle == component) ? g_proxies.erase(it) : it + 1;
            g_proxies.push_back(proxy);
        }
        if (tbl[4] != g_send_thunk) g_send_orig = tbl[4], tbl[4] = g_send_thunk;
        if (tbl[5] != g_get_thunk) g_get_orig = tbl[5], tbl[5] = g_get_thunk;
        if (tbl[6] != g_set_thunk) g_set_orig = tbl[6], tbl[6] = g_set_thunk;
        if (tbl[15] != g_empty_thunk) g_empty_orig = tbl[15], tbl[15] = g_empty_thunk;
        if (tbl[16] != g_fill_this_thunk) g_fill_this_orig = tbl[16], tbl[16] = g_fill_this_thunk;
        if (tbl[18] != g_deinit_thunk) g_deinit_orig = tbl[18], tbl[18] = g_deinit_thunk;
        start_proxy_pump(c);
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
std::atomic<uint64_t> g_video_decoder{0};
std::atomic<unsigned> g_video_calls{0};
std::atomic<unsigned> g_slot_count[kSlots];

template <int N>
void trace_decoder_slot(CPU& c) {
    const uint64_t self = c.x[0], a1 = c.x[1], a2 = c.x[2], a3 = c.x[3], a4 = c.x[4], a5 = c.x[5];
    call(c, g_slot_original[N]);
    const int32_t result = static_cast<int32_t>(c.x[0]);
    const unsigned n = g_slot_count[N].fetch_add(1, std::memory_order_relaxed);
    const bool video = self == g_video_decoder.load();
    static std::atomic<unsigned> notable{0};
    // Start/Stop/Reset/Flush/GetLastError and every failure are always worth a line (the busy slots are limited).
    const bool rare = (N >= 2 && N <= 8) || N == 16 || result != 0;
    if (rare ? notable.fetch_add(1) >= 400
             : (video ? g_video_calls.fetch_add(1) >= 600 : (n >= 30 && n % 300 != 0)))
        return;
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
    const bool is_video = static_cast<int>(c.x[1]) == 1;
    log("movie: Decoder ctor type %d format %d mode %d priority %d (object %llx)",
        static_cast<int>(c.x[1]), static_cast<int>(c.x[2]), static_cast<int>(c.x[3]),
        static_cast<int>(c.x[4]), static_cast<unsigned long long>(self));
    call(c, original_decoder_ctor);
    if (is_video) g_video_decoder = self;
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
