#[[
CMakeLists.txt - build alternativa a test/run_tests.sh.

Uso:
    cmake -S . -B build_cmake
    cmake --build build_cmake
    # eseguibile: build_cmake/main

    ctest --test-dir build_cmake --output-on-failure
    # lancia tutti i test Unity registrati sotto test/ (equivalente a test/run_tests.sh)

Struttura del progetto: moduli sotto lib/<Modulo>/, eseguibile principale
in app/main.c - vedi test/run_tests.sh per la stessa convenzione di
include (una -I per ciascuna sottocartella di lib/ con un header, vedi
LIB_INCLUDE_DIRS).
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
    lib/Oggetto/object.c
    lib/Idlist/idlist.c
    lib/Registry/registry.c
    lib/Buffer/buffer.c
    lib/Cella/cell.c
    lib/Nastro/nastro.c
    lib/Macchina/machine.c
    lib/Isp/isp.c
    lib/Attuatori/Motore/Motore.c
    lib/Attuatori/Deviatore/Deviatore.c
    "lib/Sensori/Sensore Presenza/S_Presenza.c"
    "lib/Sensori/Sensore Buffer/S_Buffer.c"
    "lib/Sensori/Sensore Qualita/S_Qualita.c"
    lib/Controllore/Controllore.c
    lib/Parser/parser.c
    lib/Statistiche/statistiche.c
    lib/log/log.c
)

# Ogni header e' incluso in forma "flat" (es. #include "object.h"), quindi
# ogni sottocartella di lib/ che contiene un .h deve comparire qui.
set(LIB_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/lib/Oggetto
    ${CMAKE_SOURCE_DIR}/lib/Idlist
    ${CMAKE_SOURCE_DIR}/lib/Registry
    ${CMAKE_SOURCE_DIR}/lib/Buffer
    ${CMAKE_SOURCE_DIR}/lib/Cella
    ${CMAKE_SOURCE_DIR}/lib/Nastro
    ${CMAKE_SOURCE_DIR}/lib/Macchina
    ${CMAKE_SOURCE_DIR}/lib/Isp
    ${CMAKE_SOURCE_DIR}/lib/Attuatori/Motore
    ${CMAKE_SOURCE_DIR}/lib/Attuatori/Deviatore
    "${CMAKE_SOURCE_DIR}/lib/Sensori/Sensore Presenza"
    "${CMAKE_SOURCE_DIR}/lib/Sensori/Sensore Buffer"
    "${CMAKE_SOURCE_DIR}/lib/Sensori/Sensore Qualita"
    ${CMAKE_SOURCE_DIR}/lib/Controllore
    ${CMAKE_SOURCE_DIR}/lib/Parser
    ${CMAKE_SOURCE_DIR}/lib/Statistiche
    ${CMAKE_SOURCE_DIR}/lib/log
    ${CMAKE_SOURCE_DIR}/lib/Errori
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

add_executable(main app/main.c)
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
    # "Config/Layout/plant_config_layout1.txt"), assumendo di essere lanciati
    # dalla root del progetto - stessa convenzione di test/run_tests.sh.
    set_tests_properties(${TEST_NAME} PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endforeach()
