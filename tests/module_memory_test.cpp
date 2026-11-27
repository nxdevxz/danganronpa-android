// Run from the repository root:
// c++ -std=c++20 -ffunction-sections -fdata-sections -Iruntime/include -Iruntime/src
//     tests/module_memory_test.cpp runtime/src/memory.cpp -Wl,--gc-sections -pthread -o /tmp/module_memory_test
#include "module_memory.h"
#include <cassert>
#include <cstdio>

using namespace nx;

static void check(const ModuleInfo& m, uint64_t size) {
    assert(map_module_memory(m, size));
    // Include the last byte of both file sections and their page padding.
    for (uint64_t offset : {m.text, m.text + m.text_size - 1, m.ro - 1}) {
        auto q = vm_query(m.base + offset);
        assert(q.addr == m.base + m.text && q.size == m.ro - m.text);
        assert(q.state == kMemCode && q.perm == 5);
    }
    for (uint64_t offset : {m.ro, m.ro + m.ro_size - 1, m.data - 1}) {
        auto q = vm_query(m.base + offset);
        assert(q.addr == m.base + m.ro && q.size == m.data - m.ro);
        assert(q.state == kMemCode && q.perm == 1);
    }
    auto data = vm_query(m.base + size - 1);
    assert(data.addr == m.base + m.data && data.size == size - m.data);
    assert(data.state == kMemCodeData && data.perm == 3);
    // Model the unwinder's forward walk; no inaccessible/free gap may
    // terminate module discovery before its writable section and BSS.
    uint64_t cursor = m.base;
    for (int i = 0; i < 3; ++i) {
        auto q = vm_query(cursor);
        assert(q.addr == cursor && q.size && !(q.size & 0xfff));
        assert(q.state == (i == 2 ? kMemCodeData : kMemCode));
        cursor += q.size;
    }
    assert(cursor == m.base + size);
    assert(vm_query(m.base - 1).state == kMemInaccessible);
    assert(vm_query(cursor).state == kMemInaccessible);
}

int main() {
    const uint64_t base = kModuleBase;
    vm_set(base - 0x1000, 0x1000000, kMemInaccessible, 0);
    ModuleInfo m{};
    // Segment offsets/sizes from the user's subsdk0 NSO header.
    m.base = base;
    m.text_size = 0x3548e0;
    m.ro = 0x355000; m.ro_size = 0x20f47c;
    m.data = 0x565000; m.data_size = 0x30710;
    check(m, 0x600000);
    // Also cover already page-aligned sections in a distinct module.
    m.base += 0x800000;
    m.text_size = 0x1000;
    m.ro = 0x1000; m.ro_size = 0x1000;
    m.data = 0x2000; m.data_size = 0x1000;
    check(m, 0x4000);
    m.base += 0x10000;
    m.ro = 0x800;  // invalid layout must not publish a partial map
    assert(!map_module_memory(m, 0x4000));
    assert(vm_query(m.base).state == kMemInaccessible);
    std::puts("module memory: padding, module walk, BSS and boundaries passed");
}
