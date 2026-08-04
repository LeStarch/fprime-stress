# doomgeneric (third-party)

This deployment vendors the upstream **doomgeneric** project from
[ozkl/doomgeneric](https://github.com/ozkl/doomgeneric) at commit
`dcb7a8d` under `Doom/doomgeneric/`. doomgeneric in turn is a port of the
[Chocolate Doom](https://github.com/chocolate-doom/chocolate-doom)
engine, which is itself derived from the original id Software DOOM
source release.

## License

doomgeneric is licensed under the **GNU General Public License,
version 2** (GPLv2). The verbatim license text is preserved at:

```
Doom/doomgeneric/COPYING
```

Per the requirements of GPLv2 we redistribute that license file
unaltered alongside the source.

## Source preservation

No file under `Doom/doomgeneric/` has been modified by this project.
The complete vendored source tree is identical to the upstream
release with the following platform-specific files **removed** (they
are not built and never replaced by an in-place modification):

```
doomgeneric_allegro.c
doomgeneric_emscripten.c
doomgeneric_linuxvt.c
doomgeneric_sdl.c
doomgeneric_soso.c
doomgeneric_sosox.c
doomgeneric_win.c
doomgeneric_xlib.c
i_allegromusic.c
i_allegrosound.c
i_sdlmusic.c
i_sdlsound.c
```

The required `DG_*` platform glue functions are implemented in
`Doom/DoomEngine.cpp` outside the vendored tree.

## Combined work licensing

When the final deployment binary is linked, it incorporates
GPLv2-covered object code from doomgeneric. The resulting binary is
therefore distributable only under the terms of GPLv2 (or any later
version, per upstream's "or, at your option, any later version"
clause).

The F Prime framework itself is unmodified and is consumed as an
external library at its own (Apache-2.0) license; that license is
unchanged.

## Where the rules of the road live

- doomgeneric source / GPLv2 license: `Doom/doomgeneric/`
- F Prime license (Apache-2.0): see the linked F Prime checkout
- This project's own (new) source: see top-level `LICENSE`
