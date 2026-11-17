cmake_minimum_required(VERSION 3.24)

# Clang par défaut, mais -DCMAKE_C_COMPILER=... reste possible.
if (NOT DEFINED CMAKE_C_COMPILER AND NOT DEFINED ENV{CC})
    set(CMAKE_C_COMPILER clang)
endif()

project(tp3 LANGUAGES C)

if (NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "Ce projet doit être compilé avec Clang (compilateur détecté : "
        "${CMAKE_C_COMPILER_ID}). Essayez -DCMAKE_C_COMPILER=clang.")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

include(CTest)

# -----------------------------------------------------------------------------
# Options
# -----------------------------------------------------------------------------

set(HEATSIM_IMPL_DIR "${PROJECT_SOURCE_DIR}/source" CACHE PATH
    "Dossier contenant heatsim-mpi.c ; relatif à la racine du projet si le chemin n'est pas absolu (ex. : -DHEATSIM_IMPL_DIR=charge/solution)")
file(REAL_PATH "${HEATSIM_IMPL_DIR}" HEATSIM_IMPL_DIR BASE_DIRECTORY "${PROJECT_SOURCE_DIR}")

set(HEATSIM_IMAGE "images/earth-medium.png" CACHE STRING "Image des cibles run-*")
set(HEATSIM_ITERATIONS 1000 CACHE STRING "Nombre d'itérations des cibles run-* (la correction en utilise 12000, en 8x8 rangs)")
set(HEATSIM_DIM_X 3 CACHE STRING "Rangs en X pour run-mpi et run-all")
set(HEATSIM_DIM_Y 4 CACHE STRING "Rangs en Y pour run-mpi et run-all")

option(ENABLE_ASAN "Build project executables with AddressSanitizer" OFF)

if (ENABLE_ASAN)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=address)
endif()

# -----------------------------------------------------------------------------
# Dépendances (installées sur la machine : OpenMPI et libpng)
# -----------------------------------------------------------------------------

find_package(MPI REQUIRED COMPONENTS C)
find_package(PNG REQUIRED)

# -----------------------------------------------------------------------------
# Exécutable
# -----------------------------------------------------------------------------

add_library(heatsim-common OBJECT
    source/cart.c
    source/color.c
    source/grid.c
    source/image.c
)
target_include_directories(heatsim-common PUBLIC include)
target_compile_options(heatsim-common PRIVATE -Wall)
target_link_libraries(heatsim-common PUBLIC PNG::PNG m)

add_executable(heatsim
    source/heatsim.c
    source/main.c
    ${HEATSIM_IMPL_DIR}/heatsim-mpi.c
)
# Les `fail_exit:` des stubs ne sont utilisés qu'une fois le code complété.
target_compile_options(heatsim PRIVATE -Wall -Wno-unused-label)
target_link_libraries(heatsim PRIVATE heatsim-common MPI::MPI_C)

# -----------------------------------------------------------------------------
# Tests
# -----------------------------------------------------------------------------

add_subdirectory(tests)

# Plusieurs topologies (1x1, 1x5, 3x3, 2x5, 4x4, 8x4) doivent donner exactement l'image de référence
# (scripts/run-check). Un échange de bordures qui se bloque dépasse le délai et échoue.
add_test(NAME check-checksums
    COMMAND ${PROJECT_SOURCE_DIR}/scripts/run-check $<TARGET_FILE:heatsim> ${PROJECT_BINARY_DIR}/check
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
)
set_tests_properties(check-checksums PROPERTIES TIMEOUT 120)

# -----------------------------------------------------------------------------
# Commandes
# -----------------------------------------------------------------------------

set(HEATSIM_RUN_ARGS --input ${HEATSIM_IMAGE} --iterations ${HEATSIM_ITERATIONS})
set(HEATSIM_RANKS 1)
math(EXPR HEATSIM_RANKS "${HEATSIM_DIM_X} * ${HEATSIM_DIM_Y}")

execute_process(COMMAND ${MPIEXEC_EXECUTABLE} --oversubscribe --help
    RESULT_VARIABLE HEATSIM_OVERSUBSCRIBE_RESULT OUTPUT_QUIET ERROR_QUIET)
set(HEATSIM_MPIEXEC ${MPIEXEC_EXECUTABLE})
if (HEATSIM_OVERSUBSCRIBE_RESULT EQUAL 0)
    list(APPEND HEATSIM_MPIEXEC --oversubscribe)
endif()

add_custom_target(run-serial
    COMMAND ${HEATSIM_MPIEXEC} -n 1 $<TARGET_FILE:heatsim> --dim-x 1 --dim-y 1
            ${HEATSIM_RUN_ARGS} --output ${PROJECT_BINARY_DIR}/serial.png
    DEPENDS heatsim
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    USES_TERMINAL
)

add_custom_target(run-mpi
    COMMAND ${HEATSIM_MPIEXEC} -n ${HEATSIM_RANKS} $<TARGET_FILE:heatsim>
            --dim-x ${HEATSIM_DIM_X} --dim-y ${HEATSIM_DIM_Y}
            ${HEATSIM_RUN_ARGS} --output ${PROJECT_BINARY_DIR}/parallel.png
    DEPENDS heatsim
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    USES_TERMINAL
)

# Compare les temps du sériel et du parallèle et affiche l'accélération.
add_custom_target(run-all
    COMMAND ${PROJECT_SOURCE_DIR}/scripts/run-all $<TARGET_FILE:heatsim> ${HEATSIM_IMAGE}
            ${HEATSIM_ITERATIONS} ${HEATSIM_DIM_X} ${HEATSIM_DIM_Y} ${PROJECT_BINARY_DIR}
    DEPENDS heatsim
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    USES_TERMINAL
)

file(GLOB_RECURSE HEATSIM_FORMAT_FILES CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/source/*.c"
    "${PROJECT_SOURCE_DIR}/include/*.h"
    "${PROJECT_SOURCE_DIR}/tests/*.c")
add_custom_target(format
    COMMAND clang-format -i --verbose ${HEATSIM_FORMAT_FILES}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    COMMAND_EXPAND_LISTS
)

install(TARGETS heatsim)
