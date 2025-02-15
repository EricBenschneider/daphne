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
void writePositionalMap(const char* filename, const std::vector<std::vector<std::streampos>> &posMap) {
    std::ofstream ofs(getPosMapFile(filename), std::ios::binary);
    if (!ofs.good())
        throw std::runtime_error("Cannot open file for writing posMap");
    size_t numRows = posMap[0].size();
    size_t numCols = posMap.size();
    ofs.write(reinterpret_cast<const char*>(&numRows), sizeof(numRows));
    ofs.write(reinterpret_cast<const char*>(&numCols), sizeof(numCols));
    // Write full absolute offsets for col 0.
    for (size_t r = 0; r < numRows; r++) {
        auto offset = posMap[0][r];
        ofs.write(reinterpret_cast<const char*>(&offset), sizeof(offset));
    }
    // Write relative offsets for cols > 0.
    for (size_t c = 1; c < numCols; c++) {
        for (size_t r = 0; r < numRows; r++) {
            int32_t rel = static_cast<int32_t>(posMap[c][r] - posMap[0][r]);
            ofs.write(reinterpret_cast<const char*>(&rel), sizeof(rel));
        }
    }
}

// Updated readPositionalMap: reconstruct full offsets.
std::vector<std::vector<std::streampos>> readPositionalMap(const char* filename, size_t numCols) {
    std::ifstream ifs(getPosMapFile(filename), std::ios::binary);
    if (!ifs.good())
        throw std::runtime_error("Cannot open posMap file");
    size_t numRows;
    size_t colsStored;
    ifs.read(reinterpret_cast<char*>(&numRows), sizeof(numRows));
    ifs.read(reinterpret_cast<char*>(&colsStored), sizeof(colsStored));
    if (colsStored != numCols)
        throw std::runtime_error("posMap file: stored number of columns does not match");
    std::vector<std::vector<std::streampos>> posMap(numCols, std::vector<std::streampos>(numRows));
    // Read column 0 absolute offsets.
    for (size_t r = 0; r < numRows; r++) {
        std::streampos offset;
        ifs.read(reinterpret_cast<char*>(&offset), sizeof(offset));
        posMap[0][r] = offset;
    }
    // Read each column c > 0 relative offsets and reconstruct full offsets.
    for (size_t c = 1; c < numCols; c++) {
        for (size_t r = 0; r < numRows; r++) {
            int32_t rel;
            ifs.read(reinterpret_cast<char*>(&rel), sizeof(rel));
            posMap[c][r] = posMap[0][r] + rel;
        }
    }
    return posMap;
}