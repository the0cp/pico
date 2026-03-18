#!/bin/bash

cd "$(dirname "$0")" || exit 1

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BOLD='\033[1m'
NC='\033[0m'

TIMEOUT_SEC=5

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
timeout_cnt=0

shopt -s nullglob
test_files=(test_*.pcs)
if [ ${#test_files[@]} -eq 0 ]; then
    echo "No test files found in $(pwd)."
    exit 1
fi

echo -e "${BOLD}Starting tests...${NC}\n"

for file in "${test_files[@]}"; do
    printf "Running %-35s " "$file"

    timeout "$TIMEOUT_SEC" "$PICO_EXEC" "$file" > "$LOG_FILE" 2>&1
    exit_code=$?

    if [ $exit_code -eq 0 ]; then
        printf "${GREEN}[PASS]${NC}\n"
        passed=$((passed + 1))
    elif [ $exit_code -eq 124 ]; then
        printf "${YELLOW}[TIMEOUT] (> %ds)${NC}\n" "$TIMEOUT_SEC"
        cat "$LOG_FILE"
        timeout_cnt=$((timeout_cnt + 1))
        failed=$((failed + 1))
    else
        printf "${RED}[FAIL]${NC}\n"
        cat "$LOG_FILE"
        failed=$((failed + 1))
    fi
done

rm -f "$LOG_FILE"

total=$((passed + failed))

echo -e "\n${BOLD}========================================${NC}"
if [ $failed -eq 0 ]; then
    echo -e "${GREEN}All tests passed! ($passed/$total)${NC}"
else
    echo -e "${RED}Summary: $passed Passed, $failed Failed ($timeout_cnt Timeouts)${NC}"
fi
echo -e "${BOLD}========================================${NC}"

if [ $failed -ne 0 ]; then
    exit 1
fi