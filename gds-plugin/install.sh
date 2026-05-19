#!/usr/bin/env bash
# Installs the doom-display fprime-gds addon source files into the
# active fprime-gds Python package.
#
# Why this script exists:
#
# 1. fprime-gds discovers addons by importing modules listed in
#    `<fprime_gds>/flask/static/addons/enabled.js`. That file lives
#    inside the installed Python package, so registering an out-of-tree
#    addon requires copying the source in and appending an import line.
#    There is no project-local override hook for this yet upstream.
#
# 2. The dashboards feature flag (`config.enableDashboards`) used to
#    be patched here too, but is now flipped per-project via the
#    `flask.JS_CONFIGURATION_FILE` override in `fprime-gds.yml` which
#    points the GDS at `gds-plugin/config.js`. That means a clone +
#    `pip install` + this script is the full setup - no
#    site-packages mutation required to enable the Dashboard tab.
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

echo
echo "Done. Next steps:"
echo "  1. From the project root, run: fprime-gds"
echo "     (auto-loads fprime-gds.yml, which enables dashboards via"
echo "      a project-local config.js override - no site-packages"
echo "      mutation needed for the dashboard flag.)"
echo "  2. Open the GDS in your browser, click the 'Dashboard' tab,"
echo "     then 'Upload Dashboard File' and select:"
echo "         $DASHBOARD_XML"
