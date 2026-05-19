#!/usr/bin/env bash

set -u

TIMEOUT_SEC="${TIMEOUT_SEC:-5}"
LOG_FILE="${LOG_FILE:-test_output.log}"

if [ -z "${PICO_EXEC:-}" ]; then
    if [ -f "../build/pico.exe" ]; then
        PICO_EXEC="../build/pico.exe"
    elif [ -f "../build/pico" ]; then
        PICO_EXEC="../build/pico"
    else
        PICO_EXEC="$(find ../build -name "pico*" -type f 2>/dev/null | head -n 1)"
    fi
fi

if [ -z "${PICO_EXEC:-}" ] || [ ! -f "$PICO_EXEC" ]; then
    echo "Error: pico executable not found."
    echo "Set it manually, for example:"
    echo "  PICO_EXEC=\"../build/pico\" bash run_tests.sh"
    echo "  PICO_EXEC=\"../build-stress/pico\" TIMEOUT_SEC=60 bash run_tests.sh"
    exit 1
fi

if [ ! -x "$PICO_EXEC" ]; then
    chmod +x "$PICO_EXEC" 2>/dev/null || true
fi

PASSED=0
FAILED=0
TIMEOUTS=0

rm -f "$LOG_FILE"

echo "Starting tests..."
echo "PICO_EXEC: $PICO_EXEC"
echo "TIMEOUT_SEC: $TIMEOUT_SEC"
echo

for file in test_*.pcs; do
    if [ ! -f "$file" ]; then
        continue
    fi

    printf "Running %-35s " "$file"

    timeout "$TIMEOUT_SEC" "$PICO_EXEC" "$file" > "$LOG_FILE" 2>&1
    EXIT_CODE=$?

    if [ "$EXIT_CODE" -eq 0 ]; then
        echo "[PASS]"
        PASSED=$((PASSED + 1))
    elif [ "$EXIT_CODE" -eq 124 ]; then
        echo "[TIMEOUT]"
        echo "Timed out after ${TIMEOUT_SEC}s"
        if [ -s "$LOG_FILE" ]; then
            head -80 "$LOG_FILE"
        fi
        TIMEOUTS=$((TIMEOUTS + 1))
        FAILED=$((FAILED + 1))
    elif [ "$EXIT_CODE" -eq 139 ]; then
        echo "[SEGFAULT]"
        if [ -s "$LOG_FILE" ]; then
            head -120 "$LOG_FILE"
        else
            echo "Segmentation fault, no output captured."
        fi
        FAILED=$((FAILED + 1))
    else
        echo "[FAIL]"
        if [ -s "$LOG_FILE" ]; then
            head -120 "$LOG_FILE"
        else
            echo "No output captured."
        fi
        FAILED=$((FAILED + 1))
    fi
done

rm -f "$LOG_FILE"

echo
echo "========================================"
echo "Summary: $PASSED Passed, $FAILED Failed ($TIMEOUTS Timeouts)"
echo "========================================"

if [ "$FAILED" -eq 0 ]; then
    exit 0
else
    exit 1
fi
