/*
 * Copyright 2021 The DAPHNE Consortium
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <fstream>
#include <iostream>
#include <limits>
#include <runtime/local/datastructures/FixedSizeStringValueType.h>
#include <runtime/local/io/FileMetaData.h>
#include <runtime/local/io/utils.h>
#include <sstream>
#include <string>
#include <vector>

// Helper function to trim leading and trailing whitespace.
static std::string trim(const std::string &s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start])))
        start++;
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1])))
        end--;
    return s.substr(start, end - start);
}

int generality(ValueTypeCode type) { // similar to generality in TypeInferenceUtils.cpp but for ValueTypeCode
    switch (type) {
    case ValueTypeCode::SI8:
        return 0;
    case ValueTypeCode::UI8:
        return 1;
    case ValueTypeCode::SI32:
        return 2;
    case ValueTypeCode::UI32:
        return 3;
    case ValueTypeCode::SI64:
        return 4;
    case ValueTypeCode::UI64:
        return 5;
    case ValueTypeCode::F32:
        return 6;
    case ValueTypeCode::F64:
        return 7;
    case ValueTypeCode::FIXEDSTR16:
        return 8;
    default:
        return 9;
    }
}

// Helper function to check if a line is empty or contains only whitespace.
bool isEmptyLine(const std::string &line) {
    return std::all_of(line.begin(), line.end(), [](unsigned char c) { return std::isspace(c); });
}

ValueTypeCode inferValueType(const char *line, size_t &pos, char delim) {
    std::string field;
    // Extract field until delimiter
    while (line[pos] != delim && line[pos] != '\0') {
        field.push_back(line[pos]);
        pos++;
    }
    // Skip delimiter if present.
    if (line[pos] == delim)
        pos++;
    return inferValueType(field);
}

// Function to infer the data type of string value
ValueTypeCode inferValueType(const std::string &valueStr) {

    if (valueStr.empty())
        return ValueTypeCode::STR;

    std::string token;
    token = trim(valueStr);
    if (valueStr.front() == '\"') {
        if (valueStr.back() != '\"')
            return ValueTypeCode::STR;
        // Remove the surrounding quotes.
        token = valueStr.substr(1, valueStr.size() - 2);
        if (token.size() == 16)
            return ValueTypeCode::FIXEDSTR16;
    }

    // Check if the string represents an integer.
    bool isInteger = true;
    for (char c : token) {
        if (!isdigit(c) && c != '-' && c != '+' && !isspace(c)) {
            isInteger = false;
            break;
        }
    }
    if (isInteger) {
        try {
            size_t pos;
            int64_t value = std::stoll(token, &pos);
            // ensure there were no extra characters that were silently ignored
            if (pos == token.size()) {
                if (value >= std::numeric_limits<int8_t>::min() && value <= std::numeric_limits<int8_t>::max())
                    return ValueTypeCode::SI8;
                else if (value >= 0 && value <= std::numeric_limits<uint8_t>::max())
                    return ValueTypeCode::UI8;
                else if (value >= std::numeric_limits<int32_t>::min() && value <= std::numeric_limits<int32_t>::max())
                    return ValueTypeCode::SI32;
                else if (value >= 0 && value <= std::numeric_limits<uint32_t>::max())
                    return ValueTypeCode::UI32;
                else if (value >= std::numeric_limits<int64_t>::min() && value <= std::numeric_limits<int64_t>::max())
                    return ValueTypeCode::SI64;
                else
                    return ValueTypeCode::UI64;
            }
        } catch (const std::invalid_argument &) {
            // Fall through to string.
        } catch (const std::out_of_range &) {
            return ValueTypeCode::UI64;
        }
    }

    // Check if the string represents a float.
    try {
        size_t pos;
        float fvalue = std::stof(token, &pos);
        if (pos == token.size() && fvalue >= std::numeric_limits<float>::lowest() &&
            fvalue <= std::numeric_limits<float>::max())
            return ValueTypeCode::F32;
    } catch (const std::invalid_argument &) {
    } catch (const std::out_of_range &) {
    }

    // Check if the string represents a double.
    try {
        size_t pos;
        double dvalue = std::stod(token, &pos);
        if (pos == token.size() && dvalue >= std::numeric_limits<double>::lowest() &&
            dvalue <= std::numeric_limits<double>::max())
            return ValueTypeCode::F64;
    } catch (const std::invalid_argument &) {
    } catch (const std::out_of_range &) {
    }

    if (token.size() == 16)
        return ValueTypeCode::FIXEDSTR16;
    return ValueTypeCode::STR;
}

// Function to read the CSV file and determine the FileMetaData
FileMetaData generateFileMetaData(const std::string &filename, char delim, size_t sampleRows, bool isMatrix) {
    std::ifstream file(filename);
    if (!file.is_open())
        throw std::runtime_error("Cannot open file: " + filename);
    std::string line;
    std::vector<ValueTypeCode> colTypes; // resized once we know numCols
    bool firstLine = true;
    size_t row = 0;
    while (std::getline(file, line) && row < sampleRows) {
        // Discard empty rows.
        if (isEmptyLine(line))
            continue;

        // If a token may span multiple lines, join subsequent lines until quotes match.
        size_t quoteCount = std::count(line.begin(), line.end(), '\"');
        while ((quoteCount % 2) != 0) {
            std::string nextLine;
            if (!std::getline(file, nextLine))
                break;
            line += "\n" + nextLine;
            quoteCount = std::count(line.begin(), line.end(), '\"');
        }

        size_t pos = 0;
        size_t col = 0;
        // On first non-empty line, determine number of columns.
        if (firstLine) {
            size_t ncols = 1;
            for (char c : line)
                if (c == delim)
                    ncols++;
            colTypes.resize(ncols, ValueTypeCode::SI8); // start with narrow type.
            firstLine = false;
        }
        // Process each token.
        while (pos < line.size() && col < colTypes.size()) {
            size_t tempPos = pos;
            // Extract token using the existing inferValueType helper.
            ValueTypeCode tokenType = inferValueType(line.c_str(), tempPos, delim);
            // Promote type if needed.
            if (generality(tokenType) > generality(colTypes[col]))
                colTypes[col] = tokenType;
            pos = tempPos;
            col++;
        }
        row++;
    }
    file.close();

    std::vector<std::string> labels;
    size_t numCols = colTypes.size();
    bool isSingleValueType = true;
    ValueTypeCode firstValueType = colTypes[0];
    ValueTypeCode maxValueType = colTypes[0];
    for (size_t i = 0; i < numCols; i++) {
        labels.push_back("col_" + std::to_string(i));
        if (generality(colTypes[i]) > generality(maxValueType))
            maxValueType = colTypes[i];
        if (colTypes[i] != firstValueType)
            isSingleValueType = false;
    }
    if (isSingleValueType) {
        colTypes.clear();
        labels.clear();
        colTypes.push_back(maxValueType);
    }
    if (isMatrix)
        return FileMetaData(row, numCols, true, {maxValueType}, {});
    return FileMetaData(row, numCols, isSingleValueType, colTypes, labels);
}

// Function save the positional map
void writePositionalMap(const char *filename, const std::vector<std::vector<std::streampos>> &posMap) {
    std::ofstream posMapFile(std::string(filename) + ".posmap", std::ios::binary);
    if (!posMapFile.is_open()) {
        throw std::runtime_error("Failed to open positional map file");
    }
    
    for (const auto &colPositions : posMap) {
        for (const auto &pos : colPositions) {
            posMapFile.write(reinterpret_cast<const char *>(&pos), sizeof(pos));
        }
    }

    posMapFile.close();
}

// Function to read or create the positional map
std::vector<std::vector<std::streampos>> readPositionalMap(const char *filename, size_t numCols) {
    std::ifstream posMapFile(std::string(filename) + ".posmap", std::ios::binary);
    if (!posMapFile.is_open()) {
        std::cerr << "Positional map file not found, creating a new one." << std::endl;
        return std::vector<std::vector<std::streampos>>(numCols);
    }
    posMapFile.seekg(0, std::ios::end);
    auto fileSize = posMapFile.tellg();
    posMapFile.seekg(0, std::ios::beg);
    size_t totalEntries = fileSize / sizeof(std::streampos);
    if (totalEntries % numCols != 0) {
        throw std::runtime_error("Incorrect number of entries in posmap file");
    }
    size_t numRows = totalEntries / numCols;
    std::vector<std::vector<std::streampos>> posMap(numCols, std::vector<std::streampos>(numRows));
    // Read in column-major order:
    for (size_t col = 0; col < numCols; col++) {
        for (size_t i = 0; i < numRows; i++) {
            posMap[col][i] = 0;
            posMapFile.read(reinterpret_cast<char *>(&posMap[col][i]), sizeof(std::streampos));
        }
    }
    posMapFile.close();
    return posMap;
}

void writeRelativePosMap(const char* filename,
                         const std::vector<uint32_t>& rowStartMap,
                         const std::vector<std::vector<uint16_t>>& relPosMap) {
    std::string posmapFile = getPosMapFile(filename);
    std::ofstream ofs(posmapFile, std::ios::binary);
    if (!ofs)
        throw std::runtime_error("Could not open file for writing positional map.");
    
    // Write the number of rows.
    uint32_t numRows = static_cast<uint32_t>(rowStartMap.size());
    ofs.write(reinterpret_cast<const char*>(&numRows), sizeof(uint32_t));

    // Write the absolute row start positions.
    ofs.write(reinterpret_cast<const char*>(rowStartMap.data()), numRows * sizeof(uint32_t));

    // Write the number of columns.
    uint32_t numCols = static_cast<uint32_t>(relPosMap.size());
    ofs.write(reinterpret_cast<const char*>(&numCols), sizeof(uint32_t));

    // For each column, write its size (should equal numRows) and then its relative offsets.
    for (const auto &colVec : relPosMap) {
        uint32_t colSize = static_cast<uint32_t>(colVec.size());
        ofs.write(reinterpret_cast<const char*>(&colSize), sizeof(uint32_t));
        ofs.write(reinterpret_cast<const char*>(colVec.data()), colSize * sizeof(uint16_t));
    }
    ofs.close();
}

std::pair<std::vector<uint32_t>, std::vector<std::vector<uint16_t>>>
readRelativePosMap(const char* filename, size_t numRows, size_t numCols) {
    std::string posmapFile = getPosMapFile(filename);
    std::ifstream ifs(posmapFile, std::ios::binary);
    if (!ifs)
        throw std::runtime_error("Could not open positional map file for reading.");

    // Read the number of rows.
    uint32_t storedNumRows = 0;
    ifs.read(reinterpret_cast<char*>(&storedNumRows), sizeof(uint32_t));
    if (storedNumRows != numRows)
        throw std::runtime_error("Row count in positional map does not match expected value.");

    // Read the absolute row start positions.
    std::vector<uint32_t> rowStartMap(storedNumRows);
    ifs.read(reinterpret_cast<char*>(rowStartMap.data()), storedNumRows * sizeof(uint32_t));

    // Read the number of columns.
    uint32_t storedNumCols = 0;
    ifs.read(reinterpret_cast<char*>(&storedNumCols), sizeof(uint32_t));
    if (storedNumCols != numCols)
        throw std::runtime_error("Column count in positional map does not match expected value.");

    // Read the relative offsets per column.
    std::vector<std::vector<uint16_t>> relPosMap(numCols);
    for (size_t c = 0; c < numCols; c++) {
        uint32_t colSize = 0;
        ifs.read(reinterpret_cast<char*>(&colSize), sizeof(uint32_t));
        if (colSize != numRows)
            throw std::runtime_error("Relative mapping size for a column does not match expected number of rows.");
        relPosMap[c].resize(colSize);
        ifs.read(reinterpret_cast<char*>(relPosMap[c].data()), colSize * sizeof(uint16_t));
    }
    ifs.close();
    return {rowStartMap, relPosMap};
}

struct PosMapHeader {
    char magic[4];          // e.g. "PMap"
    uint16_t version;       // currently 1
    uint32_t numRows;       // number of rows
    uint32_t numCols;       // number of columns
    uint8_t offsetSize;     // byte-width for relative offsets (e.g., 2)
};

void writeRelativePosMap(const char* filename, const std::vector<std::vector<std::streampos>>& posMap) {
    std::string posmapFile = getPosMapFile(filename);
    std::ofstream ofs(posmapFile, std::ios::binary);
    if (!ofs.good())
        throw std::runtime_error("Unable to open posMap file for writing.");
    
    // Decide which storage size to use for relative offsets:
    // One row always stores an absolute offset for the first column (we store that as uint32_t)
    // and for every subsequent column, we store relative offset to previous delimiter.
    // Assume that for our CSV, relative offsets fit into uint16_t.
    const uint8_t relSize = 2;  // 2 bytes per relative offset

    uint32_t numRows = static_cast<uint32_t>(posMap[0].size());
    uint32_t numCols = static_cast<uint32_t>(posMap.size());
    PosMapHeader header = { {'P', 'M', 'A', 'P'}, 1, numRows, numCols, relSize };
    ofs.write(reinterpret_cast<const char*>(&header), sizeof(header));

    // Write each row
    for (uint32_t r = 0; r < numRows; r++) {
        // Write the absolute offset for the first column as uint32_t.
        uint32_t absOffset = static_cast<uint32_t>(posMap[0][r]);
        ofs.write(reinterpret_cast<const char*>(&absOffset), sizeof(uint32_t));

        // For remaining columns, store relative offsets.
        for (uint32_t c = 1; c < numCols; c++) {
            // Compute the relative offset from the previous delimiter.
            uint32_t relative = static_cast<uint32_t>(posMap[c][r] - posMap[c - 1][r]);
            // Ensure that the relative offset fits into uint16_t.
            if(relative > std::numeric_limits<uint16_t>::max())
                throw std::runtime_error("Relative offset too large to store in 16 bits.");
            uint16_t shortRel = static_cast<uint16_t>(relative);
            ofs.write(reinterpret_cast<const char*>(&shortRel), sizeof(uint16_t));
        }
    }
    ofs.close();
}

std::vector<std::vector<std::streampos>> readRelativePosMap(const char* filename, size_t expectedCols) {
    std::string posmapFile = getPosMapFile(filename);
    std::ifstream ifs(posmapFile, std::ios::binary);
    if (!ifs.good())
        throw std::runtime_error("Failed to open posMap file for reading.");
    
    PosMapHeader header;
    ifs.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (std::string(header.magic, 4) != "PMap")
        throw std::runtime_error("Invalid posMap file format.");
    if (header.numCols != expectedCols)
        throw std::runtime_error("Column count mismatch in posMap file.");

    size_t numRows = header.numRows;
    size_t numCols = header.numCols;
    std::vector<std::vector<std::streampos>> posMap(numCols, std::vector<std::streampos>(numRows, 0));

    // Read absolute offset of first column per row (stored as uint32_t)
    for (size_t r = 0; r < numRows; r++) {
        uint32_t absOffset;
        ifs.read(reinterpret_cast<char*>(&absOffset), sizeof(uint32_t));
        posMap[0][r] = absOffset;
    }
    // For columns 1..(numCols-1), read relative offsets stored as uint16_t and add them cumulatively.
    for (size_t c = 1; c < numCols; c++) {
        for (size_t r = 0; r < numRows; r++) {
            uint16_t relOffset;
            ifs.read(reinterpret_cast<char*>(&relOffset), sizeof(uint16_t));
            posMap[c][r] = posMap[c-1][r] + static_cast<std::streamoff>(relOffset);
        }
    }
    return posMap;
}
