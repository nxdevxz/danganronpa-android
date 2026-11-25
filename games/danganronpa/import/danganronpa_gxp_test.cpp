
#include "danganronpa_gxp.h"

#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr
            << "Uso: danganronpa_gxp_test <arquivo.bin>\n";
        return 1;
    }

    std::ifstream file(argv[1], std::ios::binary);

    if (!file) {
        std::cerr << "Erro ao abrir arquivo.\n";
        return 1;
    }

    std::vector<uint8_t> data(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
    );

    std::vector<danganronpa::GxpRecord> records;
    std::string error;

    if (!danganronpa::parseGxp(data, records, error)) {
        std::cerr << "Erro: " << error << "\n";
        return 1;
    }

    std::cout
        << "Total de programas GXP: "
        << records.size() << "\n\n";

    for (const auto& record : records) {
        std::cout
            << record.name
            << " | offset: " << record.offset
            << " | tamanho: " << record.size
            << " bytes\n";
    }

    return 0;
}
