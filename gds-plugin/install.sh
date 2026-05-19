#!/usr/bin/env bash
# Installs the doom-display fprime-gds addon and flips the dashboards
# feature flag on inside the active fprime-gds Python package.
#
# Why this script exists:
#
# 1. fprime-gds discovers addons by importing modules listed in
#    `<fprime_gds>/flask/static/addons/enabled.js`. That file lives
#    inside the installed Python package, so registering an out-of-tree
#    addon requires copying the source in and appending an import line.
#
# 2. Dashboards (the customisable XML-driven panel screen that hosts
#    the doom-display tag) are gated by `config.enableDashboards` in
#    `<fprime_gds>/flask/static/js/config.js`. Some fprime-gds releases
#    ship that flag as `false` by default; this script flips it to
#    `true` so the dashboard tab actually renders.
#
# 3. The dashboard XML itself (gds-plugin/dashboard.xml, next to this
#    script) is uploaded by the operator via the Dashboard tab the
#    first time they open the GDS in a browser. fprime-gds does not
#    yet expose a CLI flag for this.
#
# All edits are idempotent: re-running the script is safe.
set -euo pipefail

FPRIME_GDS=$(python3 -c "import os, fprime_gds; print(os.path.dirname(fprime_gds.__file__))")
ADDONS_DIR="$FPRIME_GDS/flask/static/addons"
ENABLED_JS="$ADDONS_DIR/enabled.js"
CONFIG_JS="$FPRIME_GDS/flask/static/js/config.js"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SRC_DIR="$SCRIPT_DIR/doom-display"
DASHBOARD_XML="$SCRIPT_DIR/dashboard.xml"

if [ ! -d "$ADDONS_DIR" ]; then
    echo "fprime-gds addons dir not found: $ADDONS_DIR" >&2
    exit 1
fi

echo "Installing doom-display addon into $ADDONS_DIR"
rm -rf "$ADDONS_DIR/doom-display"
cp -r "$SRC_DIR" "$ADDONS_DIR/"

if ! grep -q "doom-display/addon.js" "$ENABLED_JS"; then
    echo 'import "./doom-display/addon.js";' >> "$ENABLED_JS"
    echo "Enabled doom-display in $ENABLED_JS"
else
    echo "doom-display already enabled in $ENABLED_JS"
fi

if [ -f "$CONFIG_JS" ]; then
    if grep -qE "config\.enableDashboards\s*=\s*true" "$CONFIG_JS"; then
        echo "Dashboards already enabled in $CONFIG_JS"
    elif grep -qE "config\.enableDashboards\s*=\s*false" "$CONFIG_JS"; then
        sed -i -E 's/config\.enableDashboards\s*=\s*false/config.enableDashboards = true/' "$CONFIG_JS"
        echo "Flipped enableDashboards to true in $CONFIG_JS"
    else
        # No existing assignment - append one inside setConfig().
        # We target the closing brace of setConfig and insert before it.
        if grep -q "export function setConfig" "$CONFIG_JS"; then
            python3 - "$CONFIG_JS" <<'PY'
import re, sys
path = sys.argv[1]
text = open(path).read()
pattern = re.compile(r'(export function setConfig\([^)]*\)\s*\{)([\s\S]*?)(\n\})', re.M)
m = pattern.search(text)
if not m:
    print(f"could not locate setConfig() in {path}", file=sys.stderr)
    sys.exit(1)
body = m.group(2)
if 'enableDashboards' in body:
    sys.exit(0)
injected = body.rstrip() + "\n    config.enableDashboards = true\n"
text = text[:m.start(2)] + injected + text[m.end(2):]
open(path, 'w').write(text)
PY
            echo "Injected config.enableDashboards = true into $CONFIG_JS"
        else
            echo "WARN: could not find setConfig() in $CONFIG_JS - skipping dashboards flag" >&2
        fi
    fi
else
    echo "WARN: $CONFIG_JS not present - skipping dashboards flag" >&2
fi

echo
echo "Done. Next steps:"
echo "  1. Restart fprime-gds so it picks up the new addon and config."
echo "  2. Open the GDS in your browser, click the 'Dashboard' tab,"
echo "     then 'Upload Dashboard File' and select:"
echo "         $DASHBOARD_XML"
echo "     (or copy it to your project root and upload from there.)"
