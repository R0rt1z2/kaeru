#!/bin/bash

CONTAINER_NAME="kaeru"
DOCKERFILE="Dockerfile"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if ! docker image inspect "$CONTAINER_NAME" &> /dev/null; then
    echo "Container not found, building..."
    docker build -t "$CONTAINER_NAME" -f "$SCRIPT_DIR/$DOCKERFILE" "$SCRIPT_DIR"
fi

docker run -it --rm \
    -v "$SCRIPT_DIR":/kaeru \
    -w /kaeru \
    "$CONTAINER_NAME" \
    bash