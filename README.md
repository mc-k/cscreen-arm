# cscreen-arm

A native Apple Silicon (arm64) re-implementation of **cscreen**, the classic
command-line tool for changing display settings on macOS.

The original cscreen (freeware, ca. 2004–2010s) only exists as an
x86_64/i386 binary. It runs on Apple Silicon only through Rosetta 2, which
macOS is phasing out. cscreen-arm is written from scratch in C against the
public CoreGraphics display API and aims to be a **drop-in replacement**:
same options, same output format.

## Features

```
cscreen [-d <depth>] [-x <width>] [-y <height>] [-r <refresh>] [-s <display>] [-v] [-m] [-f] [-l] [-h]
```

| Option | Meaning |
|---|---|
| `-l` | list current displays and modes (default action) |
| `-v` | list valid modes |
| `-s <n>` / `-s a` | select display by index / all displays |
| `-i <id>` | select display by CGDirectDisplayID |
| `-x`, `-y`, `-r`, `-d` | set width, height, refresh rate, depth |
| `-m` | require an exact mode match |
| `-p` | make the selected display the main display (menu bar) |
| `-h` | show usage |

Typical use – swap the main display between two screens (e.g. bound to a
hotkey in BetterTouchTool):

```sh
cscreen -s 2 -p
```

## Build & install

Requires the Xcode Command Line Tools (`xcode-select --install`).

```sh
make
make install          # installs to ~/.local/bin (override with PREFIX=...)
```

## Compatibility notes

- Output of `-l`, `-v` and `-h` matches the original byte for byte
  (tested on macOS 27 with two displays).
- `-p` swaps the origins of the old and the new main display, which
  reproduces the original behaviour of moving all windows to the other
  screen.
- Reconstructed, not verified against the original: mode selection without
  `-m` (closest match), `-f` (accepted, no effect), error messages.
- Depth is always reported as 32.

## Disclaimer

This is an independent re-implementation and is not affiliated with the
author of the original cscreen.

## License

MIT – see [LICENSE](LICENSE).
