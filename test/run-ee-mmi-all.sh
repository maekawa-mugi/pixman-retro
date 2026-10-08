#!/bin/sh
# Run once on PS2 Linux after copying ee-mmi-suite and Pixman test binaries.
# Arguments: [suite-path] [optional-Pixman-test-directory].
# Continues after failures, prints a final summary and returns nonzero.
SUITE=${1:-./ee-mmi-suite}
TEST_DIR=${2:-}
FAIL=0
run_it()
{
    label=$1
    shift
    echo "EE_RUN_BEGIN,$label"
    "$@"
    rc=$?
    echo "EE_RUN_END,$label,$rc"
    if [ "$rc" -ne 0 ]; then FAIL=$((FAIL + 1)); fi
}
run_it suite "$SUITE"
if [ -n "$TEST_DIR" ]; then
    for name in composite combiner-test blitters-test neg-stride-test scaling-test; do
        if [ -x "$TEST_DIR/$name" ]; then
            run_it "$name:EE" "$TEST_DIR/$name"
            PIXMAN_DISABLE=ee-mmi
            export PIXMAN_DISABLE
            run_it "$name:C" "$TEST_DIR/$name"
            unset PIXMAN_DISABLE
        else
            echo "EE_TEST_MISSING,$name"
            FAIL=$((FAIL + 1))
        fi
    done
fi
echo "EE_ALL_RESULT,failures=$FAIL"
if [ "$FAIL" -ne 0 ]; then exit 1; fi
exit 0
