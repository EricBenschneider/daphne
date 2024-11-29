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

#include <runtime/local/io/ReadCsv.h>
#include <runtime/local/io/utils.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <type_traits>
#include <limits>

// Function to infer the data type of a string value
ValueTypeCode inferValueType(const std::string &value) {
    if (value == "true" || value == "false") {
        return ValueTypeCode::STR;
        //return ValueTypeCode::BOOLEAN; // TODO: introduce bool as ValueTypeCode
    }
    try {
        int intValue = std::stoi(value);
        if (intValue >= std::numeric_limits<int8_t>::min() && intValue <= std::numeric_limits<int8_t>::max()) {
            return ValueTypeCode::SI8;
        }
        return ValueTypeCode::SI32;
    } catch (...) {}
    try {
        std::stol(value);
        return ValueTypeCode::SI64;
    } catch (...) {}
    try {
        unsigned int uintValue = std::stoul(value);
        if (uintValue <= std::numeric_limits<uint8_t>::max()) {
            return ValueTypeCode::UI8;
        }
        return ValueTypeCode::UI32;
    } catch (...) {}
    try {
        std::stoull(value);
        return ValueTypeCode::UI64;
    } catch (...) {}
    try {
        std::stof(value);
        return ValueTypeCode::F32;
    } catch (...) {}
    try {
        std::stod(value);
        return ValueTypeCode::F64;
    } catch (...) {}
    if (value.length() == 16) {
        return ValueTypeCode::FIXEDSTR16;
    }
    return ValueTypeCode::STR;
}

// Function to read the CSV file and determine the FileMetaData
FileMetaData generateFileMetaData(const std::string &filename) {
    std::ifstream file(filename);
    std::string line;
    std::vector<ValueTypeCode> schema;
    std::vector<std::string> labels;
    size_t numRows = 0;
    size_t numCols = 0;
    bool isSingleValueType = true;
    ValueTypeCode firstColumnType = ValueTypeCode::INVALID;

    if (file.is_open()) {
        // Read the first line to get the column labels
        if (std::getline(file, line)) {
            std::stringstream ss(line);
            std::string label;
            while (std::getline(ss, label, ',')) {
                labels.push_back(label);
            }
            numCols = labels.size();
        }

        // Read the rest of the file to infer the schema
        while (std::getline(file, line)) {
            std::stringstream ss(line);
            std::string value;
            size_t colIndex = 0;
            while (std::getline(ss, value, ',')) {
                ValueTypeCode inferredType = inferValueType(value);
                //if (numRows == 0) { // TODO: check if first row differs from rest
                    schema.push_back(inferredType);
                    if (colIndex == 0) {
                        firstColumnType = inferredType;
                    } else if (inferredType != firstColumnType) {
                        isSingleValueType = false;
                    }
                /*} else {
                    if (schema[colIndex] != inferredType) {
                        schema[colIndex] = ValueTypeCode::STR;
                        isSingleValueType = false;
                    }
                }*/
                colIndex++;
            }
            numRows++;
        }
        file.close();
    }

    return FileMetaData(numRows, numCols, isSingleValueType, schema, labels);
}