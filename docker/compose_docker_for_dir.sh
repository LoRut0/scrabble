#!/usr/bin/env bash
export UID=$(id -u)
export GID=$(id -g)
export DIR=$(readlink -f $1)
SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)

docker compose -f $SCRIPT_DIR/docker-compose.yml run --service-ports dev bash
# docker compose -f $SCRIPT_DIR/docker-compose.yml build dev
