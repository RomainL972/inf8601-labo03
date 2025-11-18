/** \file main.cpp
 * \brief Program checker entry point
 *
 * \author Sébastien Darche <sebastien.darche@polymtl.ca>
 */

#include "file_search.hpp"

#include <array>
#include <iostream>
#include <sstream>

int main(int argc, const char** argv) {
    if (argc != 3) {
        printf("Usage: %s <variant> <lab_directory>\n", argv[0]);
        return 1;
    }

    // Grep-like search for some constructs
    std::stringstream buffer;

    buffer << argv[1] << " " << argv[2] << "/source/heatsim-mpi.c\n";

    int variant;
    std::string path;

    buffer >> variant >> path;

    auto req = getVariantRequirements(variant);

    std::cout << req;

    assertVariant(req, path);

    std::cout << "\n\nChecker OK\n";
    return 0;
}
