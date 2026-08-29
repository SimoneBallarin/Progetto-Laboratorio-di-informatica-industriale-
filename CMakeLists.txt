#[[
CMakeLists.txt - build alternativa a build.sh/.vscode/tasks.json.

Uso:
    cmake -S . -B build_cmake
    cmake --build build_cmake
    # eseguibile: build_cmake/main  (equivalente ad app/main.exe)

    ctest --test-dir build_cmake --output-on-failure
    # lancia tutti i test Unity registrati sotto test/ (equivalente a test/run_tests.sh)

Struttura del progetto: un modulo per cartella sotto lib/, ciascuno con la
coppia include/ (o cartella diretta, per i moduli piu' vecchi come Controllore/
Statistiche/parser/Log/Errori/Macchina/ISP) + src/ o scr/ (refuso storico in
alcuni moduli, mantenuto per non rinominare file gia' committati). Alcune
cartelle contengono spazi nel nome ("Nastro Trasportare", "Sensore Buffer",
"Sensore di Prossimita", "Sensore Qualita"): CMake li gestisce senza problemi
purche' passati come stringhe quotate, mai come glob non quotati.
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
# Librerie del progetto: percorsi sorgente/header, uno per modulo.
# Elencati esplicitamente (non con file(GLOB_RECURSE ...)) per lo stesso
# motivo per cui build.sh non usa un glob: un file .c aggiunto per errore
# in una cartella (es. un file di test lasciato lì per sbaglio) non deve
# finire silenziosamente nella build principale.
# ------------------------------------------------------------------

set(LIB_SOURCES
    lib/Oggetto/src/object.c
    lib/IDlist/scr/idlist.c
    lib/Registro/scr/registry.c
    lib/Buffer/src/buffer.c
    lib/Cella/src/cell.c
    "lib/Nastro Trasportare/src/nastro.c"
    lib/Macchina/machine.c
    lib/ISP/isp.c
    lib/Attuatori/Motori/scr/Motore.c
    lib/Attuatori/Servo/scr/Deviatore.c
    "lib/Sensori/Sensore di Prossimita/src/S_Presenza.c"
    "lib/Sensori/Sensore Buffer/src/S_Buffer.c"
    "lib/Sensori/Sensore Qualita/src/S_Qualita.c"
    lib/Controllore/Controllore.c
    lib/parser/parser.c
    lib/Statistiche/statistiche.c
    lib/Log/log.c
)

set(LIB_INCLUDE_DIRS
    lib/Errori
    lib/Oggetto/include
    lib/IDlist/include
    lib/Registro/include
    lib/Buffer/include
    lib/Cella/include
    "lib/Nastro Trasportare/include"
    lib/Macchina
    lib/ISP
    lib/Attuatori/Motori/include
    lib/Attuatori/Servo/include
    "lib/Sensori/Sensore di Prossimita/include"
    "lib/Sensori/Sensore Buffer/include"
    "lib/Sensori/Sensore Qualita/include"
    lib/Controllore
    lib/parser
    lib/Statistiche
    lib/Log
)

# Libreria statica interna: cosi' sia l'eseguibile principale sia ogni
# eseguibile di test (sotto) linkano lo STESSO oggetto compilato una sola
# volta, invece di ricompilare tutti i moduli per ogni test_*.c (come fa
# invece test/run_tests.sh, per restare indipendente da CMake).
add_library(cella_lib STATIC ${LIB_SOURCES})
target_include_directories(cella_lib PUBLIC ${LIB_INCLUDE_DIRS})
target_link_libraries(cella_lib PUBLIC m)   # -lm: usata da statistiche/macchina (calcoli in double)

# ------------------------------------------------------------------
# Eseguibile principale (equivalente ad app/main.exe)
# ------------------------------------------------------------------

add_executable(main app/main.c)
target_link_libraries(main PRIVATE cella_lib)

# ------------------------------------------------------------------
# Test Unity (equivalenti a test/run_tests.sh), registrati in CTest.
# Ogni test/test_*.c ha gia' il proprio main() (UNITY_BEGIN/RUN_TEST/
# UNITY_END, vedi test/test_*.c): diventa un eseguibile CTest a se'
# stante, cosi' un test che fallisce non impedisce agli altri di girare
# (stessa filosofia di test/run_tests.sh).
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
    # "lib/parser/scenario_difficile.txt"), assumendo di essere lanciati
    # dalla root del progetto - stessa convenzione di test/run_tests.sh.
    # CTest di default lancia ogni eseguibile con cwd = cartella di
    # build, non la root: senza questa property, i test che aprono file
    # (es. test_parser.c) fallirebbero SOLO sotto ctest, non sotto
    # test/run_tests.sh - scoperto proprio verificando che entrambi i
    # percorsi di build dessero lo stesso risultato.
    set_tests_properties(${TEST_NAME} PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endforeach()
