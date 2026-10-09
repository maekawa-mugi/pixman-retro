#!/usr/bin/env python3
"""Idempotent harness upgrade for ZIP v5 all-PR source.

Place beside test/ps2/progress.c and run with Python 3.
Experimental assembly/test/model files must be copied alongside it.
"""
from pathlib import Path

root = Path(__file__).resolve().parents[2]


def swap(name, before, after):
    path = root / name
    src = path.read_text(encoding="utf-8")
    if after in src:
        print("ALREADY", name)
        return
    if src.count(before) != 1:
        raise RuntimeError(f"{name}: expected one anchor, found {src.count(before)}")
    path.write_text(src.replace(before, after), encoding="utf-8")
    print("UPDATED", name)


swap("test/ps2/progress.c",
     "    if(current>=0)\n    {\n        for(side=0;side<2;++side)",
     """    /* Group-level errors with no pixel mismatch used to display O/O.
     * Neither side can be attributed, so conservatively mark both X.
     */
    if (current >= 0 && failed &&
        !cells[current][0].bad && !cells[current][1].bad)
    {
        cells[current][0].bad = cells[current][1].bad = 1;
        printf("GROUP_UNATTRIBUTED_FAILURE,%s\\n", name);
    }
    if(current>=0)
    {
        for(side=0;side<2;++side)""")

swap("test/ps2/main.c",
     "extern int ee_kernel_suite_main(int argc, char **argv);",
     """extern int ee_kernel_suite_main(int argc, char **argv);
extern int ps2_latency_run(void);""")

swap("test/ps2/main.c",
     """        failures += i;

    }
    /* Keep two entirely separate chains""",
     """        failures += i;

    }
    /* A/B/C experiment does not alter production kernel dispatch. */
    failures += ps2_latency_run();
    /* Keep two entirely separate chains""")

swap("test/ps2/main.c",
     """    fail=test?test():composites(group);
    ps2_progress_group(name,fail,ps2_elapsed_us(start));""",
     """    fail=test?test():composites(group);
#ifdef PS2_TEST_HOST
    /* Negative control: fail after every pixel comparison has passed. */
    if (getenv("PS2_PROGRESS_FORCE_GROUP_FAILURE") &&
        !strcmp(name, "Scaled OVER"))
        fail = 1;
#endif
    ps2_progress_group(name,fail,ps2_elapsed_us(start));""")

flags = "$" + "{flags[@]}"
swap("test/ps2/build.sh",
     'suite_objects=("$output_dir/benchmark.o" "$output_dir/progress.o")',
     f'''"$cc" "{flags}" -std=gnu99 -c "$source_dir/test/ps2/latency-variants-test.c" -o "$output_dir/latency-variants-test.o"
"$cc" "{flags}" -c "$source_dir/test/ps2/latency-variants-asm.S" -o "$output_dir/latency-variants-asm.o"
suite_objects=("$output_dir/benchmark.o" "$output_dir/progress.o" "$output_dir/latency-variants-test.o" "$output_dir/latency-variants-asm.o")''')

swap("test/ps2/verify-host.sh",
     '"$root/test/ee-mmi-host-reference.c" \\\n',
     '"$root/test/ee-mmi-host-reference.c" \\\n    "$root/test/ps2/latency-variants-test.c" "$root/test/ps2/latency-variants-host.c" \\\n')

swap("test/ps2/verify-host.sh",
     '-lm -pthread -o "$out/runner"\n"$out/runner"\n',
     '''-lm -pthread -o "$out/runner"
"$out/runner"
# Host-only negative controls must both fail, and for the expected reason.
if PS2_LATENCY_TEST_FAULT=1 "$out/runner" > "$out/latency-fault.log" 2>&1; then
    echo 'ERROR: latency mismatch escaped the harness' >&2; exit 1
fi
grep -q 'LATENCY_FAIL,nearest_u4' "$out/latency-fault.log" || {
    echo 'ERROR: latency mismatch not identified' >&2; exit 1;
}
if PS2_PROGRESS_FORCE_GROUP_FAILURE=1 "$out/runner" > "$out/group-fault.log" 2>&1; then
    echo 'ERROR: group failure escaped the harness' >&2; exit 1
fi
grep -q 'GROUP_UNATTRIBUTED_FAILURE,Scaled OVER' "$out/group-fault.log" || {
    echo 'ERROR: green O/O despite failed group' >&2; exit 1;
}
for side in A B; do
    grep -q "SUITE_AB_RESULT,Scaled OVER,$side,X" "$out/group-fault.log" || {
        echo "ERROR: side $side remained green after group failure" >&2; exit 1;
    }
done
echo 'HOST_NEGATIVE_CONTROLS_PASS,latency,group-status'
''')

print("V5_HARNESS_PATCH_PASS")
