
#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "nxgame.h"

namespace danganronpa {

// Extrai os shaders de Persona 4 Golden.
// Implementacao pendente no danganronpa_shaders.cpp.
//
// Retorna a quantidade de shaders extraidos,
// ou -1 em caso de erro.
long extract_shaders(
    const nxi::GameImage& game,
    const std::string& out_dir,
    const std::function<void(int, int)>& progress,
    std::string& error
);

} // namespace danganronpa
