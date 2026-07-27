#!/usr/bin/env bash

set -e

CONTAINER_NAME="kaeru"
DOCKERFILE="Dockerfile"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

FORCE_REBUILD=false

usage() {
    cat <<EOF
Usage: $0 [-f|--force] [command [args...]]

Commands:
  build <codename> <bootloader> [-d]   Build an image, see build.sh
  shell                                Open a shell, this is the default
  <command...>                         Run anything else in the container

Options:
  -f, --force    Force rebuild of the Docker image
  -h, --help     Show this help

Examples:
  $0 build lamu lamu.img
  $0 build lamu lamu.img -d
  $0 make lancelot_defconfig
EOF
    exit "${1:-0}"
}

while [[ $# -gt 0 ]]; do
    case $1 in
        -f|--force)
            FORCE_REBUILD=true
            shift
            ;;
        -h|--help)
            usage
            ;;
        *)
            break
            ;;
    esac
done

if [ "$FORCE_REBUILD" = true ] || ! docker image inspect "$CONTAINER_NAME" &> /dev/null; then
    if [ "$FORCE_REBUILD" = true ]; then
        echo "Force rebuilding container..."
    else
        echo "Container not found, building..."
    fi
    docker build -t "$CONTAINER_NAME" -f "$SCRIPT_DIR/$DOCKERFILE" "$SCRIPT_DIR"
fi

case "${1:-shell}" in
    build)
        shift
        COMMAND=(./build.sh "$@")
        ;;
    shell)
        COMMAND=(bash)
        ;;
    *)
        COMMAND=("$@")
        ;;
esac

TTY_ARGS=(-i)
[ -t 0 ] && TTY_ARGS+=(-t)

IDS_DIR="$(mktemp -d)"
trap 'rm -rf "$IDS_DIR"' EXIT

printf 'root:x:0:0:root:/root:/bin/bash\nkaeru:x:%s:%s:kaeru:/tmp:/bin/bash\n' \
    "$(id -u)" "$(id -g)" > "$IDS_DIR/passwd"
printf 'root:x:0:\nkaeru:x:%s:\n' "$(id -g)" > "$IDS_DIR/group"

docker run --rm "${TTY_ARGS[@]}" \
    -v "$SCRIPT_DIR:/kaeru" \
    -v "$IDS_DIR/passwd:/etc/passwd:ro" \
    -v "$IDS_DIR/group:/etc/group:ro" \
    -w /kaeru \
    --user "$(id -u):$(id -g)" \
    -e HOME=/tmp \
    -e GIT_CONFIG_COUNT=1 \
    -e GIT_CONFIG_KEY_0=safe.directory \
    -e GIT_CONFIG_VALUE_0='*' \
    "$CONTAINER_NAME" \
    "${COMMAND[@]}"
