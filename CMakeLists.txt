#[[
CMakeLists.txt - build alternativa a test/run_tests.sh.

Uso:
    cmake -S . -B build_cmake
    cmake --build build_cmake
    # eseguibile: build_cmake/main

    ctest --test-dir build_cmake --output-on-failure
    # lancia tutti i test Unity registrati sotto test/ (equivalente a test/run_tests.sh)

Struttura del progetto: tutti i moduli (.c/.h) nella cartella principale,
nessuna sottocartella lib/ o app/ - vedi test/run_tests.sh per la stessa
convenzione (-I. come unico include).
#]]

cmake_minimum_required(VERSION 3.16)
project(cella_meccatronica C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

add_compile_options(-Wall -Wextra)

# ------------------------------------------------------------------
# Librerie del progetto: elencate esplicitamente (non con
# file(GLOB_RECURSE ...)) per lo stesso motivo per cui test/run_tests.sh
# non usa un glob: un file .c aggiunto per errore non deve finire
# silenziosamente nella build principale.
# ------------------------------------------------------------------

set(LIB_SOURCES
    object.c
    idlist.c
    registry.c
    buffer.c
    cell.c
    nastro.c
    machine.c
    isp.c
    Motore.c
    Deviatore.c
    S_Presenza.c
    S_Buffer.c
    S_Qualita.c
    Controllore.c
    parser.c
    statistiche.c
    log.c
)

set(LIB_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}
)

# Libreria statica interna: cosi' sia l'eseguibile principale sia ogni
# eseguibile di test (sotto) linkano lo STESSO oggetto compilato una sola
# volta, invece di ricompilare tutti i moduli per ogni test_*.c.
add_library(cella_lib STATIC ${LIB_SOURCES})
target_include_directories(cella_lib PUBLIC ${LIB_INCLUDE_DIRS})
target_link_libraries(cella_lib PUBLIC m)   # -lm: usata da statistiche/macchina (calcoli in double)

# ------------------------------------------------------------------
# Eseguibile principale (equivalente a main.c compilato da build.sh/VSCode)
# ------------------------------------------------------------------

add_executable(main main.c)
target_link_libraries(main PRIVATE cella_lib)

# ------------------------------------------------------------------
# Test Unity (equivalenti a test/run_tests.sh), registrati in CTest.
# ------------------------------------------------------------------

enable_testing()

add_library(unity STATIC test/unity/unity.c)
target_include_directories(unity PUBLIC test/unity)
target_compile_definitions(unity PUBLIC UNITY_INCLUDE_DOUBLE)

file(GLOB UNITY_TEST_SOURCES CONFIGURE_DEPENDS "test/test_*.c")
foreach(TEST_SRC ${UNITY_TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_SRC} NAME_WE)
    add_executable(${TEST_NAME} ${TEST_SRC})
    target_link_libraries(${TEST_NAME} PRIVATE unity cella_lib)
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
    # I test leggono file con percorsi RELATIVI (es. "test/fixtures/...",
    # "plant_config_valid.txt"), assumendo di essere lanciati dalla root
    # del progetto - stessa convenzione di test/run_tests.sh.
    set_tests_properties(${TEST_NAME} PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endforeach()
