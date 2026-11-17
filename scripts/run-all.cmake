# Exécute le même calcul en série (1x1) puis en parallèle (DIM_X x DIM_Y) et affiche l'accélération.
#
# Usage: cmake -DHEATSIM=<exécutable> -DMPIEXEC=<mpiexec> -DOVERSUBSCRIBE=<ON|OFF> -DIMAGE=<png>
#              -DITERATIONS=<n> -DDIM_X=<n> -DDIM_Y=<n> -DOUT=<dossier> -P scripts/run-all.cmake
# À lancer depuis la racine du projet.

math(EXPR ranks "${DIM_X} * ${DIM_Y}")

set(mpi ${MPIEXEC})
if (OVERSUBSCRIBE)
    list(APPEND mpi --oversubscribe)
endif()

file(MAKE_DIRECTORY "${OUT}")

# Temps écoulé, en millisecondes, d'une exécution de heatsim (mesuré par `cmake -E time`).
function(timed result_var n dim_x dim_y output)
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E time ${mpi} -n ${n} ${HEATSIM} --dim-x ${dim_x} --dim-y ${dim_y}
                --input ${IMAGE} --iterations ${ITERATIONS} --output ${output}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE out
    )
    if (NOT result EQUAL 0)
        message(FATAL_ERROR "heatsim ${dim_x}x${dim_y} a échoué (code ${result})")
    endif()
    if (NOT out MATCHES "Elapsed time \\(seconds\\): ([0-9]+)(\\.([0-9]+))?")
        message(FATAL_ERROR "Temps écoulé introuvable dans la sortie de `cmake -E time`")
    endif()
    string(SUBSTRING "${CMAKE_MATCH_3}000" 0 3 millis)
    math(EXPR total "${CMAKE_MATCH_1} * 1000 + 1${millis} - 1000")
    set(${result_var} ${total} PARENT_SCOPE)
endfunction()

# Millisecondes -> "s.mmm".
function(format_seconds result_var ms)
    math(EXPR whole "${ms} / 1000")
    math(EXPR frac "${ms} % 1000 + 1000")
    string(SUBSTRING "${frac}" 1 3 frac)
    set(${result_var} "${whole}.${frac}" PARENT_SCOPE)
endfunction()

message(STATUS "Série (1x1)...")
timed(serial 1 1 1 "${OUT}/serial.png")
message(STATUS "Parallèle (${DIM_X}x${DIM_Y}, ${ranks} rangs)...")
timed(parallel ${ranks} ${DIM_X} ${DIM_Y} "${OUT}/parallel.png")

if (parallel EQUAL 0)
    set(parallel 1)
endif()
math(EXPR speedup "${serial} * 100 / ${parallel}")
math(EXPR speedup_whole "${speedup} / 100")
math(EXPR speedup_frac "${speedup} % 100 + 100")
string(SUBSTRING "${speedup_frac}" 1 2 speedup_frac)

format_seconds(serial_s ${serial})
format_seconds(parallel_s ${parallel})
message(STATUS "")
message(STATUS "série:        ${serial_s} s")
message(STATUS "parallèle:    ${parallel_s} s")
message(STATUS "accélération: x${speedup_whole}.${speedup_frac}")
