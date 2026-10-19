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
 *  -debug: Print debug information
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

using namespace std;

bool debug;

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

// Function to find the SSYB packet with the given packet number
vector<uint8_t> get_ssyb_pack(const vector<uint8_t> &data, uint8_t pack_num) {
    size_t seq_count = (data.size() >= 144000) ? 12 : 10; // PAL (10 sequences) or NTSC (12 sequences)

    for (size_t i = 0; i < seq_count; ++i) {
        for (size_t j = 0; j < 2; ++j) { // Each sequence has two DIF blocks with subcode data
            for (size_t k = 0; k < 6; ++k) { // Each block contains 6 packets
                size_t offset = i * 150 * 80 + j * 80 + 3 + k * 8 + 3;
                if (data[offset] == pack_num) {
                    return vector<uint8_t>(data.begin() + offset, data.begin() + offset + 8);
                }
            }
        }
    }
    return {}; // Return an empty vector if the packet is not found
}

// Function to extract date and time from the DV stream
vector<int> get_dv_recording_time(const vector<uint8_t> &data, const string &name, size_t offset) {
    if (data.size() != 144000 && data.size() != 120000) {
        return {}; // Return an empty vector if the size is not NTSC or PAL frame size
    }

    auto pack62 = get_ssyb_pack(data, 0x62); // Date packet
    auto pack63 = get_ssyb_pack(data, 0x63); // Time packet

    if (pack62.empty() || pack63.empty()) {
        return {}; // Could not find required packets
    }

    int day = (pack62[2] & 0xf) + 10 * ((pack62[2] >> 4) & 0x3);
    int month = (pack62[3] & 0xf) + 10 * ((pack62[3] >> 4) & 0x1);
    int year = (pack62[4] & 0xf) + 10 * ((pack62[4] >> 4) & 0xf);
    year += (year < 50) ? 2000 : 1900;

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
                    file.seekg(stream_offset);
                    vector<uint8_t> data = read_chunk(file, stream_offset, size);

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

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cerr << "dv2str <video_file_path> <-debug>" << endl;
        return 1;
    }

    debug = (argc == 3);

    string file_path = argv[1];
    auto timecodes = parse_avi_file(file_path);

    map<vector<int>, size_t> occurrences;
    for (const auto &timecode : timecodes) {
        ++occurrences[timecode];
    }

    bool has_repeated_timecode = false;
    for (const auto &[timecode, count] : occurrences) {
        if (count >= 3) {
            has_repeated_timecode = true;
            cout << "Timecode: ";
            for (int part : timecode) {
                cout << part << " ";
            }
            cout << endl;
        }
    }

    if (!has_repeated_timecode) {
        cerr << "Warning: no valid repeated date/time packets (0x62/0x63) were found; file cannot be processed." << endl;
        return 1;
    }

    return 0;
}