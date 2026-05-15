"""fprime_get_doom.__main__:

Console-script entry point. Mirrors the
`fprime-tools/src/fprime/util/__main__.py` convention so the binary
installed by ``pip install fprime-get-doom`` behaves like the rest of
the F Prime CLI family.
"""

import sys

from fprime_get_doom import cli


def main():
    """Run wrapper, to point a console_script at."""
    return cli.fetch_entry(args=sys.argv[1:])


if __name__ == "__main__":
    sys.exit(main())
