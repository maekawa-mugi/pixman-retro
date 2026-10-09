#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/../.." && pwd)
out="$root/build-ps2-mmi/host"
cc=${HOST_CC:-cc}
setup=(meson setup "$out/library" "$root" --default-library=static
       -Dtests=disabled -Ddemos=disabled -Dee-mmi=disabled -Dgtk=disabled -Dlibpng=disabled)
[[ ! -f "$out/library/meson-private/coredata.dat" ]] || setup+=(--reconfigure)
CC="$cc" "${setup[@]}"
meson compile -C "$out/library"
flags=(-O2 -g -std=gnu99 -fno-strict-aliasing -DHAVE_CONFIG_H -DUSE_EE_MMI
       -DPS2_TEST_HOST "-I$out/library/pixman" "-I$root/pixman")
objects=()
for name in ee-mmi ee-mmi-over ee-mmi-ca ee-mmi-nearest ee-mmi-bilinear; do
    "$cc" "${flags[@]}" -c "$root/pixman/pixman-$name.c" -o "$out/$name.o"
    objects+=("$out/$name.o")
done
wrap=()
for name in add copy fill in_a8 over 8888_to_0565 ca_over ca_over_solid over_solid_a8 over_image_a8 over_image_ca over_8888_0565 nearest_opaque bilinear_lerp bilinear_hsum bilinear_vfinish; do
    wrap+=("-Wl,--wrap=ee_mmi_${name}_quads")
done
suite_objects=()
model_flags=()
for name in add copy fill in_a8 over 8888_to_0565 ca_over ca_over_solid over_solid_a8 over_image_a8 over_image_ca over_8888_0565 nearest_opaque bilinear_lerp bilinear_hsum bilinear_vfinish; do
    model_flags+=("-Dee_mmi_${name}_quads=b_ee_mmi_${name}_quads")
done
"$cc" "${flags[@]}" "${model_flags[@]}" -c "$root/test/ee-mmi-host-reference.c" -o "$out/b-models.o"
suite_objects+=("$out/b-models.o")
for spec in 'ee-mmi-suite:ee_kernel_suite_main' 'ee-mmi-ca-fused-test:ee_test_ca' 'ee-mmi-a8-fused-test:ee_test_solid_a8' 'ee-mmi-565-fused-test:ee_test_565' 'ee-mmi-image-a8-fused-test:ee_test_image_a8' 'ee-mmi-image-ca-fused-test:ee_test_image_ca' 'ee-mmi-scale-test:ee_test_scale' 'ee-mmi-bilinear-4tap-test:ee_test_bilinear4'; do
    file=${spec%%:*}
    entry=${spec#*:}
    "$cc" "${flags[@]}" -DPS2_TEST_PROGRESS -Dmain="$entry" -c "$root/test/$file.c" -o "$out/$file.o"
    suite_objects+=("$out/$file.o")
done
"$cc" "${flags[@]}" -c "$root/test/ee-mmi-bench.c" -o "$out/ee-mmi-bench.o"
suite_objects+=("$out/ee-mmi-bench.o")
# Mirror the PS2SDK cross compiler's strict pointer-type diagnostic:
# a type-mismatched ?: expression must fail the host preflight as well.
"$cc" "${flags[@]}" -Werror=incompatible-pointer-types -fsyntax-only \
    "$root/test/ps2/throughput-variants-test.c"
"$cc" "${flags[@]}" "$root/test/ps2/main.c" "$root/test/ps2/benchmark.c" "$root/test/ps2/progress.c" "$root/test/ee-mmi-host-reference.c" \
    "$root/test/ps2/latency-variants-test.c" "$root/test/ps2/latency-variants-host.c" \
    "$root/test/ps2/throughput-variants-test.c" "$root/test/ps2/throughput-variants-host.c" \
    "${objects[@]}" "${suite_objects[@]}" "$out/library/pixman/libpixman-1.a" "${wrap[@]}" -lm -pthread -o "$out/runner"
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
if PS2_THROUGHPUT_TEST_FAULT=1 "$out/runner" > "$out/throughput-fault.log" 2>&1; then
    echo 'ERROR: throughput mismatch escaped the harness' >&2; exit 1
fi
grep -q 'THROUGHPUT_FAIL,ADD,u8,blocks=7' "$out/throughput-fault.log" || {
    echo 'ERROR: throughput injected mismatch not identified' >&2; exit 1;
}
echo 'HOST_NEGATIVE_CONTROLS_PASS,latency,group-status,throughput'
