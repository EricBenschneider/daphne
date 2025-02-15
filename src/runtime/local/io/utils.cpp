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

#include <iostream>
#include <runtime/local/io/utils.h>

// create positional map based on csv data

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