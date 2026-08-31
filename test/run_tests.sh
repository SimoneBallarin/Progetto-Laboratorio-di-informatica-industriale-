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

INCLUDES="-Itest/unity \
  -Ilib/Errori -Ilib/Oggetto -Ilib/IDlist -Ilib/Registro -Ilib/Buffer \
  -Ilib/Cella -I\"lib/Nastro Trasportare\" -Ilib/Macchina -Ilib/ISP \
  -Ilib/Attuatori/Motori -Ilib/Attuatori/Servo \
  -I\"lib/Sensori/Sensore di Prossimita\" -I\"lib/Sensori/Sensore Buffer\" -I\"lib/Sensori/Sensore Qualita\" \
  -Ilib/Controllore -Ilib/parser -Ilib/Statistiche -Ilib/Log"

SRCS_COMUNI="lib/Oggetto/object.c lib/IDlist/idlist.c lib/Registro/registry.c lib/Buffer/buffer.c \
  lib/Cella/cell.c \"lib/Nastro Trasportare/nastro.c\" lib/Macchina/machine.c lib/ISP/isp.c \
  lib/Attuatori/Motori/Motore.c lib/Attuatori/Servo/Deviatore.c \
  \"lib/Sensori/Sensore di Prossimita/S_Presenza.c\" \"lib/Sensori/Sensore Buffer/S_Buffer.c\" \"lib/Sensori/Sensore Qualita/S_Qualita.c\" \
  lib/Controllore/Controllore.c lib/Statistiche/statistiche.c lib/Log/log.c lib/parser/parser.c"

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
