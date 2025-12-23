#!/bin/bash

RED='\033[0;31m'
GREEN='\033[0;32m'
BOLD='\033[1m'
NC='\033[0m'

PICO_EXEC="../build/pico"
LOG_FILE="test_output.log"

if [ -f "../build/pico.exe" ]; then
    PICO_EXEC="../build/pico.exe"
elif [ -f "../build/pico" ]; then
    PICO_EXEC="../build/pico"
else
    PICO_EXEC=$(find ../build -name "pico*" -type f | head -n 1)
fi

passed=0
failed=0

for file in test_*.pcs; do
    if [ ! -e "$file" ]; then
        break
    fi

    "$PICO_EXEC" "$file" > "$LOG_FILE" 2>&1
    if [ $? -eq 0 ]; then
        printf "${GREEN}[PASS]${NC} %s\n" "$file"
        passed=$((passed + 1))
    else
        printf "${RED}[FAIL]${NC} %s\n" "$file"
        cat "$LOG_FILE"
        failed=$((failed + 1))
    fi
    echo ""
done

rm -f "$LOG_FILE"

total=$((passed + failed))

echo -e "${BOLD}========================================${NC}"
if [ $failed -eq 0 ]; then
    echo -e "${GREEN}All tests passed! ($passed/$total)${NC}"
else
    echo -e "${RED}Summary: $passed Passed, $failed Failed${NC}"
fi
echo -e "${BOLD}========================================${NC}"

if [ $failed -ne 0 ]; then
    exit 1
fi