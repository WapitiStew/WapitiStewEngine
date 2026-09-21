#!/usr/bin/env sh
set -eu

usage()
{
    echo "Usage: $0 [--help]"
    echo "Prints a privacy-filtered Raspberry Pi 4 environment inventory."
}

if [ "$#" -gt 1 ]; then
    usage >&2
    exit 2
fi
if [ "$#" -eq 1 ]; then
    case "$1" in
        -h|--help)
            usage
            exit 0
            ;;
        *)
            usage >&2
            exit 2
            ;;
    esac
fi

emit()
{
    printf '%s=%s\n' "$1" "$2"
}

command_available()
{
    command -v "$1" >/dev/null 2>&1
}

os_release_value()
{
    if [ ! -r /etc/os-release ]; then
        printf 'unknown'
        return
    fi
    sed -n "s/^$1=//p" /etc/os-release |
        sed 's/^"//;s/"$//' |
        sed -n '1p'
}

count_nodes()
{
    directory=$1
    pattern=$2
    if [ ! -d "$directory" ]; then
        printf '0'
        return
    fi
    find "$directory" -maxdepth 1 -type c -name "$pattern" 2>/dev/null |
        wc -l |
        tr -d ' '
}

machine=$(uname -m 2>/dev/null || printf 'unknown')
case "$machine" in
    aarch64|arm64) ;;
    *)
        echo "ERROR: Raspberry Pi OS 64-bit is required; machine architecture is not ARM64." >&2
        exit 1
        ;;
esac

pi_generation=unknown
if [ -r /proc/device-tree/model ]; then
    pi_model=$(tr -d '\000' < /proc/device-tree/model 2>/dev/null || true)
    case "$pi_model" in
        *"Raspberry Pi 4"*) pi_generation=4 ;;
        *"Raspberry Pi 5"*) pi_generation=5 ;;
        *"Raspberry Pi"*) pi_generation=other ;;
    esac
fi

emit wse.pi.inventory_schema 1
emit platform.machine "$machine"
emit platform.pi_generation "$pi_generation"
emit os.id "$(os_release_value ID)"
emit os.version_id "$(os_release_value VERSION_ID)"
emit kernel.release "$(uname -r 2>/dev/null || printf 'unknown')"
emit memory.total_kib "$(awk '/^MemTotal:/ { print $2; exit }' /proc/meminfo 2>/dev/null || printf 'unknown')"
emit storage.root_available_kib "$(df -Pk / 2>/dev/null | awk 'NR == 2 { print $4 }' || printf 'unknown')"

for tool in cmake ctest gcc g++ ninja make pkg-config git python3 node java javac \
    vulkaninfo wayland-info modetest v4l2-ctl libcamera-hello vcgencmd
do
    if command_available "$tool"; then
        emit "tool.$tool" available
    else
        emit "tool.$tool" missing
    fi
done

if command_available cmake; then
    emit tool.cmake_version "$(cmake --version 2>/dev/null | sed -n '1s/^cmake version //p')"
fi
if command_available gcc; then
    emit tool.gcc_version "$(gcc -dumpfullversion -dumpversion 2>/dev/null || printf 'unknown')"
fi
if command_available g++; then
    emit tool.gxx_version "$(g++ -dumpfullversion -dumpversion 2>/dev/null || printf 'unknown')"
fi
if command_available python3; then
    emit tool.python_version "$(python3 --version 2>&1 | sed -n '1s/^Python //p')"
fi
if command_available node; then
    emit tool.node_version "$(node --version 2>/dev/null || printf 'unknown')"
fi
if command_available java; then
    emit tool.java_version "$(java -version 2>&1 | sed -n '1s/.*version "\([^"]*\)".*/\1/p')"
fi

session_type=headless
case "${XDG_SESSION_TYPE:-}" in
    wayland) session_type=wayland ;;
    x11) session_type=x11 ;;
    tty) session_type=tty ;;
esac
emit display.session_type "$session_type"
emit display.drm_card_count "$(count_nodes /dev/dri 'card*')"
emit display.drm_render_count "$(count_nodes /dev/dri 'renderD*')"
emit camera.video_node_count "$(count_nodes /dev 'video*')"

vulkan_api_version=unavailable
if command_available vulkaninfo; then
    vulkan_summary=$(vulkaninfo --summary 2>/dev/null || true)
    discovered_version=$(printf '%s\n' "$vulkan_summary" |
        sed -n 's/^[[:space:]]*apiVersion[[:space:]]*=[[:space:]]*//p' |
        sed -n '1p')
    if [ -n "$discovered_version" ]; then
        vulkan_api_version=$discovered_version
    fi
fi
emit display.vulkan_api_version "$vulkan_api_version"

default_route=missing
if command_available ip && ip route show default 2>/dev/null | grep -q .; then
    default_route=available
fi
emit network.default_route "$default_route"

ssh_listener=unavailable
if command_available ss; then
    ssh_listener=not_listening
    if ss -ltn 2>/dev/null | awk '{ print $4 }' | grep -Eq '(^|:|\])22$'; then
        ssh_listener=listening
    fi
fi
emit network.ssh_listener "$ssh_listener"

throttled=unavailable
temperature=unavailable
if command_available vcgencmd; then
    throttled=$(vcgencmd get_throttled 2>/dev/null | sed -n 's/^throttled=//p')
    temperature=$(vcgencmd measure_temp 2>/dev/null | sed -n "s/^temp=\([^']*\).*/\1/p")
    [ -n "$throttled" ] || throttled=unavailable
    [ -n "$temperature" ] || temperature=unavailable
fi
emit health.throttled "$throttled"
emit health.temperature_c "$temperature"

emit privacy.hostname omitted
emit privacy.network_addresses omitted
emit privacy.account_name omitted
emit privacy.device_identifiers omitted
