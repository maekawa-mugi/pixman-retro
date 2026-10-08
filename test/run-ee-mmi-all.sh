#!/bin/sh
# One-shot PS2 Linux verification. Arguments: [suite] [Pixman test dir].
# Collects ALL results, compares API outputs across EE/C, and exits nonzero
# if any test, benchmark, or checksum comparison fails.
SUITE=${1:-./ee-mmi-suite}
TEST_DIR=${2:-}
FAIL=0
unset PIXMAN_DISABLE
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
run_capture()
{
    label=$1
    path=$2
    shift 2
    echo "EE_RUN_BEGIN,$label"
    "$@" > "$path" 2>&1
    rc=$?
    cat "$path"
    echo "EE_RUN_END,$label,$rc"
    if [ "$rc" -ne 0 ]; then FAIL=$((FAIL + 1)); fi
}
echo "EE_RUN_PLATFORM_BEGIN"
uname -a
echo "EE_RUN_PLATFORM_END"
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

    if [ -x "$TEST_DIR/ee-mmi-pixman-bench" ]; then
        tmpbase=${TMPDIR:-.}/ee-mmi-pixman.$$
        ee_file=$tmpbase.ee
        c_file=$tmpbase.c
        run_capture "pixman-bench:EE" "$ee_file" "$TEST_DIR/ee-mmi-pixman-bench"
        PIXMAN_DISABLE=ee-mmi
        export PIXMAN_DISABLE
        run_capture "pixman-bench:C" "$c_file" "$TEST_DIR/ee-mmi-pixman-bench"
        unset PIXMAN_DISABLE

        if command -v awk >/dev/null 2>&1; then
            awk -F, '
                FNR==NR {
                    if ($1=="PIXMAN_BENCH") {
                        key=$2","$3","$4
                        ref[key]=$7
                        nref++
                    }
                    next
                }
                $1=="PIXMAN_BENCH" {
                    key=$2","$3","$4
                    if (!(key in ref) || ref[key]!=$7) {
                        print "EE_PIXEL_MISMATCH," key ",EE=" ref[key] ",C=" $7
                        bad++
                    }
                    nseen++
                }
                END {
                    if (nref!=42 || nseen!=42) {
                        print "EE_CHECKSUM_INCOMPLETE,EE=" nref ",C=" nseen
                        bad++
                    }
                    print "EE_CHECKSUM_RESULT," (bad?"FAIL":"PASS") ",mismatches=" bad+0
                    if (bad) exit 1
                }
            ' "$ee_file" "$c_file"
            rc=$?
            if [ "$rc" -ne 0 ]; then FAIL=$((FAIL + 1)); fi
        else
            echo "EE_CHECKSUM_UNAVAILABLE,awk_not_installed"
            FAIL=$((FAIL + 1))
        fi
        rm -f "$ee_file" "$c_file"
    else
        echo "EE_TEST_MISSING,ee-mmi-pixman-bench"
        FAIL=$((FAIL + 1))
    fi
fi
echo "EE_ALL_RESULT,failures=$FAIL"
if [ "$FAIL" -ne 0 ]; then exit 1; fi
exit 0
