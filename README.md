# DV2str - A DV Timecode Extractor Tool

## Table of Content
- [Project Overview](#why-do-i-need-the-dv-timecode-extractor-tool)
- [Features](#features)
- [How DV2str Works](#how-dv2str-works)
- [(.avi) file format - Header References](#avi-file-format---header-references)
- [DV Format in .avi](#dv-format-in-avi)
- [Main Functions](#main-functions)
- [Debug](#debugging-with-d-flag)
- [How to Install](#how-to-install)
- [License](#license)
- [Authors & Contributors](#authors--contributors)


## Project Overview

The **DV Timecode Extractor Tool** is a **C++** program to extract the **recording date & time** data (following the **DD** / **MM** / **YYYY** **HH** : **mm** : **ss** Format) from **DV video streams**, encapsulated in **.avi** files, and generate **.srt** files, that could then be embedded in **.mp4** containers.

There's also a project that contains the same ideia applied for **.mp4** camcorder files, called [mp4str](https://github.com/joserodpt/mp42str).

## Features

- Precise extraction of Date timecodes, directly from DV (**.avi**) into **.srt** files.
- Offers support for both **NTSC** and **PAL** streaming systems.


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
Passing a second argument (e.g. `-debug`) provides detailed information when executing the program, to assist in troubleshooting. 
When enabled, the program outputs:
- Raw DV packet data for inspection.
- Frame timecodes and subcodes extracted from the AVI file.
- Any anomalies or errors encountered during processing.

This is particularly useful for validating DV data integrity and analyzing specific frames in the video stream.


---


## How to Build & Run

1. **First, make sure you have:**
    - A C++17-capable compiler (e.g. `g++` or `clang++`).
    - `make` (optional, a `Makefile` is provided).

2. **Clone this repository using git (NOTE: although not recommended, you can opt to download this repository directly from the GitHub source).**

3. **Build the program**:
    ```bash
    make
    # or, without make:
    c++ -std=c++17 -Wall -Wextra main.cpp -o dv2str
    ```

4. **Run it**:
    ```bash
    ./dv2str <path_to_the_avi_file> [-debug]
    ```

5. **Output**:
   - The Date and time will then be shown on your device's Terminal.


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

