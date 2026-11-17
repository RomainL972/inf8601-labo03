# Vérifie qu'une topologie (DIM_X x DIM_Y) produit exactement l'image de référence: le SHA-256 de
# l'image doit être celui de images/check<N>.checksum (N = DIM_X * DIM_Y). Un échange de bordures
# bloqué ne termine jamais: c'est le délai de CTest qui fait alors échouer le test.
#
# Usage: cmake -DHEATSIM=<exécutable> -DMPIEXEC=<mpiexec> -DOVERSUBSCRIBE=<ON|OFF> -DDIM_X=<n>
#              -DDIM_Y=<n> -DOUT=<dossier> -P scripts/check-topology.cmake
# À lancer depuis la racine du projet.

math(EXPR N "${DIM_X} * ${DIM_Y}")
set(output "${OUT}/check${N}.png")

set(mpi ${MPIEXEC})
if (OVERSUBSCRIBE)
    list(APPEND mpi --oversubscribe)
endif()

file(MAKE_DIRECTORY "${OUT}")
execute_process(
    COMMAND ${mpi} -n ${N} ${HEATSIM} --dim-x ${DIM_X} --dim-y ${DIM_Y}
            --input ./images/earth-medium.png --iterations 10 --output ${output}
    RESULT_VARIABLE result
    OUTPUT_QUIET
)
if (NOT result EQUAL 0)
    message(FATAL_ERROR "${DIM_X}x${DIM_Y}: heatsim a échoué (code ${result})")
endif()

file(READ "images/check${N}.checksum" expected)
string(REGEX MATCH "^[0-9a-f]+" expected "${expected}")
file(SHA256 "${output}" actual)

if (NOT expected STREQUAL actual)
    message(FATAL_ERROR "${DIM_X}x${DIM_Y}: image différente de la référence\n"
        "  attendu: ${expected}\n  obtenu:  ${actual}")
endif()
message(STATUS "${DIM_X}x${DIM_Y} (${N} rangs): OK")
