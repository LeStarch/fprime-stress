/**
 * config.js: project-local fprime-gds front-end configuration for
 * fprime-stress-reference. Served at `/js/config.js` by the GDS Flask
 * app instead of the package default, when fprime-gds.yml sets
 * `flask.JS_CONFIGURATION_FILE` to point at this file.
 *
 * See fprime-gds/src/fprime_gds/flask/static/js/config_init.js for the
 * full set of fields and their defaults.
 */
export function setConfig(config) {
    // Dashboards host the doom-display addon. Off by default in
    // fprime-gds because uploaded dashboard XML can carry arbitrary
    // Vue templates; we opt in here because the only dashboard
    // shipped by this project is the one in
    // lib/fprime-stress/gds-plugin/dashboard.xml, reviewed in-tree.
    config.enableDashboards = true;
}
