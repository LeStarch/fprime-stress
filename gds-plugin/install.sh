#!/usr/bin/env bash
# Installs the doom-display fprime-gds addon into the active fprime-gds
# installation.
set -euo pipefail

FPRIME_GDS=$(python3 -c "import os, fprime_gds; print(os.path.dirname(fprime_gds.__file__))")
ADDONS_DIR="$FPRIME_GDS/flask/static/addons"
ENABLED_JS="$ADDONS_DIR/enabled.js"
SRC_DIR="$(cd "$(dirname "$0")" && pwd)/doom-display"

if [ ! -d "$ADDONS_DIR" ]; then
    echo "fprime-gds addons dir not found: $ADDONS_DIR" >&2
    exit 1
fi

echo "Installing doom-display into $ADDONS_DIR"
cp -r "$SRC_DIR" "$ADDONS_DIR/"

if ! grep -q "doom-display/addon.js" "$ENABLED_JS"; then
    echo 'import "./doom-display/addon.js";' >> "$ENABLED_JS"
    echo "Patched $ENABLED_JS"
fi

echo "Done. Restart fprime-gds to pick up the addon."
