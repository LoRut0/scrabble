#!/usr/bin/env bash
export HOST_UID=$(id -u)
export HOST_GID=$(id -g)
export DIR=$(readlink -f "$1")
SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)

# docker compose -f $SCRIPT_DIR/docker-compose-main.yml build
# docker compose -f $SCRIPT_DIR/docker-compose-dev.yml build
docker compose -f "$SCRIPT_DIR/docker-compose-dev.yml" up -d
docker attach scryablya-dev
