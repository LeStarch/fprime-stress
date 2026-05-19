"""fprime_get_doom.cli:

Implements ``fprime-get-doom``. Downloads the shareware DOOM1.WAD from
one of several public mirrors, verifies its SHA-256 against the
canonical id Software shareware v1.9 release, and writes it to a
caller-specified path (default: ``./doom1.wad`` in the current working
directory).

This package exists because:

* doomgeneric does not bundle any WAD (it is engine source only).
* id Software's shareware redistribution license permits redistribution
  only in original, complete form. Rather than commit the 4 MB binary
  blob plus all of id's required-companion files into the repo, we
  let developers and CI fetch the WAD on demand from upstream mirrors.

Usage::

    fprime-get-doom                    # writes build-artifacts/<plat>/<dep>/data/doom1.wad
    fprime-get-doom -o /tmp/doom1.wad  # writes to a specific path
    fprime-get-doom --force            # overwrite existing target

If the caller does not pass ``-o`` the output path defaults to
``build-artifacts/<platform>/<deployment>/data/doom1.wad``, auto-discovered
by globbing ``./build-artifacts/*/*/``. This matches the WAD path the
DOOM-enabled F Prime deployment looks for at boot when run from its
install ``bin/`` directory. If the build-artifacts directory is missing
or ambiguous (more than one platform/deployment present), the script
falls back to ``./doom1.wad`` and prints a hint.

The script is stdlib-only on purpose (urllib + hashlib + argparse) so
it works in minimal CI containers without needing pip.
"""

from __future__ import annotations

import argparse
import hashlib
import os
import platform as platform_module
import shutil
import sys
import tempfile
import urllib.error
import urllib.request
from pathlib import Path
from typing import Iterable, List, Optional

# ---------------------------------------------------------------------------
# Canonical hash for the id Software shareware DOOM1.WAD v1.9 (release of
# 1995-02-04). This is the exact file that ships in the doom-wad-shareware
# Debian package, the SlackBuilds shareware package, and every doomworld /
# ibiblio mirror that hosts the unmodified original.
#
# If any mirror serves a file whose SHA-256 does not match this constant the
# downloaded copy is discarded as untrusted. We never silently accept a
# differently-hashed file - that's the whole point of pinning a checksum.
# ---------------------------------------------------------------------------
EXPECTED_SHA256 = (
    "1d7d43be501e67d927e415e0b8f3e29c3bf33075e859721816f652a526cac771"
)
EXPECTED_SIZE_BYTES = 4_196_020

# Mirrors are tried in order. Each entry should serve the unmodified
# shareware DOOM1.WAD v1.9. Keep the canonical/most-stable mirror first
# so a healthy network requires only one HTTP fetch.
DEFAULT_MIRRORS: List[str] = [
    "https://distro.ibiblio.org/slitaz/sources/packages/d/doom1.wad",
    "https://archive.org/download/DoomsharewareEpisode/doom1.wad",
    "https://www.doomworld.com/3ddownloads/ports/shareware_doom_iwad.zip",
]

# Default output path matches the F Prime build-artifacts layout. When
# `fprime-util build` runs from the project root it produces
# `build-artifacts/<platform>/<deployment>/{bin,dict,lib}/`. We deposit
# the WAD into a sibling `data/` directory so the deployment binary can
# find it via the canonical `../data/doom1.wad` relative path when
# launched from inside its own `bin/` directory.
FALLBACK_OUTPUT = Path("doom1.wad")


def default_output_path(
    build_artifacts: Path = Path("build-artifacts"),
) -> Path:
    """Resolve the default output path.

    Returns ``build-artifacts/<platform>/<deployment>/data/doom1.wad``
    when a single ``<platform>/<deployment>`` pair can be unambiguously
    selected under ``build-artifacts/``. Falls back to ``./doom1.wad``
    otherwise.

    Selection rules:

    * If only one platform directory exists, use it.
    * Otherwise prefer the host OS's native platform (``Linux``,
      ``Darwin``, etc., per :func:`platform.system`) so cross-compile
      outputs sitting next to a native build do not change the default
      destination.
    * The deployment directory under the selected platform must also
      be unambiguous (exactly one).
    """
    try:
        if not build_artifacts.is_dir():
            return FALLBACK_OUTPUT
        platforms = [p for p in build_artifacts.iterdir() if p.is_dir()]
        if not platforms:
            return FALLBACK_OUTPUT
        if len(platforms) == 1:
            chosen_platform = platforms[0]
        else:
            host = platform_module.system()  # e.g. "Linux", "Darwin"
            host_matches = [p for p in platforms if p.name == host]
            if len(host_matches) == 1:
                chosen_platform = host_matches[0]
            else:
                return FALLBACK_OUTPUT
        deployments = [d for d in chosen_platform.iterdir() if d.is_dir()]
        if len(deployments) != 1:
            return FALLBACK_OUTPUT
        return chosen_platform / deployments[0].name / "data" / "doom1.wad"
    except OSError:
        return FALLBACK_OUTPUT


class FetchError(Exception):
    """Raised when no mirror could supply a valid DOOM1.WAD."""


def _format_bytes(n: int) -> str:
    for unit in ("B", "KiB", "MiB", "GiB"):
        if n < 1024.0:
            return f"{n:6.1f} {unit}"
        n /= 1024.0
    return f"{n:6.1f} TiB"


def _sha256_of(path: Path, chunk: int = 1 << 16) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(chunk), b""):
            h.update(block)
    return h.hexdigest()


def _download(url: str, dest: Path, *, timeout: float = 30.0) -> None:
    """Stream ``url`` to ``dest``. Raises urllib.error.URLError on failure."""
    request = urllib.request.Request(
        url,
        headers={"User-Agent": "fprime-get-doom/0.1"},
    )
    with urllib.request.urlopen(request, timeout=timeout) as response:
        with open(dest, "wb") as out:
            shutil.copyfileobj(response, out, length=1 << 16)


def fetch(
    output: Path,
    *,
    mirrors: Optional[Iterable[str]] = None,
    force: bool = False,
    quiet: bool = False,
) -> Path:
    """Fetch DOOM1.WAD to ``output``.

    Returns the resolved path on success. Raises :class:`FetchError`
    when every mirror has been tried and none produced a file with the
    expected SHA-256.
    """
    output = output.expanduser().resolve()

    if output.exists() and not force:
        actual = _sha256_of(output)
        if actual == EXPECTED_SHA256:
            if not quiet:
                print(
                    f"[fprime-get-doom] {output} already present and verified "
                    f"({_format_bytes(output.stat().st_size)})"
                )
            return output
        if not quiet:
            print(
                f"[fprime-get-doom] {output} exists but SHA-256 mismatch "
                f"(got {actual}, expected {EXPECTED_SHA256}); "
                f"refusing to overwrite (use --force)."
            )
        raise FetchError(
            f"existing file {output} has unexpected SHA-256 {actual}"
        )

    output.parent.mkdir(parents=True, exist_ok=True)

    candidates = list(mirrors) if mirrors is not None else list(DEFAULT_MIRRORS)
    if not candidates:
        raise FetchError("no mirrors configured")

    last_error: Optional[BaseException] = None
    for url in candidates:
        if not quiet:
            print(f"[fprime-get-doom] trying {url} ...")
        with tempfile.NamedTemporaryFile(
            dir=output.parent, prefix=".doom1.wad.", delete=False
        ) as tmp:
            tmp_path = Path(tmp.name)
        try:
            _download(url, tmp_path)
            size = tmp_path.stat().st_size
            if size != EXPECTED_SIZE_BYTES:
                if not quiet:
                    print(
                        f"[fprime-get-doom]   size mismatch from {url}: "
                        f"got {size} B, expected {EXPECTED_SIZE_BYTES} B"
                    )
                last_error = FetchError(f"size mismatch from {url}: {size} B")
                continue
            digest = _sha256_of(tmp_path)
            if digest != EXPECTED_SHA256:
                if not quiet:
                    print(
                        f"[fprime-get-doom]   SHA-256 mismatch from {url}: "
                        f"got {digest}"
                    )
                last_error = FetchError(f"SHA-256 mismatch from {url}")
                continue
            os.replace(tmp_path, output)
            if not quiet:
                print(
                    f"[fprime-get-doom] saved verified DOOM1.WAD to {output} "
                    f"({_format_bytes(size)})"
                )
            return output
        except (urllib.error.URLError, OSError, TimeoutError) as exc:
            last_error = exc
            if not quiet:
                print(f"[fprime-get-doom]   {url} failed: {exc}")
        finally:
            if tmp_path.exists():
                try:
                    tmp_path.unlink()
                except OSError:
                    pass

    raise FetchError(
        f"all {len(candidates)} mirror(s) failed; last error: {last_error!r}"
    )


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="fprime-get-doom",
        description=(
            "Download and verify the shareware DOOM1.WAD for use with the "
            "fprime-stress Doom component."
        ),
    )
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        default=None,
        help=(
            "Path to write the WAD to. If omitted, defaults to the"
            " build-artifacts data directory when a single platform/deployment"
            " is present, else ./doom1.wad."
        ),
    )
    parser.add_argument(
        "--mirror",
        action="append",
        dest="mirrors",
        default=None,
        help=(
            "Override the default mirror list. Pass once per mirror to try "
            "in order. If omitted, the built-in canonical mirrors are used."
        ),
    )
    parser.add_argument(
        "--force",
        action="store_true",
        help="Overwrite the output path even if it already exists.",
    )
    parser.add_argument(
        "--quiet",
        action="store_true",
        help="Suppress progress messages.",
    )
    parser.add_argument(
        "--print-sha256",
        action="store_true",
        help="Print the expected SHA-256 and exit.",
    )
    return parser


def fetch_entry(args: Optional[List[str]] = None) -> int:
    parser = _build_parser()
    parsed = parser.parse_args(args)

    if parsed.print_sha256:
        print(EXPECTED_SHA256)
        return 0

    output = parsed.output if parsed.output is not None else default_output_path()
    if parsed.output is None and not parsed.quiet:
        if output == FALLBACK_OUTPUT:
            print(
                "[fprime-get-doom] build-artifacts/<plat>/<dep>/ not"
                " auto-discovered; writing ./doom1.wad. Pass -o to override."
            )
        else:
            print(f"[fprime-get-doom] default output resolved to {output}")

    try:
        fetch(
            output,
            mirrors=parsed.mirrors,
            force=parsed.force,
            quiet=parsed.quiet,
        )
    except FetchError as exc:
        print(f"fprime-get-doom: ERROR: {exc}", file=sys.stderr)
        return 1
    return 0
