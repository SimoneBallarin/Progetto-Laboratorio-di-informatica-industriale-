#!/bin/bash
# Compila e lancia tutti i test Unity del progetto (uno per file test_*.c),
# con gli stessi flag/include del build.sh principale. Va lanciato dalla
# ROOT del progetto (stessa cartella di build.sh), non da dentro test/:
#   ./test/run_tests.sh
#
# Ogni file test_*.c e' un eseguibile Unity indipendente (con il proprio
# main() -> UNITY_BEGIN/RUN_TEST/UNITY_END, vedi test/test_*.c): questo
# script li compila e li lancia uno alla volta, cosi' un crash in un file
# di test non impedisce agli altri di girare.
set -u

INCLUDES="-I. -Itest/unity"

SRCS_COMUNI="Controllore.c Deviatore.c Motore.c S_Buffer.c S_Presenza.c S_Qualita.c \
  buffer.c cell.c idlist.c isp.c log.c machine.c nastro.c object.c parser.c registry.c statistiche.c"
mkdir -p test/build

FALLITI=0
TOTALE=0

for TEST_FILE in test/test_*.c; do
    NOME=$(basename "$TEST_FILE" .c)
    ESEGUIBILE="test/build/$NOME"
    TOTALE=$((TOTALE + 1))

    echo "=============================================="
    echo " Compilo ed eseguo: $NOME"
    echo "=============================================="

    # -DUNITY_INCLUDE_DOUBLE: senza questo flag Unity disabilita gli assert
    # su double (TEST_ASSERT_EQUAL_DOUBLE) e li segna come FAIL - necessario
    # per test_object.c/test_statistiche.c, che confrontano dimensionX/raggio
    # e le medie aggregate (double) del modulo Statistiche.
    eval gcc -g -Wall -Wextra -std=c11 -DUNITY_INCLUDE_DOUBLE \
        $INCLUDES \
        test/unity/unity.c "$TEST_FILE" $SRCS_COMUNI \
        -o "$ESEGUIBILE" -lm

    if [ $? -ne 0 ]; then
        echo ">>> COMPILAZIONE FALLITA per $NOME"
        FALLITI=$((FALLITI + 1))
        continue
    fi

    "./$ESEGUIBILE"
    if [ $? -ne 0 ]; then
        FALLITI=$((FALLITI + 1))
    fi
    echo ""
done

echo "=============================================="
echo " Riepilogo: $((TOTALE - FALLITI))/$TOTALE file di test superati"
echo "=============================================="

if [ $FALLITI -gt 0 ]; then
    exit 1
fi
exit 0
