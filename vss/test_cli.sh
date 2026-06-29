#!/usr/bin/env bash
# =============================================================================
# VSS CLI Test Suite
# Tests that the argument dispatcher works correctly for all documented modes.
# Usage: bash test_cli.sh [path/to/vss]
# =============================================================================
set -euo pipefail

VSS=${1:-"./vss"}
PASS=0
FAIL=0

# ── Helpers ───────────────────────────────────────────────────────────────────
ok() {
    echo "  [PASS] $1"
    PASS=$((PASS + 1))
}

fail() {
    echo "  [FAIL] $1"
    echo "         Expected: $2"
    echo "         Got:      $3"
    FAIL=$((FAIL + 1))
}

assert_exit() {
    local desc="$1" expected_exit="$2"
    shift 2
    local actual_exit=0
    "$@" >/dev/null 2>&1 || actual_exit=$?
    if [ "$actual_exit" -eq "$expected_exit" ]; then
        ok "$desc"
    else
        fail "$desc" "exit $expected_exit" "exit $actual_exit"
    fi
}

assert_output_contains() {
    local desc="$1" pattern="$2"
    shift 2
    local output
    output=$("$@" 2>&1 || true)
    if echo "$output" | grep -q "$pattern"; then
        ok "$desc"
    else
        fail "$desc" "output containing '$pattern'" "$output"
    fi
}

assert_not_output_contains() {
    local desc="$1" pattern="$2"
    shift 2
    local output
    output=$("$@" 2>&1 || true)
    if echo "$output" | grep -q "$pattern"; then
        fail "$desc" "output NOT containing '$pattern'" "$output"
    else
        ok "$desc"
    fi
}

# ── Setup: create temp .vss files ─────────────────────────────────────────────
TMP_DIR=$(mktemp -d)
trap 'rm -rf "$TMP_DIR"' EXIT

# Resolve VSS binary to absolute path so cd subshells can still find it
VSS_ABS=$(cd "$(dirname "$VSS")" && pwd)/$(basename "$VSS")

cat > "$TMP_DIR/hello.vss" << 'EOF'
say "Hello from VSS!"
EOF

cat > "$TMP_DIR/add.vss" << 'EOF'
make a becomes 10
make b becomes 20
say a + b
EOF

echo ""
echo "=========================================="
echo " VSS CLI Test Suite"
echo "=========================================="
echo ""

# ── 1. No arguments → show help ───────────────────────────────────────────────
echo "[Group 1] No arguments"
assert_exit     "vss (no args) exits 0"          0  "$VSS"
assert_output_contains "vss (no args) prints help" "Usage" "$VSS"

# ── 2. Direct .vss file execution ────────────────────────────────────────────
echo ""
echo "[Group 2] Direct file execution"
assert_exit            "vss hello.vss exits 0"          0  "$VSS" "$TMP_DIR/hello.vss"
assert_output_contains "vss hello.vss prints output"    "Hello from VSS!" "$VSS" "$TMP_DIR/hello.vss"
assert_exit            "vss add.vss exits 0"            0  "$VSS" "$TMP_DIR/add.vss"
assert_output_contains "vss add.vss prints 30"          "30" "$VSS" "$TMP_DIR/add.vss"

# Relative path (run from TMP_DIR) - tests that vss resolves files relative to CWD
assert_exit            "vss relative path exits 0"      0  bash -c "cd '$TMP_DIR' && '$VSS_ABS' hello.vss"
assert_output_contains "vss relative path prints output" "Hello from VSS!" bash -c "cd '$TMP_DIR' && '$VSS_ABS' hello.vss"

# ── 3. Direct .vssc bytecode execution ───────────────────────────────────────
echo ""
echo "[Group 3] Bytecode file execution"
# Build a .vssc first, then run it
"$VSS" build "$TMP_DIR/hello.vss" >/dev/null 2>&1 || true
if [ -f "$TMP_DIR/hello.vssc" ]; then
    assert_exit            "vss hello.vssc exits 0"          0  "$VSS" "$TMP_DIR/hello.vssc"
    assert_output_contains "vss hello.vssc prints output"    "Hello from VSS!" "$VSS" "$TMP_DIR/hello.vssc"
else
    echo "  [SKIP] vss build did not produce hello.vssc (build may not be supported)"
fi

# ── 4. Missing .vss file → clear error, not "Unknown command" ─────────────────
echo ""
echo "[Group 4] File not found errors"
assert_exit                 "vss missing.vss exits nonzero"          1  "$VSS" "missing.vss"
assert_not_output_contains  "vss missing.vss: no 'Unknown command'"  "Unknown command" "$VSS" "missing.vss"
assert_output_contains      "vss missing.vss: prints 'not found'"    "not found" "$VSS" "missing.vss"

assert_exit                 "vss missing.vssc exits nonzero"         1  "$VSS" "missing.vssc"
assert_not_output_contains  "vss missing.vssc: no 'Unknown command'" "Unknown command" "$VSS" "missing.vssc"

# ── 5. Named commands ─────────────────────────────────────────────────────────
echo ""
echo "[Group 5] Named commands"
assert_exit            "vss version exits 0"                     0  "$VSS" version
assert_output_contains "vss version prints version string"       "VSS" "$VSS" version

assert_exit            "vss help exits 0"                        0  "$VSS" help
assert_output_contains "vss help prints usage"                   "Usage" "$VSS" help

assert_exit            "vss --help exits 0"                      0  "$VSS" --help
assert_exit            "vss -h exits 0"                          0  "$VSS" -h
assert_exit            "vss --version exits 0"                   0  "$VSS" --version

assert_exit            "vss run <file> exits 0"                  0  "$VSS" run "$TMP_DIR/hello.vss"
assert_output_contains "vss run <file> prints output"            "Hello from VSS!" "$VSS" run "$TMP_DIR/hello.vss"

assert_exit            "vss build <file> exits 0"                0  "$VSS" build "$TMP_DIR/hello.vss"

# ── 6. Invalid commands → typo suggestion, not a crash ───────────────────────
echo ""
echo "[Group 6] Invalid commands"
assert_exit            "vss foobar exits nonzero"                1  "$VSS" foobar
assert_exit            "vss vershun exits nonzero (typo)"        1  "$VSS" vershun
assert_output_contains "vss vershun suggests version"            "version" "$VSS" vershun
assert_exit            "vss bild exits nonzero (typo)"          1  "$VSS" bild
assert_output_contains "vss bild suggests build"                 "build" "$VSS" bild

# .vss files are NOT treated as unknown commands
assert_not_output_contains "vss hello.vss is not 'Unknown command'" "Unknown command" "$VSS" "$TMP_DIR/hello.vss"

# ── 7. package subcommand ─────────────────────────────────────────────────────
echo ""
echo "[Group 7] Package subcommand"
assert_exit            "vss package (no sub) exits 0"            0  "$VSS" package
assert_output_contains "vss package shows usage"                 "install" "$VSS" package

# ── Summary ───────────────────────────────────────────────────────────────────
echo ""
echo "=========================================="
TOTAL=$((PASS + FAIL))
echo " Results: $PASS/$TOTAL passed"
if [ "$FAIL" -gt 0 ]; then
    echo " $FAIL test(s) FAILED"
    echo "=========================================="
    exit 1
else
    echo " All tests passed!"
    echo "=========================================="
    exit 0
fi
