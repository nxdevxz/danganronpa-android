#pragma once

#include <nx/runtime.h>

namespace nx {

// QueryMemory describes mapped pages, not the unpadded bytes in the NSO file.
// GetExceptionInfo walks these adjacent Code/CodeData regions to locate MOD0
// and its unwind tables. A hole at the end of .text or .ro stops that walk.
inline bool map_module_memory(const ModuleInfo& m, uint64_t size) {
    if ((m.base | m.text | m.ro | m.data | size) & 0xfff) return false;
    if (m.text > m.ro || m.ro > m.data || m.data > size ||
        m.text_size > m.ro - m.text || m.ro_size > m.data - m.ro ||
        m.data_size > size - m.data || m.base + size < m.base) return false;

    if (m.ro > m.text) vm_set(m.base + m.text, m.ro - m.text, kMemCode, 5);
    if (m.data > m.ro) vm_set(m.base + m.ro, m.data - m.ro, kMemCode, 1);
    if (size > m.data) vm_set(m.base + m.data, size - m.data, kMemCodeData, 3);
    return true;
}

}  // namespace nx
