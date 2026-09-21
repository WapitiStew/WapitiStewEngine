#!/usr/bin/env sh
set -eu

if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <directory-containing-arm64-debug-artifacts>" >&2
    exit 2
fi

artifact_dir=$1
machine=$(uname -m)
case "$machine" in
    aarch64|arm64) ;;
    *)
        echo "ERROR: Raspberry Pi ARM64 is required; uname -m reported: $machine" >&2
        exit 1
        ;;
esac

required_files="
libWonderStewEngine_d.so
wse.core.characterization
wse.core.runtime_contract
wse.xpt.tcp_loopback
wse.xpt.udp_loopback
wse.xpt.retry_policy_contract
wse.xpt.http_contract
wse.xpt.serial_port_contract
"

for name in $required_files; do
    if [ ! -f "$artifact_dir/$name" ]; then
        echo "ERROR: missing ARM64 smoke artifact: $artifact_dir/$name" >&2
        exit 1
    fi
done

export LD_LIBRARY_PATH="$artifact_dir${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

if ldd "$artifact_dir/wse.xpt.http_contract" | grep -q "not found"; then
    echo "ERROR: unresolved runtime dependency" >&2
    ldd "$artifact_dir/wse.xpt.http_contract" >&2
    exit 1
fi

for test_name in \
    wse.core.characterization \
    wse.core.runtime_contract \
    wse.xpt.tcp_loopback \
    wse.xpt.udp_loopback \
    wse.xpt.retry_policy_contract \
    wse.xpt.http_contract \
    wse.xpt.serial_port_contract
do
    echo "[wse-pi-smoke] RUN $test_name"
    "$artifact_dir/$test_name"
done

echo "[wse-pi-smoke] PASS machine=$machine"
