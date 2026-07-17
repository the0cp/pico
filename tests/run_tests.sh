#!/usr/bin/env bash

set -u

CIETO_EXEC="${1:-${CIETO_EXEC:-}}"
TIMEOUT_SEC="${2:-${TIMEOUT_SEC:-5}}"
LOG_FILE="${LOG_FILE:-test_output.log}"

if [ -z "$CIETO_EXEC" ]; then
    echo "Error: Cieto executable path was not provided."
    echo "Usage:"
    echo "  bash run_tests.sh /path/to/cieto [timeout_seconds]"
    exit 1
fi

if [ ! -f "$CIETO_EXEC" ]; then
    echo "Error: Cieto executable not found:"
    echo "  $CIETO_EXEC"
    exit 1
fi

PASSED=0
FAILED=0
TIMEOUTS=0

rm -f "$LOG_FILE"

echo "Starting tests..."
echo "CIETO_EXEC: $CIETO_EXEC"
echo "TIMEOUT_SEC: $TIMEOUT_SEC"
echo

for file in test_*.cies; do
    if [ ! -f "$file" ]; then
        continue
    fi

    printf "Running %-35s " "$file"

    timeout "$TIMEOUT_SEC" "$CIETO_EXEC" "$file" > "$LOG_FILE" 2>&1
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
