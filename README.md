# DV2str - A DV Timecode Extractor Tool

## Table of Content
- [Project Overview](#project-overview)
- [Features](#features)
- [Download](#download)
  - [Supported platforms](#supported-platforms)
  - [Windows 7 / 8 / 8.1 note](#windows-7--8--81-note)
- [Usage](#usage)
- [How DV2str Works](#how-dv2str-works)
- [(.avi) file format - Header References](#avi-file-format---header-references)
- [DV Format in .avi](#dv-format-in-avi)
- [Main Functions](#main-functions)
- [Debug](#debugging-with-the-debug-flag)
- [Build from Source](#build-from-source)
- [License](#license)
- [Authors & Contributors](#authors--contributors)


## Project Overview

The **DV Timecode Extractor Tool** is a **C++** program to extract the **recording date & time** data (following the **DD** / **MM** / **YYYY** **HH** : **mm** : **ss** Format) from **DV video streams**, encapsulated in **.avi** files, and generate **.srt** files, that could then be embedded in **.mp4** containers.

There's also a project that contains the same ideia applied for **.mp4** camcorder files, called [mp4str](https://github.com/joserodpt/mp42str).

## Features

- Precise extraction of Date timecodes, directly from DV (**.avi**) into **.srt** files.
- Offers support for both **NTSC** and **PAL** streaming systems.
- Handles both classic AVI and **OpenDML / AVI 2.0** files.
- Batch mode: convert a whole **folder** of `.avi` files in one run (`-dir`).


---


## Download

Prebuilt binaries are published on the [**Releases**](https://github.com/joserodpt/dv2str/releases/latest) page. Grab the one for your system:

| Platform | Download |
|---|---|
| **Windows** (64-bit) | [dv2str-windows-x86_64.exe](https://github.com/joserodpt/dv2str/releases/latest/download/dv2str-windows-x86_64.exe) |
| **macOS** (Universal: Apple Silicon + Intel) | [dv2str-macos-universal](https://github.com/joserodpt/dv2str/releases/latest/download/dv2str-macos-universal) |
| **Linux** (x86-64) | [dv2str-linux-x86_64](https://github.com/joserodpt/dv2str/releases/latest/download/dv2str-linux-x86_64) |
| **Linux** (ARM64) | [dv2str-linux-arm64](https://github.com/joserodpt/dv2str/releases/latest/download/dv2str-linux-arm64) |

On macOS and Linux, make the file executable after downloading:

```bash
chmod +x dv2str-*
```

> On macOS, the first launch may be blocked by Gatekeeper because the binary is unsigned. Allow it under **System Settings → Privacy & Security**, or run `xattr -d com.apple.quarantine dv2str-macos-universal`.

### Supported platforms

| OS | Supported versions | Notes |
|---|---|---|
| **Windows** | Windows 7 SP1 and later (64-bit) | On Windows 7/8/8.1 the Universal C Runtime must be present — see the note below. Built-in on Windows 10/11. |
| **macOS** | macOS 11 (Big Sur) and later | Single Universal binary runs natively on both Apple Silicon and Intel. |
| **Linux** | Any modern 64-bit distribution (x86-64 or ARM64) | Statically linked against musl — no glibc or other runtime dependencies. |

### Windows 7 / 8 / 8.1 note

The Windows build depends on the **Universal C Runtime (UCRT)**. This ships built-in with Windows 10 and 11, so no extra steps are needed there.

On **Windows 7 SP1, 8, and 8.1** the UCRT is only present if the system has been kept up to date through Windows Update. If the program fails to start with an error about a missing `api-ms-win-crt-*.dll`, install Microsoft's free update:

- **Update for Universal C Runtime in Windows (KB2999226):** <https://support.microsoft.com/en-us/help/2999226>
- Direct downloads (Microsoft Download Center): <https://www.microsoft.com/en-us/download/details.aspx?id=48234> — pick the `.msu` for your system, e.g. `Windows6.1-KB2999226-x64.msu` for 64-bit Windows 7 SP1.

After installing the update and rebooting, the program will run.


---


## Usage

```bash
dv2str <path_to_the_avi_file> [options]
dv2str -dir <folder> [options]
```

Options:

| Option | Description |
|---|---|
| `-dir <folder>` | Process every `.avi` file in the given folder (non-recursive), writing an `.srt` next to each. Use instead of a single file path. |
| `-debug` | Print detailed diagnostic information (including the running timecode pack `0x13`). |
| `-pivot <0-99>` | Century pivot for the 2-digit recording year (default `50`). Years below the pivot map to `20xx`, the rest to `19xx`. |
| `-format <spec>` | Date/time layout written to the SRT (see below). Default: day/month/year on one line, time on the next. |
| `-locale <name>` | Language for month and weekday names (`%A`, `%B`, …). E.g. `it_IT.UTF-8` → `lunedì`, `ottobre`. Pass `""` to use the system default. |

The extracted date/time is printed to the terminal, and an **`.srt`** file is written next to the source `.avi` (one 1-second subtitle per second of footage).

Example:

```bash
dv2str sample/DVsample.23-09-06_17-22-00.avi
```

To convert a whole folder of tapes in one go, point `-dir` at it. Files that can't be processed are reported and skipped, and the rest of the batch still runs:

```bash
dv2str -dir ~/Videos/dv-captures
```

### Date/time formats (`-format`)

`-format` accepts either a **preset name** or any **`strftime` pattern**.

Presets:

| Preset | Example output |
|---|---|
| `default` | `23/09/2006` + `17:22:54` (two lines) |
| `inline` | `23/09/2006 17:22:54` |
| `long` | `23 September 2006` + `17:22:54` (two lines) |
| `long-inline` | `23 September 2006 17:22:54` |
| `iso` | `2006-09-23 17:22:54` |
| `date` | `23/09/2006` |
| `time` | `17:22:54` |

For full control, pass a [`strftime`](https://en.cppreference.com/w/cpp/chrono/c/strftime) pattern directly (use `\n` / `\t` for a newline / tab):

```bash
dv2str movie.avi -format "%d %B %Y"                   # 23 September 2006
dv2str movie.avi -format "%A, %d %B %Y - %H:%M:%S"    # Saturday, 23 September 2006 - 17:22:54
dv2str movie.avi -format "%Y-%m-%d\n%H:%M:%S"         # 2006-09-23 (newline) 17:22:54
```

Common fields: `%d` day, `%m` month number, `%B` full month name, `%b` short month, `%Y` 4-digit year, `%H:%M:%S` time, `%A` weekday.

### Localized names (`-locale`)

By default month and weekday names are in English. Use `-locale` to render them in another language (the locale must be installed on your system):

```bash
dv2str movie.avi -locale it_IT.UTF-8 -format "%A %d %B %Y"   # sabato 23 settembre 2006
dv2str movie.avi -locale it_IT.UTF-8 -format "%A %d %b %Y"   # sabato 23 set 2006
dv2str movie.avi -locale "" -format "%A %d %B %Y"            # use the system's default locale
```

Locale names differ by platform:

- **macOS / Linux**: `it_IT.UTF-8`, `pt_PT.UTF-8`, `fr_FR.UTF-8`, … On Linux the locale may need to be generated first (e.g. `sudo locale-gen it_IT.UTF-8`). List what's installed with `locale -a`.
- **Windows**: use names like `Italian_Italy`, `Portuguese_Portugal`, or BCP-47 tags such as `it-IT`.


---


## How DV2str Works

1. The **.avi** file is opened
2. The header is analysed, cycling through the most relevant **chunks**.
3. It searches for subcode packages (such as **pack62** and **pack63**), which hold all the **Time and Date** information from the file.
4. It **decodes and checks for any errors** in the extracted data, **returning the timecode** formated on a more readable state.
5. The Information is then ready to be **exported as a .srt file**, or used in any other way intended.


## (.avi) file format - Header References

NOTE: All the following references are based on **.avi** File Format Research Databases.
Go on (https://xoax.net/sub_web/ref_dev/fileformat_avi/) to find more information regarding the Audio Video Interleave (avi) File Format.
- **RIFF Header**: Defines the file as an **.avi** type
- **Chunks "hdrl"**: Contains all metadata from the file (such as the it's resolution, etc...)
- **Chunks "movi"**: Contains all Multiplexed Audio and Video data.
- **Chunks "idx1"**: Although optional, it can be used to index the frames, making navigation on the file much more efficient.


## DV Format in .avi
The DV format encapsulates digital video (DV) in data packets with a specific structure. Each DV frame contains timecode subcodes that can be extracted for use in editing or analysis.

The DV Data Extraction Process:
- Identify video data chunks in the **.avi** file (typically located in the "movi" chunk).
- Read 80-byte DV packets, which include:
  - Timecodes: Information such as hour, minute, second, and frame number.
  - Auxiliary subcodes: Used for synchronization and error correction.

NOTE: The source code in this project uses logic similar to that found in WinDV(https://github.com/hfiggs/WinDV/blob/main/DV.cpp) to identify and interpret encapsulated DV packets.


### Main Functions
- **get_dv_recording_time(data, name, offset)**: Extracts and verifies the Date and Time from the data stream.
- **get_ssyb_pack(data, pack_num)**: Finds specific subcode packages on the stream.
- **parse_riff_header(file)**: Analyses the RIFF Header from the file.
- **parse_idx1(file, offset)**: Locates the chunks index on the file.


### Debugging with the *-debug* Flag
Passing the `-debug` flag provides detailed information when executing the program, to assist in troubleshooting.
When enabled, the program outputs:
- Raw DV packet data for inspection.
- Frame timecodes and subcodes extracted from the AVI file.
- Any anomalies or errors encountered during processing.

This is particularly useful for validating DV data integrity and analyzing specific frames in the video stream.


---


## Build from Source

### Requirements

- A C++17-capable compiler (e.g. `g++` or `clang++`) for a native build.
- `make` (a `Makefile` is provided).
- [Zig](https://ziglang.org/download/) (`brew install zig`) — only needed for the Linux/Windows cross builds.

### Native build

```bash
make
# or, without make:
c++ -std=c++17 -Wall -Wextra -O2 main.cpp -o dv2str
```

### Cross-compiling release binaries

The `Makefile` can produce binaries for every supported platform into `dist/`:

```bash
make dist          # build macOS, Linux (x86-64 + ARM64) and Windows binaries
```

Individual targets are also available: `make macos`, `make linux`, `make linux-arm`, `make windows`.

- **macOS** is built with the native toolchain as a Universal binary (`-arch arm64 -arch x86_64`), so it must be built on macOS.
- **Linux** and **Windows** are built with `zig c++` and are statically linked, so they run without any runtime dependencies (aside from the Windows 7/8/8.1 UCRT note above).


---


# License
MIT License

Copyright (c) 2024 DV2str

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

---


# Authors & Contributors
- José Rodrigues - Author
- Tomás Gonçalves - Contributor
