# fprime-get-doom

A tiny console script that downloads, verifies, and pins the shareware
DOOM1.WAD from a public upstream mirror. It exists because the
[fprime-stress](../../) Doom component needs *some* WAD to load at
startup, and id Software's shareware redistribution license is
clearer when the WAD is fetched on demand from id's own published
mirrors than when a binary copy is committed into a third-party repo.

## Install

From a checkout of `fprime-stress`:

```bash
pip install ./tools/fprime-get-doom
```

This installs the `fprime-get-doom` console script onto your `$PATH`,
mirroring the entry-point conventions of `fprime-util` and `fprime-gds`.

## Use

```bash
# writes ./doom1.wad (default)
fprime-get-doom

# writes to a specific path
fprime-get-doom -o /var/lib/fprime/doom1.wad

# override the mirror list (try a private mirror first)
fprime-get-doom --mirror https://my.intranet.example.com/wads/doom1.wad

# print the expected SHA-256 (for use in another script)
fprime-get-doom --print-sha256
```

The download is verified against the canonical SHA-256 of id Software
shareware DOOM1.WAD v1.9
(`1d7d43be501e67d927e415e0b8f3e29c3bf33075e859721816f652a526cac771`,
4,196,020 bytes). If a mirror serves a file that does not match,
the temporary download is discarded and the next mirror is tried.

The script has no third-party Python dependencies (stdlib only: `urllib`,
`hashlib`, `argparse`) so it ships into minimal CI containers cleanly.

## Why not just commit the WAD?

The id Software shareware redistribution EULA permits redistribution
of DOOM1.WAD only "in original, complete form" alongside id's
LICENSE / README / VENDOR.DOC files. Rather than carry that whole
distribution kit in a third-party repository with its own
Apache-2.0 / GPLv2 licensing matrix, we leave the WAD on its
upstream mirrors and provide this fetch-on-demand helper. See
`THIRDPARTY/doomgeneric.md` in the `fprime-stress-reference` deployment
for the engine licensing details (GPLv2 via doomgeneric).
