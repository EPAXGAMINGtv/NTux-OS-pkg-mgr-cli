# NTux Package Manager (ntuxpkg)

A **standalone** graphical package manager for [NTux-OS](https://github.com/epaxgaming/NTux-OS).

Fully self-contained — includes its own copy of the NTux libc, headers,
linker script, and Makefile.  You can push this folder to its own GitHub
repo and build it without the rest of the NTux source tree.

## Requirements

- Linux x86_64 host
- `cc`, `ld`, `as` (GCC or Clang, GNU ld / binutils)

## Build

```sh
cd ntuxpkg
make
```

Output: `out/ntuxpkg.elf`

## What it does

Connects to a package server (default `http://10.0.2.2:8080`) and lets you:

- **Browse** available packages
- **Install** – download `.elf` → save to `/usr/bin/<name>` → register in `/etc/pkg/packages.json`
- **Remove** – delete file + unregister from local DB
- **Update** – compare versions, download newer if available
- **Refresh** – re-fetch package list from server

## Protocol

The server must expose three endpoints:

| Endpoint              | Returns                               |
|-----------------------|---------------------------------------|
| `GET /packages`       | JSON array of package name strings    |
| `GET /package/<name>` | JSON object (`name`, `version`, etc.) |
| `GET /download/<name>`| Raw ELF binary                        |

A reference Python server is available at `tools/pkgserver/pkgserver.py`.

## Project structure

```
ntuxpkg/
├── GNUmakefile         # Standalone build
├── linker.ld           # NTux ELF linker script
├── README.md
├── libc/               # NTux libc (C source)
├── include/            # libc + window API headers
└── src/
    ├── ntuxpkg.h       # Shared types / function declarations
    ├── ntuxpkg_main.c  # Entry point, event loop, HTTP & DB logic
    └── ntuxpkg_ui.c    # Window rendering (list, details, buttons)
```
