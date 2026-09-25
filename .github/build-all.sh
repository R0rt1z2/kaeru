#!/usr/bin/env bash
#
# SPDX-FileCopyrightText: 2026 Roger Ortiz <roger@r0rt1z2.com>
# SPDX-License-Identifier: AGPL-3.0-or-later
#

set -u

procs=$(nproc --all)
failed=()
passed=0

# Keep the same build ID for every device, it's only useful for
# comparing builds and a -dirty suffix would be misleading here...
export KAERU_GIT_SHA=${KAERU_GIT_SHA:-$(git rev-parse --short HEAD 2>/dev/null)}

if [ $# -gt 0 ]; then
    configs=()
    for device in "$@"; do
        configs+=($(find configs -name "${device}_defconfig" -type f))
    done
else
    mapfile -t configs < <(find configs -name '*_defconfig' -type f | sort)
fi

if [ ${#configs[@]} -eq 0 ]; then
    echo "No defconfigs found!"
    exit 1
fi

echo "Building ${#configs[@]} device(s) with $procs job(s)."

log=$(mktemp)
trap 'rm -f "$log"' EXIT

for config in "${configs[@]}"; do
    device=$(basename "$config" _defconfig)
    vendor=$(basename "$(dirname "$config")")

    make distclean >/dev/null 2>&1

    if make "${device}_defconfig" >"$log" 2>&1 && make -j"$procs" >>"$log" 2>&1; then
        echo "  OK    $vendor/$device"
        passed=$((passed + 1))
    else
        echo "  FAIL  $vendor/$device"
        failed+=("$vendor/$device")

        [ -n "${GITHUB_ACTIONS:-}" ] && echo "::group::$vendor/$device build log"
        cat "$log"
        [ -n "${GITHUB_ACTIONS:-}" ] && echo "::endgroup::"
        [ -n "${GITHUB_ACTIONS:-}" ] && echo "::error title=Build failed::$vendor/$device"
    fi
done

make distclean >/dev/null 2>&1

echo
echo "$passed passed, ${#failed[@]} failed."

if [ -n "${GITHUB_STEP_SUMMARY:-}" ]; then
    {
        echo "### Device builds"
        echo
        echo "$passed passed, ${#failed[@]} failed."
        if [ ${#failed[@]} -gt 0 ]; then
            echo
            printf -- '- `%s`\n' "${failed[@]}"
        fi
    } >>"$GITHUB_STEP_SUMMARY"
fi

[ ${#failed[@]} -eq 0 ]