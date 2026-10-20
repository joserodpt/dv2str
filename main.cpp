/*
 *
 *    _______      _____      _
 *   |  __ \ \    / /__ \    | |
 *   | |  | \ \  / /   ) |___| |_ _ __
 *   | |  | |\ \/ /   / // __| __| '__|
 *   | |__| | \  /   / /_\__ \ |_| |
 *   |_____/   \/   |____|___/\__|_|
 *
 *  C++ Utility program to extract timecode to .str files from DV file's
 *  that are compliant with the DV specification (IEC 61834-2) and that have:
 *  - SSYB packets (0x62 and 0x63) with the date and time information
 *
 *  WARNING: The input must contain valid repeated date/time SSYB packets
 *  (0x62 and 0x63). Files without these packets cannot be processed.
 *
 *  Syntax: dv2str <video_file_path> [options]
 *  Options:
 *  -debug         : Print debug information (incl. the running timecode pack 0x13)
 *  -pivot <0-99>  : Century pivot for the 2-digit recording year (default 50).
 *                   Years below the pivot map to 20xx, the rest to 19xx.
 *                   The DV spec's own suggested pivot is 25; 50 is used here so
 *                   that typical 2000s-era camcorder footage decodes correctly.
 *  -format <spec> : Date/time layout written to the SRT. Accepts a preset name
 *                   (default, inline, long, long-inline, iso, date, time) or any
 *                   strftime pattern, e.g. "%d %B %Y" -> "23 September 2006".
 *                   Use \n / \t for a newline / tab. Default: "%d/%m/%Y\n%H:%M:%S"
 *                   (day/month/year on one line, time on the next).
 *
 *  This program is licensed under the MIT License.
 *  (c) José Rodrigues, Tomás Gonçalves 2024
 */

#include <iostream>

#include <fstream>
#include <vector>
#include <string>
#include <iomanip>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <memory>
#include <algorithm>
#include <sstream>
#include <ctime>

using namespace std;

bool debug;
int century_pivot = 50; // 2-digit years below this map to 20xx, the rest to 19xx

// strftime pattern for the date/time written to each SRT entry. The default
// keeps the original layout: day/month/year on one line, time on the next.
string date_format = "%d/%m/%Y\n%H:%M:%S";

// Function to read data from the file at a specific offset
vector<uint8_t> read_chunk(ifstream &file, streampos offset, size_t size) {
    file.seekg(offset);
    vector<uint8_t> buffer(size);
    file.read(reinterpret_cast<char*>(buffer.data()), size);
    buffer.resize(static_cast<size_t>(file.gcount()));
    return buffer;
}

// Function to extract a 4-byte integer from byte data
uint32_t read_int(const vector<uint8_t> &data, size_t offset) {
    return (data[offset] | (data[offset + 1] << 8) | (data[offset + 2] << 16) | (data[offset + 3] << 24));
}

// Function to read a 4-byte string (e.g., 'RIFF', 'AVI ') from byte data
string read_string(const vector<uint8_t> &data, size_t offset) {
    return string(reinterpret_cast<const char*>(&data[offset]), 4);
}

// Function to find the first 5-byte pack with the given ID in a DV frame.
// Scans each DIF sequence's subcode region (timecode 0x13) and then its VAUX
// region (rec date 0x62 / rec time 0x63), mirroring the reference finder.
vector<uint8_t> get_ssyb_pack(const vector<uint8_t> &data, uint8_t pack_num) {
    size_t seq_count = (data.size() >= 144000) ? 12 : 10; // PAL (12 sequences) or NTSC (10 sequences)

    for (size_t i = 0; i < seq_count; ++i) {
        size_t base = i * 150 * 80; // 12000-byte DIF sequence

        // SUBCODE: DIF blocks 1-2, 6 packs each, stride 8
        for (size_t block = 1; block <= 2; ++block) {
            for (size_t k = 0; k < 6; ++k) {
                size_t offset = base + block * 0x50 + 3 + k * 8 + 3;
                if (offset + 5 > data.size()) continue;
                if (data[offset] == pack_num) {
                    return vector<uint8_t>(data.begin() + offset, data.begin() + offset + 5);
                }
            }
        }

        // VAUX: DIF blocks 1-3, 15 packs each, stride 5
        for (size_t block = 1; block <= 3; ++block) {
            for (size_t k = 1; k <= 15; ++k) {
                size_t offset = base + block * 0x50 + 0x9e + k * 5;
                if (offset + 5 > data.size()) continue;
                if (data[offset] == pack_num) {
                    return vector<uint8_t>(data.begin() + offset, data.begin() + offset + 5);
                }
            }
        }
    }
    return {}; // Return an empty vector if the packet is not found
}

// Decode the running timecode pack (0x13) into {hours, minutes, seconds, frames}.
vector<int> get_dv_timecode(const vector<uint8_t> &data) {
    auto pack = get_ssyb_pack(data, 0x13);
    if (pack.empty()) {
        return {};
    }
    int frames  = (pack[1] & 0xf) + 10 * ((pack[1] >> 4) & 0x3); // tens: 2 bits
    int seconds = (pack[2] & 0xf) + 10 * ((pack[2] >> 4) & 0x7); // tens: 3 bits
    int minutes = (pack[3] & 0xf) + 10 * ((pack[3] >> 4) & 0x7); // tens: 3 bits
    int hours   = (pack[4] & 0xf) + 10 * ((pack[4] >> 4) & 0x3); // tens: 2 bits
    return {hours, minutes, seconds, frames};
}

// Function to extract date and time from the DV stream
vector<int> get_dv_recording_time(const vector<uint8_t> &data, const string &name, size_t offset) {
    if (data.size() != 144000 && data.size() != 120000) {
        return {}; // Return an empty vector if the size is not NTSC or PAL frame size
    }

    // Cached copies of the last successfully decoded packs. When a frame is
    // missing its date or time pack, fall back to the previously stored copy
    // rather than discarding the frame (per the reference's cached-pack fallback).
    static vector<uint8_t> cached_pack62;
    static vector<uint8_t> cached_pack63;

    auto pack62 = get_ssyb_pack(data, 0x62); // Date packet
    auto pack63 = get_ssyb_pack(data, 0x63); // Time packet

    if (pack62.empty()) pack62 = cached_pack62; else cached_pack62 = pack62;
    if (pack63.empty()) pack63 = cached_pack63; else cached_pack63 = pack63;

    if (pack62.empty() || pack63.empty()) {
        return {}; // Could not find required packets and no cached copy available
    }

    int day = (pack62[2] & 0xf) + 10 * ((pack62[2] >> 4) & 0x3);
    int month = (pack62[3] & 0xf) + 10 * ((pack62[3] >> 4) & 0x1);
    int year = (pack62[4] & 0xf) + 10 * ((pack62[4] >> 4) & 0xf);
    year += (year < century_pivot) ? 2000 : 1900;

    int sec = (pack63[2] & 0xf) + 10 * ((pack63[2] >> 4) & 0x7);
    int min = (pack63[3] & 0xf) + 10 * ((pack63[3] >> 4) & 0x7);
    int hour = (pack63[4] & 0xf) + 10 * ((pack63[4] >> 4) & 0x3);

    // Validation checks
    if (day < 1 || day > 31 || month < 1 || month > 12 || year < 1995 || year > 2100 ||
        sec < 0 || sec > 59 || min < 0 || min > 59 || hour < 0 || hour > 23) {
        return {}; // Return an empty vector if any validation fails
    }

    return {day, month, year, hour, min, sec}; // Return the extracted date and time
}

// Define a base class for polymorphism
struct Value {
    virtual ~Value() = default;
    virtual void print() const = 0;
};

// Define derived classes to hold specific types
struct StringValue : public Value {
    string value;
    explicit StringValue(const string& str) : value(str) {}
    void print() const override {
        cout << value;
    }
    string getValue() const {
        return value;
    }
};

struct IntValue : public Value {
    uint32_t value;
    explicit IntValue(uint32_t val) : value(val) {}
    void print() const override {
        cout << value;
    }
    uint32_t getValue() const {
        return value;
    }
};

// A map with string keys and Value* to hold either StringValue or IntValue
using ValueMap = map<string, shared_ptr<Value>>;


// Function to parse the 'RIFF' header
size_t parse_riff_header(ifstream &file) {
    vector<uint8_t> data = read_chunk(file, 0, 12);
    if (data.size() < 12) {
        cerr << "Invalid AVI header." << endl;
        exit(EXIT_FAILURE);
    }

    string riff_id = read_string(data, 0);

    if (riff_id != "RIFF") {
        cerr << "This is not a valid AVI file." << endl;
        exit(EXIT_FAILURE);
    }

    size_t offset = 4;
    uint32_t riff_size = read_int(data, offset);
    offset += 4;
    string riff_format = read_string(data, offset);
    if (riff_format != "AVI ") {
        cerr << "This is not a valid AVI file." << endl;
        exit(EXIT_FAILURE);
    }

    return offset + 4;
}

// Function to parse the 'idx1' chunk
vector<ValueMap> parse_idx1(ifstream &file, size_t offset) {
    file.seekg(offset);
    vector<uint8_t> chunk_header = read_chunk(file, offset, 8);
    string chunk_id = read_string(chunk_header, 0);
    uint32_t chunk_size = read_int(chunk_header, 4);

    if (chunk_id != "idx1") {
        cerr << "Error: Expected 'idx1' but found " << chunk_id << endl;
        return {};
    }

    // Vector of maps to store the entries
    vector<ValueMap> idx_entries;
    size_t num_entries = chunk_size / 16;

    for (size_t i = 0; i < num_entries; ++i) {
        vector<uint8_t> entry = read_chunk(file, offset + 8 + i * 16, 16);

        string stream_id = read_string(entry, 0);
        uint32_t stream_offset = read_int(entry, 8);
        uint32_t size = read_int(entry, 12);

        // Create a map for each entry
        ValueMap entry_map;
        entry_map["stream_id"] = make_shared<StringValue>(stream_id);
        entry_map["offset"] = make_shared<IntValue>(stream_offset);
        entry_map["size"] = make_shared<IntValue>(size);

        idx_entries.push_back(entry_map);
    }

    return idx_entries;
}

// Main function to parse the AVI file
vector<vector<int>> parse_avi_file(const string &file_path) {
    vector<vector<int>> timecodeDates;
    ifstream file(file_path, ios::binary);

    if (!file.is_open()) {
        cerr << "Error opening file: " << file_path << endl;
        exit(EXIT_FAILURE);
    }

    size_t offset = parse_riff_header(file);

    while (true) {
        vector<uint8_t> chunk_header = read_chunk(file, offset, 8);
        if (chunk_header.size() < 8) break; // End of file

        string chunk_id = read_string(chunk_header, 0);
        uint32_t chunk_size = read_int(chunk_header, 4);

        if (chunk_id == "idx1") {
            auto idx1_entries = parse_idx1(file, offset);
            if (debug) {
                cout << "Found idx1 at " << offset << " with " << idx1_entries.size() << " entries" << endl;
            }
            for (const auto &entry : idx1_entries) {
                string stream_id = dynamic_pointer_cast<StringValue>(entry.at("stream_id"))->getValue();
                size_t stream_offset = dynamic_pointer_cast<IntValue>(entry.at("offset"))->getValue();
                uint32_t size = dynamic_pointer_cast<IntValue>(entry.at("size"))->getValue();

                if (size == 144000 || size == 120000) { // Check for NTSC or PAL frame size
                    if (debug) {
                        cout << "Frame " << stream_id << " at " << stream_offset << " (" << size << " bytes)" << endl;
                    }

                    // In many AVI files (notably OpenDML/AVI 2.0) the idx1 entry
                    // offset points at the chunk HEADER (the 4-byte fourcc + the
                    // 4-byte size), not at the DV payload. Detect that case by
                    // checking for the fourcc at the offset and skip the 8-byte
                    // header so the DV frame is read with the correct alignment.
                    // Without this the whole frame is shifted by 8 bytes and the
                    // 0x62/0x63 date/time packs are never found.
                    size_t data_offset = stream_offset;
                    vector<uint8_t> hdr = read_chunk(file, stream_offset, 4);
                    if (hdr.size() == 4 && read_string(hdr, 0) == stream_id) {
                        data_offset = stream_offset + 8;
                    }

                    file.seekg(data_offset);
                    vector<uint8_t> data = read_chunk(file, data_offset, size);

                    if (debug) {
                        auto tc = get_dv_timecode(data); // {hours, minutes, seconds, frames}
                        if (!tc.empty()) {
                            cout << "  timecode (0x13): "
                                 << setfill('0') << setw(2) << tc[0] << ":"
                                 << setw(2) << tc[1] << ":" << setw(2) << tc[2]
                                 << ":" << setw(2) << tc[3] << setfill(' ') << endl;
                        }
                    }

                    auto results = get_dv_recording_time(data, stream_id, stream_offset);
                    if (!results.empty()) {
                        timecodeDates.push_back(results);
                    }
                }
            }
            break;
        }

        offset += chunk_size + 8;
    }

    return timecodeDates;
}

// Format a whole number of seconds into the SRT time format HH:MM:SS,mmm.
string format_seconds(int total_seconds) {
    int hours = total_seconds / 3600;
    int minutes = (total_seconds % 3600) / 60;
    int seconds = total_seconds % 60;
    ostringstream out;
    out << setfill('0') << setw(2) << hours << ":"
        << setw(2) << minutes << ":" << setw(2) << seconds << ",000";
    return out.str();
}

// Resolve a -format value to a strftime pattern. A handful of friendly preset
// names map to common layouts; anything else is treated as a literal strftime
// pattern. The escape sequences \n and \t are turned into real characters so
// multi-line layouts can be given on the command line.
string resolve_date_format(const string &spec) {
    static const map<string, string> presets = {
        {"default",     "%d/%m/%Y\n%H:%M:%S"}, // day/month/year, then time
        {"inline",      "%d/%m/%Y %H:%M:%S"},
        {"long",        "%d %B %Y\n%H:%M:%S"}, // e.g. 23 September 2006
        {"long-inline", "%d %B %Y %H:%M:%S"},
        {"iso",         "%Y-%m-%d %H:%M:%S"},
        {"date",        "%d/%m/%Y"},
        {"time",        "%H:%M:%S"},
    };

    auto it = presets.find(spec);
    const string &pattern = (it != presets.end()) ? it->second : spec;

    string out;
    for (size_t i = 0; i < pattern.size(); ++i) {
        if (pattern[i] == '\\' && i + 1 < pattern.size()) {
            char c = pattern[i + 1];
            if (c == 'n') { out += '\n'; ++i; continue; }
            if (c == 't') { out += '\t'; ++i; continue; }
        }
        out += pattern[i];
    }
    return out;
}

// Format one {day, month, year, hour, min, sec} entry using date_format.
string format_datetime(const vector<int> &d) {
    std::tm tm{};
    tm.tm_mday = d[0];
    tm.tm_mon  = d[1] - 1;    // tm_mon is 0-based
    tm.tm_year = d[2] - 1900; // tm_year counts from 1900
    tm.tm_hour = d[3];
    tm.tm_min  = d[4];
    tm.tm_sec  = d[5];
    tm.tm_isdst = -1;

    // Fill tm_wday / tm_yday (needed for %A, %a, %j) from a normalized copy,
    // without letting mktime's timezone/DST handling shift the displayed time.
    std::tm tmp = tm;
    if (std::mktime(&tmp) != static_cast<std::time_t>(-1)) {
        tm.tm_wday = tmp.tm_wday;
        tm.tm_yday = tmp.tm_yday;
    }

    char buf[256];
    size_t n = std::strftime(buf, sizeof(buf), date_format.c_str(), &tm);
    return string(buf, n);
}

// Write the extracted dates (each {day, month, year, hour, min, sec}) to an SRT
// file alongside the source, one 1-second subtitle per entry.
void write_dates_to_srt(const vector<vector<int>> &dates, const string &file_path) {
    string srt_path = file_path;
    const string avi_ext = ".avi";
    if (srt_path.size() >= avi_ext.size() &&
        srt_path.compare(srt_path.size() - avi_ext.size(), avi_ext.size(), avi_ext) == 0) {
        srt_path.replace(srt_path.size() - avi_ext.size(), avi_ext.size(), ".srt");
    } else {
        srt_path += ".srt";
    }

    ofstream f(srt_path);
    if (!f.is_open()) {
        cerr << "Error: could not write SRT file: " << srt_path << endl;
        return;
    }

    cout << "Writing timecodes to SRT file... " << srt_path << endl;

    int previous_time = 0;
    for (size_t i = 0; i < dates.size(); ++i) {
        const auto &d = dates[i]; // {day, month, year, hour, min, sec}
        f << (i + 1) << "\n";
        f << format_seconds(previous_time) << " --> " << format_seconds(previous_time + 1) << "\n";
        f << format_datetime(d) << "\n";
        f << "\n";
        previous_time += 1;
    }
}

int main(int argc, char* argv[]) {
    string file_path;
    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        if (arg == "-debug") {
            debug = true;
        } else if (arg == "-pivot") {
            if (i + 1 >= argc) {
                cerr << "Error: -pivot requires a value (0-99)." << endl;
                return 1;
            }
            century_pivot = atoi(argv[++i]);
        } else if (arg.rfind("-pivot=", 0) == 0) {
            century_pivot = atoi(arg.c_str() + 7);
        } else if (arg == "-format") {
            if (i + 1 >= argc) {
                cerr << "Error: -format requires a value (a preset name or strftime pattern)." << endl;
                return 1;
            }
            date_format = resolve_date_format(argv[++i]);
        } else if (arg.rfind("-format=", 0) == 0) {
            date_format = resolve_date_format(arg.substr(8));
        } else if (!arg.empty() && arg[0] != '-') {
            file_path = arg;
        } else {
            cerr << "Error: unknown option '" << arg << "'." << endl;
            return 1;
        }
    }

    if (file_path.empty()) {
        cerr << "dv2str <video_file_path> [-debug] [-pivot <0-99>] [-format <spec>]" << endl;
        return 1;
    }

    if (century_pivot < 0 || century_pivot > 99) {
        cerr << "Error: -pivot must be between 0 and 99." << endl;
        return 1;
    }

    auto timecodes = parse_avi_file(file_path);

    // Count how often each distinct date/time appears, keeping only those that
    // recur (>= 3 frames) to filter out transient decode noise.
    map<vector<int>, size_t> occurrences;
    for (const auto &timecode : timecodes) {
        ++occurrences[timecode];
    }

    vector<vector<int>> repeated; // each: {day, month, year, hour, min, sec}
    for (const auto &[timecode, count] : occurrences) {
        if (count >= 3) {
            repeated.push_back(timecode);
        }
    }

    if (repeated.empty()) {
        cerr << "Warning: no valid repeated date/time packets (0x62/0x63) were found; file cannot be processed." << endl;
        return 1;
    }

    // Sort chronologically: year, month, day, hour, minute, second.
    sort(repeated.begin(), repeated.end(), [](const vector<int> &a, const vector<int> &b) {
        return tie(a[2], a[1], a[0], a[3], a[4], a[5]) < tie(b[2], b[1], b[0], b[3], b[4], b[5]);
    });

    for (const auto &d : repeated) {
        cout << setfill('0') << setw(2) << d[0] << "/" << setw(2) << d[1] << "/" << d[2]
             << " " << setw(2) << d[3] << ":" << setw(2) << d[4] << ":" << setw(2) << d[5]
             << setfill(' ') << endl;
    }

    write_dates_to_srt(repeated, file_path);

    return 0;
}