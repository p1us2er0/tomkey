#!/usr/bin/env bash
# Runs inside the ZMK build container. Use scripts/build.sh instead.
#   /src  this repository, read only
#   /ws   persistent west workspace
#   /out  firmware/ of this repository
set -euo pipefail

force_update=0
filters=()
for arg in "$@"; do
    case "$arg" in
    --update) force_update=1 ;;
    *) filters+=("$arg") ;;
    esac
done

rm -rf /ws/config
cp -R /src/config /ws/config
cd /ws

[ -d .west ] || west init -l config

if [ "$force_update" = 1 ] || ! cmp -s config/west.yml .west.yml.last; then
    echo "Updating west projects..."
    west update --fetch-opt=--filter=tree:0
    cp config/west.yml .west.yml.last
fi

west zephyr-export > /dev/null

python3 - "${filters[@]+"${filters[@]}"}" <<'EOF'
import shutil
import subprocess
import sys

import yaml

filters = sys.argv[1:]
with open("/src/build.yaml") as f:
    targets = yaml.safe_load(f).get("include", [])

built = failed = 0
for target in targets:
    board = target["board"]
    shield = target.get("shield", "")
    # Same artifact naming as the GitHub Actions workflow.
    name = target.get("artifact-name") or (
        (f"{shield}-" if shield else "") + board.replace("/", "_") + "-zmk"
    )
    if filters and not any(f in name for f in filters):
        continue

    cmd = ["west", "build", "-s", "zmk/app", "-d", f"/ws/build/{name}", "-b", board]
    if target.get("snippet"):
        cmd += ["-S", target["snippet"]]
    cmd += ["--", "-DZMK_CONFIG=/ws/config", "-DZMK_EXTRA_MODULES=/src"]
    if shield:
        cmd.append(f"-DSHIELD={shield}")
    cmd += target.get("cmake-args", "").split()

    print(f"Building {name} ...", flush=True)
    with open(f"/out/{name}.log", "w") as log:
        result = subprocess.run(cmd, stdout=log, stderr=subprocess.STDOUT)
    if result.returncode != 0:
        print(f"  FAILED, see firmware/{name}.log", flush=True)
        failed += 1
        continue

    shutil.copy(f"/ws/build/{name}/zephyr/zmk.uf2", f"/out/{name}.uf2")
    print(f"  OK: firmware/{name}.uf2", flush=True)
    built += 1

if built + failed == 0:
    sys.exit(f"No target in build.yaml matches: {' '.join(filters)}")
sys.exit(1 if failed else 0)
EOF
