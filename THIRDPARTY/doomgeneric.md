# doomgeneric (third-party)

This library consumes the upstream **doomgeneric** project from
[ozkl/doomgeneric](https://github.com/ozkl/doomgeneric) as the git
submodule `Doom/DoomEngine/doomgeneric/`, pinned at commit
`dcb7a8dbc7a16ce3dda29382ac9aae9d77d21284`. doomgeneric in turn is a port of the
[Chocolate Doom](https://github.com/chocolate-doom/chocolate-doom)
engine, which is itself derived from the original id Software DOOM
source release.

## License

doomgeneric is licensed under the **GNU General Public License,
version 2** (GPLv2). The verbatim license text is preserved at:

```
Doom/DoomEngine/doomgeneric/LICENSE
```

Per the requirements of GPLv2 the submodule carries that license file
unaltered alongside the source.

## Source preservation

No file in the submodule is modified by this project; the gitlink pins
the exact upstream commit. Only the portable engine sources listed in
`Doom/DoomEngine/CMakeLists.txt` are compiled. The upstream
platform-specific back ends (`doomgeneric_*.c`, `i_sdl*.c`,
`i_allegro*.c`) and Makefiles are present in the checkout but never
built; the required `DG_*` platform glue functions are implemented in
`Doom/DoomEngine/DoomEngine.cpp` outside the submodule.

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

- doomgeneric source / GPLv2 license: `Doom/DoomEngine/doomgeneric/`
- F Prime license (Apache-2.0): see the linked F Prime checkout
- This project's own (new) source: see top-level `LICENSE`
