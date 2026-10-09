#!/bin/sh
# Package one archive for PS2 Linux. Run from the source repository root.
# Usage: sh test/package-ee-mmi.sh build-ee/test [output.tar.gz]
set -eu
TEST_BUILD=${1:-build-ee/test}
OUT=${2:-ee-mmi-test-kit.tar.gz}
if [ ! -f "./ee-mmi-suite" ]; then
    echo "Missing ./ee-mmi-suite: build test/Makefile.ee-mmi first" >&2
    exit 1
fi
WORK=${TMPDIR:-/tmp}/ee-mmi-package.$$
KIT=$WORK/ee-mmi-kit
trap 'rm -rf "$WORK"' 0 1 2 3 15
mkdir -p "$KIT/tests"
cp ./ee-mmi-suite "$KIT/ee-mmi-suite"
cp test/run-ee-mmi-all.sh "$KIT/run.sh"
for exe in composite combiner-test blitters-test neg-stride-test scaling-test ee-mmi-pixman-bench; do
    if [ ! -f "$TEST_BUILD/$exe" ]; then
        echo "Missing $TEST_BUILD/$exe" >&2
        exit 1
    fi
    cp "$TEST_BUILD/$exe" "$KIT/tests/$exe"
done
# Copy more Pixman validation binaries when available for EE_FULL=1.
for exe in pixel-test fetch-test solid-test cover-test scaling-helpers-test affine-test composite-traps-test glyph-test; do
    if [ -f "$TEST_BUILD/$exe" ]; then
        cp "$TEST_BUILD/$exe" "$KIT/tests/$exe"
    fi
done
cat > "$KIT/README-RUN.txt" <<'EOF'
PS2 Linux / R5900 one-shot EE MMI validation:

  cd ee-mmi-kit
  EE_FULL=1 sh ./run.sh ./ee-mmi-suite ./tests > ee-mmi-results.log 2>&1
  echo "EXIT_CODE=$?" >> ee-mmi-results.log

Transfer ee-mmi-results.log back for review. Do not discard this log.
All required tests run with MMI enabled and disabled. The Pixman
benchmark output checksums are automatically compared; any difference
is reported as EE_PIXEL_MISMATCH and fails the validation run.

This package assumes the binaries and linked Pixman libraries can run
on your PS2 Linux system. Bundle required shared libraries or build
static binaries if they are not already installed on the target.
EOF
tar -czf "$OUT" -C "$WORK" ee-mmi-kit
echo "EE_PACKAGE_CREATED,$OUT"
