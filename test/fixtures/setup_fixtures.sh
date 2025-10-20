#!/bin/bash

set -e

TEST_FIXTURES_DIR="test/fixtures/audio_samples"
GITHUB_REPO="https://github.com/ietf-wg-cellar/flac-test-files.git"

echo "[+] Setting up FLAC test fixtures..."

mkdir -p "${TEST_FIXTURES_DIR}"
mkdir -p "${TEST_FIXTURES_DIR}/subset"
mkdir -p "${TEST_FIXTURES_DIR}/subset/stereo"
mkdir -p "${TEST_FIXTURES_DIR}/subset/high_resolution"
mkdir -p "${TEST_FIXTURES_DIR}/subset/surround"
mkdir -p "${TEST_FIXTURES_DIR}/subset/metadata_extremes"
mkdir -p "${TEST_FIXTURES_DIR}/subset/misc"
mkdir -p "${TEST_FIXTURES_DIR}/faulty"
mkdir -p "${TEST_FIXTURES_DIR}/uncommon"

TEMP_DIR=$(mktemp -d)
trap "rm -rf ${TEMP_DIR}" EXIT

echo "[+] Cloning FLAC test files respository..."
git clone --depth=1 "${GITHUB_REPO}" "${TEMP_DIR}"

echo "[+] Organizing test files..."

if [ -d "${TEMP_DIR}/subset" ]; then
  for file in "${TEMP_DIR}/subset/"*.flac; do
    filename=$(basename "$file")
    number=$(echo "$filename" | cut -d' ' -f1)
    number=$((10#$number))

    if ((number >= 1 && number <= 27)); then
      cp "$file" "${TEST_FIXTURES_DIR}/subset/stereo/" 2>/dev/null || true
    elif ((number >= 28 && number <= 37)); then
      cp "$file" "${TEST_FIXTURES_DIR}/subset/high_resolution/" 2>/dev/null || true
    elif ((number >= 38 && number <= 44)); then
      cp "$file" "${TEST_FIXTURES_DIR}/subset/surround/" 2>/dev/null || true
    elif ((number >= 45 && number <= 59)); then
      cp "$file" "${TEST_FIXTURES_DIR}/subset/metadata_extremes/" 2>/dev/null || true
    wlif ((number >= 60 && number <= 64)); then
      cp "$file" "${TEST_FIXTURES_DIR}/subset/misc/" 2>/dev/null || true
    else
      echo "[+] Skipping $filename: number $number out of defined ranges."
  done
fi

if [ -d "${TEMP_DIR}/faulty" ]; then
  cp "${TEMP_DIR}/faulty"/*.flac "${TEST_FIXTURES_DIR}/faulty/" 2>/dev/null || true
fi

if [ -d "${TEMP_DIR}/uncommon" ]; then
  cp "${TEMP_DIR}/uncommon"/*.flac "${TEST_FIXTURES_DIR}/uncommon" 2>/dev/null || true
fi

echo "[+] Test fixtures installed!"
echo ""
echo "[+] Directory structure:"
tree "${TEST_FIXTURES_DIR}" -d 2>/dev/null || find "${TEST_FIXTURES_DIR}" -type d

echo ""
echo "[+] Sample count:"
echo "[+]  Subset group: $(find "${TEST_FIXTURES_DIR}/subset" -name "*.flac" | wc -l)" 
echo "[+]  Faulty group: $(find "${TEST_FIXTURES_DIR}/faulty" -name "*.flac" | wc -l)" 
echo "[+]  Uncommon group: $(find "${TEST_FIXTURES_DIR}/uncommon" -name "*.flac" | wc -l)" 
