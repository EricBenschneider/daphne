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
#include <limits>

bool isSameBaseType(const std::vector<ValueTypeCode>& schema) {
    if (schema.empty()) return true;

    auto isFloatType = [](ValueTypeCode type) {
        return type == ValueTypeCode::F32 || type == ValueTypeCode::F64;
    };

    auto isIntType = [](ValueTypeCode type) {
        return type == ValueTypeCode::SI8 || type == ValueTypeCode::SI32 || type == ValueTypeCode::SI64 ||
               type == ValueTypeCode::UI8 || type == ValueTypeCode::UI32 || type == ValueTypeCode::UI64;
    };

    bool allFloat = std::all_of(schema.begin(), schema.end(), isFloatType);
    bool allInt = std::all_of(schema.begin(), schema.end(), isIntType);

    return allFloat || allInt;
}

int generality(ValueTypeCode type) {
    switch (type) {
        case ValueTypeCode::UI8:
            return 0;
        case ValueTypeCode::SI8:
            return 1;
        case ValueTypeCode::UI32:
            return 2;
        case ValueTypeCode::SI32:
            return 3;
        case ValueTypeCode::UI64:
            return 4;
        case ValueTypeCode::SI64:
            return 5;
        case ValueTypeCode::F32:
            return 6;
        case ValueTypeCode::F64:
            return 7;
        default: // ValueTypeCode::STR, ValueTypeCode::FIXEDSTR16
            return 8;
    }
}

// Function to infer the data type of string value
ValueTypeCode inferValueType(const std::string &value) {
    // Check if the value can be parsed as a unsigned integer
    std::istringstream iss(value);
    uint64_t uintValue;
    if (iss >> uintValue && iss.eof()) {
        // Check the range of the unsigned integer to determine the appropriate type
        if (uintValue <= std::numeric_limits<uint8_t>::max()) {
            return ValueTypeCode::UI8;
        } else if (uintValue <= std::numeric_limits<uint32_t>::max()) {
            return ValueTypeCode::UI32;
        } else {
            return ValueTypeCode::UI64;
        }
    }

    // Reset the stringstream and check if the value can be parsed as an signed integer
    iss.clear();
    iss.str(value);
    int64_t intValue;
    if (iss >> intValue && iss.eof()) {
        // Check the range of the signed integer to determine the appropriate type
        if (intValue >= std::numeric_limits<int8_t>::min() && intValue <= std::numeric_limits<int8_t>::max()) {
            return ValueTypeCode::SI8;
        } else if (intValue >= std::numeric_limits<int32_t>::min() && intValue <= std::numeric_limits<int32_t>::max()) {
            return ValueTypeCode::SI32;
        } else {
            return ValueTypeCode::SI64;
        }
    }

    // Reset the stringstream and check if the value can be parsed as a floating-point number
    iss.clear();
    iss.str(value);
    float floatValue;
    if (iss >> floatValue && iss.eof()) {
        return ValueTypeCode::F32;
    }

    // Reset the stringstream and check if the value can be parsed as a floating-point number
    iss.clear();
    iss.str(value);
    double doubleValue;
    if (iss >> doubleValue && iss.eof()) {
        return ValueTypeCode::F64;
    }

    return ValueTypeCode::STR;
}

// Function to read the CSV file and determine the FileMetaData
FileMetaData generateFileMetaData(const std::string &filename, bool isMatrix) {
    std::ifstream file(filename);
    std::string line;
    std::vector<ValueTypeCode> schema;
    std::vector<std::string> labels;
    size_t numRows = 0;
    size_t numCols = 0;
    bool isSingleValueType = true;
    // set the default value type to the most specific value type
    ValueTypeCode maxValueType = ValueTypeCode::UI8;
    ValueTypeCode currentType = ValueTypeCode::INVALID;

    if (file.is_open()) {
        // TODO: get column labels if any

        // Read the rest of the file to infer the schema
        while (std::getline(file, line)) {
            std::stringstream ss(line);
            std::string value;
            size_t colIndex = 0;
            while (std::getline(ss, value, ',')) {
                //trim any whitespaces for last element in line
                // Remove any newline characters from the end of the value
                if (!value.empty() && (value.back() == '\n' || value.back() == '\r')) {
                    value.pop_back();
                }
                ValueTypeCode inferredType = inferValueType(value);
                std::cout << "inferred valueType: " << static_cast<int>(inferredType) << ", " << value << "." << std::endl;
                // fill empty schema with inferred type
                if (numCols <= colIndex) {
                    schema.push_back(inferredType);
                }
                currentType = schema[colIndex];
                //update the current type if the inferred type is more specific
                if (generality(currentType) < generality(inferredType)) {
                    currentType = inferredType;
                    schema[colIndex]= currentType;
                }
                if (generality(maxValueType) < generality(currentType)) {
                    maxValueType = currentType;
                }
                colIndex++;
            }
            numCols = std::max(numCols, colIndex);
            numRows++;
        }
        file.close();
    }else {
        std::cerr << "Unable to open file: " << filename << std::endl;
    }

    std::cout << "Generated valueType from CSV file: " << static_cast<int>(currentType) << std::endl;
    std::cout << "Max valueType from CSV file: " << static_cast<int>(maxValueType) << std::endl;
    if (isMatrix) {
        isSingleValueType = isSameBaseType(schema);
    }else {
        //check if all columns are from same valueType
        for (const auto &typeCode : schema){
            if (typeCode != currentType){
                isSingleValueType = false;
                break;
            }
        }
    }
    if (isSingleValueType) {
        schema.clear();
        schema.push_back(maxValueType);
    }
    return FileMetaData(numRows, numCols, isSingleValueType, schema, labels);
}