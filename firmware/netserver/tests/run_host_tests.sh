#!/usr/bin/env bash
set -euo pipefail

# Exercise the production network/reporter code against a simulated ESP-IDF host.
test_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
test_build_dir=$(mktemp -d /tmp/netserver-host-tests.XXXXXX)

cc -std=gnu17 -D_GNU_SOURCE -Dtime=mock_time -Dsettimeofday=mock_settimeofday \
    -Wall -Wextra -Werror -pthread -fsanitize=address,undefined \
    -I "$test_root/tests/host/stubs" -I "$test_root/main" \
    "$test_root/main/network.c" "$test_root/main/reporter.c" \
    "$test_root/tests/host/check.c" -o "$test_build_dir/check"

# The worker lives until process exit; LeakSanitizer is unavailable in this sandbox.
ASAN_OPTIONS=detect_leaks=0 "$test_build_dir/check"
