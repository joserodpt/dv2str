import os
import struct
import sys

'''

WARNING: The input must contain valid repeated date/time SSYB packets
(0x62 and 0x63). Files without these packets cannot be processed.

AVI Header information from: https://xoax.net/sub_web/ref_dev/fileformat_avi/

'''


def get_ssyb_pack(data, pack_num):
    """Find and return the packet with the given packet number."""
    seq_count = 12 if len(data) >= 144000 else 10  # PAL (10 sequences) or NTSC (12 sequences)

    for i in range(seq_count):
        for j in range(2):  # Each sequence has two DIF blocks with subcode data
            for k in range(6):  # Each block contains 6 packets
                offset = i * 150 * 80 + j * 80 + 3 + k * 8 + 3
                packet = data[offset:offset + 8]

                if packet[0] == pack_num:
                    return packet
    return None


def get_dv_recording_time(data, name, offset):
    """Extract the date and time from a DV stream."""
    if len(data) not in [144000, 120000]:  # Check for NTSC or PAL frame size
        return None

    # print(f"\nProcessing video chunk {name} at offset {offset}, size {len(data)} bytes")

    pack62 = get_ssyb_pack(data, 0x62)  # Get the date packet
    pack63 = get_ssyb_pack(data, 0x63)  # Get the time packet

    if not pack62 or not pack63:
        return None  # Could not find required packets

    # Extract and decode day, month, and year
    day = (pack62[2] & 0xf) + 10 * ((pack62[2] >> 4) & 0x3)
    month = (pack62[3] & 0xf) + 10 * ((pack62[3] >> 4) & 0x1)
    year = (pack62[4] & 0xf) + 10 * ((pack62[4] >> 4) & 0xf)
    if year < 50:
        year += 2000
    else:
        year += 1900

    # Extract and decode hour, minute, and second
    sec = (pack63[2] & 0xf) + 10 * ((pack63[2] >> 4) & 0x7)
    min = (pack63[3] & 0xf) + 10 * ((pack63[3] >> 4) & 0x7)
    hour = (pack63[4] & 0xf) + 10 * ((pack63[4] >> 4) & 0x3)

    if debug:
        print(f"Timecode: {day:02}/{month:02}/{year} {hour:02}:{min:02}:{sec:02}")

    # **Validation Checks** (all checks are done before the return)
    if not (1 <= day <= 31):
        return None
    if not (1 <= month <= 12):
        return None
    if not (1995 <= year <= 2100):
        return None
    if not (0 <= sec <= 59):
        return None
    if not (0 <= min <= 59):
        return None
    if not (0 <= hour <= 23):
        return None

    # If all checks pass, return the extracted date and time
    return (day, month, year, hour, min, sec)


def read_chunk(file, offset, size):
    file.seek(offset)
    raw_data = file.read(size)
    hex_bytes = ' '.join(f'{byte:02X}' for byte in raw_data)
    return raw_data, hex_bytes


def read_int(file, offset, size=4):
    raw_data, hex_bytes = read_chunk(file, offset, size)
    value = int.from_bytes(raw_data, 'little')
    return value, raw_data, hex_bytes


def read_string(file, offset, size=4):
    raw_data, hex_bytes = read_chunk(file, offset, size)
    value = raw_data.decode('ascii', errors='ignore')
    return value, raw_data, hex_bytes


def print_header_help(name):
    if not debug:
        return
    print(f"\n--- {name} ---")
    print(f"{'Offset':<6} {'Name':<20} {'Size':<6} {'Value':<12} {'Hex Bytes'}")


def print_header_info(offset, name, size, value, hex_bytes):
    if not debug:
        return
    print(f"{offset:<6} {name:<20} {size:<6} {value:<12} {hex_bytes}")


def parse_idx1(file, offset):
    """Parse the 'idx1' chunk to extract index entries."""
    # print("\n--- PARSING idx1 (Index List) ---")

    # Move to the correct offset for idx1 chunk
    file.seek(offset)
    chunk_header = file.read(8)
    chunk_id, chunk_size = struct.unpack('<4sI', chunk_header)

    if chunk_id != b'idx1':
        print(f"Error: Expected 'idx1' but found {chunk_id.decode('ascii', errors='ignore')}")
        return []

    print(f"Found 'idx1' chunk of size {chunk_size} bytes")

    # Initialize a list to store index entries
    idx_entries = []

    # Process each entry (16 bytes each)
    for _ in range(chunk_size // 16):
        entry = file.read(16)
        stream_id, flags, offset, size = struct.unpack('<4sIII', entry)
        stream_name = stream_id.decode('ascii', errors='ignore')

        # Print entry details
        # print(f"Stream ID: {stream_name}, Offset: {offset}, Size: {size}")

        # Store the index entry in the list
        idx_entries.append({
            'stream_id': stream_name,
            'offset': offset,
            'size': size
        })

    return idx_entries


def parse_riff_header(file):
    print_header_help('RIFF Header')
    offset = 0
    riff_id, raw_data, hex_bytes = read_string(file, offset, 4)
    print_header_info(offset, 'RIFF ID', 4, riff_id, hex_bytes)

    if riff_id != 'RIFF':
        print('This is not a valid AVI file.')
        raise SystemExit

    offset += 4
    riff_size, raw_data, hex_bytes = read_int(file, offset, 4)
    print_header_info(offset, 'Size', 4, riff_size, hex_bytes)
    offset += 4
    riff_format, raw_data, hex_bytes = read_string(file, offset, 4)
    print_header_info(offset, 'Format', 4, riff_format, hex_bytes)

    return offset


def parse_avi_header(file, offset):
    print_header_help('AVI Header')

    offset += 4
    value, raw_data, hex_bytes = read_string(file, offset, 4)
    print_header_info(offset, 'AVI Header ID', 4, value, hex_bytes)
    offset += 4
    value, raw_data, hex_bytes = read_int(file, offset, 4)
    print_header_info(offset, 'Length', 4, value, hex_bytes)
    return offset


def parse_list_header(file, offset):
    print_header_help('Header List')
    offset += 4
    value, raw_data, hex_bytes = read_string(file, offset, 4)
    print_header_info(offset, 'List ID', 4, value, hex_bytes)
    offset += 4
    value, raw_data, hex_bytes = read_int(file, offset, 4)
    print_header_info(offset, 'Length', 4, value, hex_bytes)
    offset += 4
    value, raw_data, hex_bytes = read_string(file, offset, 4)
    print_header_info(offset, 'Header ID', 4, value, hex_bytes)
    return offset


def parse_avi_main_header(file, offset):
    """Parse the AVI Main Header (MainAVIHeader) from an AVI file."""
    print_header_help('AVI Main Header (MainAVIHeader)')

    offset += 4  # Skip the "avih" header
    value, raw_data, hex_bytes = read_int(file, offset, 4)
    print_header_info(offset, 'μsecs Per Frame', 4, value, hex_bytes)
    offset += 4
    value, raw_data, hex_bytes = read_int(file, offset, 4)
    print_header_info(offset, 'Max Byte Rate', 4, value, hex_bytes)
    offset += 4
    value, raw_data, hex_bytes = read_int(file, offset, 4)
    print_header_info(offset, 'Reserved 1', 4, value, hex_bytes)
    offset += 4
    value, raw_data, hex_bytes = read_int(file, offset, 4)
    print_header_info(offset, 'Flags', 4, value, hex_bytes)
    offset += 4
    value, raw_data, hex_bytes = read_int(file, offset, 4)
    print_header_info(offset, 'Total Frames', 4, value, hex_bytes)
    offset += 4
    value, raw_data, hex_bytes = read_int(file, offset, 4)
    print_header_info(offset, 'Initial Frame', 4, value, hex_bytes)
    offset += 4
    value, raw_data, hex_bytes = read_int(file, offset, 4)
    print_header_info(offset, 'Streams', 4, value, hex_bytes)
    offset += 4
    value, raw_data, hex_bytes = read_int(file, offset, 4)
    print_header_info(offset, 'Buffer Size', 4, value, hex_bytes)
    offset += 4
    value, raw_data, hex_bytes = read_int(file, offset, 4)
    print_header_info(offset, 'Width', 4, value, hex_bytes)
    offset += 4
    value, raw_data, hex_bytes = read_int(file, offset, 4)
    print_header_info(offset, 'Height', 4, value, hex_bytes)
    offset += 4
    # value, raw_data, hex_bytes = read_chunk(file, offset, 16)
    print_header_info(offset, 'Reserved', 16, '0', 0)
    offset += 16

    return offset


def parse_video_stream_list(file, offset):
    """Parse the Video Stream List (strl) from an AVI file."""
    print_header_help('Video Stream List')

    value, raw_data, hex_bytes = read_string(file, offset, 4)
    print_header_info(offset, 'List ID', 4, value, hex_bytes)
    offset += 4
    value, raw_data, hex_bytes = read_int(file, offset, 4)
    print_header_info(offset, 'Size of List', 4, value, hex_bytes)
    offset += 4
    value, raw_data, hex_bytes = read_string(file, offset, 4)
    print_header_info(offset, 'Stream List', 4, value, hex_bytes)
    offset += 4
    value, raw_data, hex_bytes = read_string(file, offset, 4)
    print_header_info(offset, 'Stream Header', 4, value, hex_bytes)
    offset += 4
    value, raw_data, hex_bytes = read_int(file, offset, 4)
    print_header_info(offset, 'Size of Header', 4, value, hex_bytes)
    offset += 4
    return offset


def parse_avi_video_stream_header(file, offset):
    """Parse the AVI Video Stream Header (AVIStreamHeader) of an AVI file."""
    print_header_help('AVI Video Stream Header')

    value, raw_data, hex_bytes = read_string(file, offset, 4)
    print_header_info(offset, 'Type', 4, value, hex_bytes)
    offset += 4

    value, raw_data, hex_bytes = read_string(file, offset, 4)
    print_header_info(offset, 'Handler', 4, value, hex_bytes)
    offset += 4

    value, raw_data, hex_bytes = read_int(file, offset, 4)
    print_header_info(offset, 'Flags', 4, value, hex_bytes)
    offset += 4

    value, raw_data, hex_bytes = read_int(file, offset, 4)
    print_header_info(offset, 'Priority', 4, value, hex_bytes)
    offset += 4

    value, raw_data, hex_bytes = read_int(file, offset, 4)
    print_header_info(offset, 'Initial Frames', 4, value, hex_bytes)
    offset += 4

    value, raw_data, hex_bytes = read_int(file, offset, 4)
    print_header_info(offset, 'Scale', 4, value, hex_bytes)
    offset += 4

    value, raw_data, hex_bytes = read_int(file, offset, 4)
    print_header_info(offset, 'Rate', 4, value, hex_bytes)
    offset += 4

    value, raw_data, hex_bytes = read_int(file, offset, 4)
    print_header_info(offset, 'Start', 4, value, hex_bytes)
    offset += 4

    value, raw_data, hex_bytes = read_int(file, offset, 4)
    print_header_info(offset, 'Length', 4, value, hex_bytes)
    offset += 4

    value, raw_data, hex_bytes = read_int(file, offset, 4)
    print_header_info(offset, 'Buffer Size', 4, value, hex_bytes)
    offset += 4

    value, raw_data, hex_bytes = read_int(file, offset, 4)
    print_header_info(offset, 'Quality', 4, value, hex_bytes)
    offset += 4

    value, raw_data, hex_bytes = read_int(file, offset, 4)
    print_header_info(offset, 'Sample Size', 4, value, hex_bytes)
    offset += 4
    print_header_info(offset, 'Frame SKIPPED', 0, "", hex_bytes)

    # skip frame
    offset += 8

    value, raw_data, hex_bytes = read_string(file, offset, 4)
    print_header_info(offset, 'Stream Format', 4, value, hex_bytes)
    offset += 4

    value, raw_data, hex_bytes = read_int(file, offset, 4)
    print_header_info(offset, 'Length Format', 4, value, hex_bytes)
    offset += 4

    return offset


def parse_bitmapinfoheader(file, offset):
    """Parse the BITMAPINFOHEADER of an AVI video stream."""
    print_header_help('BITMAPINFOHEADER')
    # Read and display the size of the header (4 bytes)
    size, _, hex_bytes = read_int(file, offset, 4)
    print_header_info(offset, 'Size', 4, size, hex_bytes)
    offset += 4

    # Read width (4 bytes)
    width, _, hex_bytes = read_int(file, offset, 4)
    print_header_info(offset, 'Width', 4, width, hex_bytes)
    offset += 4

    # Read height (4 bytes)
    height, _, hex_bytes = read_int(file, offset, 4)
    print_header_info(offset, 'Height', 4, height, hex_bytes)
    offset += 4

    # Read planes (2 bytes)
    planes, _, hex_bytes = read_int(file, offset, 2)
    print_header_info(offset, 'Planes', 2, planes, hex_bytes)
    offset += 2

    # Read bit count (2 bytes)
    bit_count, _, hex_bytes = read_int(file, offset, 2)
    print_header_info(offset, 'Bit Count', 2, bit_count, hex_bytes)
    offset += 2

    # Read compression type (4 bytes)
    compression, _, hex_bytes = read_string(file, offset, 4)
    print_header_info(offset, 'Compression', 4, compression, hex_bytes)
    offset += 4

    # Read image size (4 bytes)
    image_size, _, hex_bytes = read_int(file, offset, 4)
    print_header_info(offset, 'Image Size', 4, image_size, hex_bytes)
    offset += 4

    # Read X Pels Per Meter (4 bytes)
    x_pels_per_meter, _, hex_bytes = read_int(file, offset, 4)
    print_header_info(offset, 'X Pels Per Meter', 4, x_pels_per_meter,
                      hex_bytes)
    offset += 4

    # Read Y Pels Per Meter (4 bytes)
    y_pels_per_meter, _, hex_bytes = read_int(file, offset, 4)
    print_header_info(offset, 'Y Pels Per Meter', 4, y_pels_per_meter,
                      hex_bytes)
    offset += 4

    # Read Colors Used (4 bytes)
    colors_used, _, hex_bytes = read_int(file, offset, 4)
    print_header_info(offset, 'Colors Used', 4, colors_used, hex_bytes)
    offset += 4

    # Read Colors Important (4 bytes)
    colors_important, _, hex_bytes = read_int(file, offset, 4)
    print_header_info(offset, 'Colors Important', 4, colors_important,
                      hex_bytes)
    offset += 4

    return offset


def parse_audio_stream_header(file, offset):
    """Parse the AVI Audio Stream Header (AVIStreamHeader) for an AVI file."""
    print_header_help('AVI Audio Stream Header')

    # Read the stream header type (should be "auds" for audio streams)
    stream_type, _, hex_bytes = read_string(file, offset, 4)
    print_header_info(offset, 'Type', 4, stream_type, hex_bytes)
    offset += 4

    # Read the handler (FOURCC for the codec)
    handler, _, hex_bytes = read_string(file, offset, 4)
    print_header_info(offset, 'Handler', 4, handler, hex_bytes)
    offset += 4

    # Read flags for the audio stream
    flags, _, hex_bytes = read_int(file, offset, 4)
    print_header_info(offset, 'Flags', 4, flags, hex_bytes)
    offset += 4

    # Read priority (typically 0 for audio)
    priority, _, hex_bytes = read_int(file, offset, 4)
    print_header_info(offset, 'Priority', 4, priority, hex_bytes)
    offset += 4

    return offset


def parse_audio_stream_list(file, offset):
    """Parse the Audio Stream List from an AVI file."""
    print_header_help('Audio Stream List')

    # Read the List ID and check if it's the correct one
    value, _, hex_bytes = read_string(file, offset, 4)
    print_header_info(offset, 'List ID', 4, value, hex_bytes)
    offset += 4

    if value != 'LIST':
        return offset

    # Read the Length of the list
    length, _, hex_bytes = read_int(file, offset, 4)
    print_header_info(offset, 'Length', 4, length, hex_bytes)
    offset += 4

    # Check for the Stream List identifier ('strl')
    value, _, hex_bytes = read_string(file, offset, 4)
    print_header_info(offset, 'Stream List', 4, value, hex_bytes)
    offset += 4

    value, _, hex_bytes = read_string(file, offset, 4)
    print_header_info(offset, 'Stream Header', 4, value, hex_bytes)
    offset += 4

    value, _, hex_bytes = read_int(file, offset, 4)
    print_header_info(offset, 'Stream Length', 4, value, hex_bytes)

    offset += 4
    offset = parse_audio_stream_header(file, offset)

    return offset


def parse_stream_type_and_handler(file, offset):
    """Parse the stream header to check if it's a video or audio stream based on stream_id and handler."""
    stream_info = {}

    # Read stream type (4 bytes)
    stream_type, _, _ = read_string(file, offset, 4)

    if stream_type == 'vids':  # Video Stream
        stream_info['type'] = 'video'
    elif stream_type == 'auds':  # Audio Stream
        stream_info['type'] = 'audio'
    else:
        stream_info['type'] = 'unknown'

    # Read the handler (FOURCC codec)
    handler, _, _ = read_string(file, offset + 4, 4)
    stream_info['handler'] = handler

    print(f"\nStream Type: {stream_info['type']}")
    print(f"Codec Handler: {stream_info['handler']}")
    print(f"Offset: {offset}")

    return stream_info


def parse_avi_file(file_path):
    timecodeDates = []

    with open(file_path, 'rb') as file:
        offset = parse_riff_header(file)
        offset = 8

        # Iterate through the chunks to find the idx1 chunk and get stream locations
        while True:
            chunk_header = file.read(8)
            if len(chunk_header) < 8:
                break  # End of file

            chunk_id, chunk_size = struct.unpack('<4sI', chunk_header)
            if chunk_id == b'idx1':  # Found idx1 chunk
                idx1_entries = parse_idx1(file, file.tell() - 8)
                for entry in idx1_entries:
                    stream_id = entry['stream_id']
                    offset = entry['offset']
                    size = entry['size']

                    if size in [144000, 120000]:  # Check for NTSC or PAL frame size
                        file.seek(offset)

                        results = get_dv_recording_time(file.read(size), stream_id, offset)
                        if results:
                            timecodeDates.append(results)

                return timecodeDates
            else:
                # Skip the current chunk
                file.seek(chunk_size, 1)

    '''
    offset = parse_list_header(file, offset)
    offset = parse_avi_header(file, offset)
    offset = parse_avi_main_header(file, offset)
    offset = 88
    offset = parse_video_stream_list(file, offset)
    offset = parse_avi_video_stream_header(file, offset)
    offset = parse_bitmapinfoheader(file, offset)
    offset = parse_audio_stream_list(file, offset)
    '''

    return timecodeDates


def formatSeconds(seconds):
    """
    Format a floating point number (seconds) into the SRT time format: HH:MM:SS,SSS.

    Parameters:
    - seconds (float): The total number of seconds, including the fractional part (milliseconds).

    Returns:
    - str: The formatted time in 'HH:MM:SS,SSS' format.
    """
    # Separate the even and decimal parts of the time
    even = int(seconds)
    decimal = seconds - even

    # Convert the decimal part to milliseconds
    milliseconds = int(decimal * 1000)

    # Calculate hours, minutes, and seconds
    hours = even // 3600
    minutes = (even % 3600) // 60
    seconds = even % 60

    # Return the formatted string in HH:MM:SS,SSS format
    return f"{hours:02d}:{minutes:02d}:{seconds:02d},{milliseconds:03d}"


def write_dates_to_srt(dates_list, filePath):
    """Write the list of dates as tuples (day, month, year, hour, min, sec) to an SRT file with the given offset."""

    filePath = filePath.replace(".avi", ".srt")

    print('Writing timecodes to SRT file...', filePath)

    with open(filePath, 'w') as f:
        previous_time = 0

        for i, date_tuple in enumerate(dates_list):
            # Extract the date and time values from the tuple
            day, month, year, hour, minute, second = date_tuple

            # Write the subtitle index, time range, and text
            f.write(f"{i + 1}\n")
            f.write(f"{formatSeconds(previous_time)} --> {formatSeconds(previous_time + 1)}\n")
            f.write(f"{day:02}/{month:02}/{year}\n")
            f.write(f"{hour:02}:{minute:02}:{second:02}\n")
            f.write("\n")

            # Update previous time
            previous_time += 1  # Increment by 1 second

def process_avi_file(file_path):
    """Processes a single AVI file, extracting timecodes and saving to an SRT file."""
    print(f"Processing file: {file_path}")
    if not file_path.endswith(".avi"):
        print("Invalid file format. Please provide an AVI file.")
        return

    timecodeDates = parse_avi_file(file_path)
    if not timecodeDates:
        print(f"Could not find timecodes in file: {file_path} (ffmpeg cut?)")
        return

    # Count occurrences of each date
    count = {}
    for date in timecodeDates:
        count[date] = count.get(date, 0) + 1

    # Sort by frequency (descending) and filter out items with fewer than 3 occurrences
    count = {key: value for key, value in sorted(count.items(), key=lambda item: item[1], reverse=True) if value >= 3}

    # Sort the filtered list of timecodes by date and time (year, month, day, hour, minute, second)
    sorted_dates = sorted(count.keys(), key=lambda x: (x[2], x[1], x[0], x[3], x[4], x[5]))

    if not sorted_dates:
        print('Warning: no valid repeated date/time packets (0x62/0x63) were found; file cannot be processed.')
        return

    for x in sorted_dates:
        print(f"{x[2]:02}/{x[1]:02}/{x[0]} {x[3]:02}:{x[4]:02}:{x[5]:02}")

    write_dates_to_srt(sorted_dates, file_path)

def process_avi_directory(directory_path):
    """Processes all AVI files within a directory."""
    print(f"Converting all AVI files in directory: {directory_path}")
    for root, _, files in os.walk(directory_path):
        for file in files:
            if file.endswith(".avi"):
                full_file_path = os.path.join(root, file)
                process_avi_file(full_file_path)

    print("All AVI files processed.")

fileName = "timecode"

debug = False

if __name__ == "__main__":
    file_path = sys.argv[1]
    debug = "-d" in sys.argv

    #check if file_Path is an AVI file or directory

    if os.path.isdir(file_path):
        process_avi_directory(file_path)
    elif os.path.isfile(file_path):
        process_avi_file(file_path)
    else:
        print("Invalid file path. Please provide a valid AVI file or directory.")
        sys.exit(1)


