#include <catch.hpp>
#include <runtime/local/io/FileMetaData.h>
#include <runtime/local/io/utils.h>
#include <fstream>
#include <sstream>
#include <parser/metadata/MetaDataParser.h>

const std::string dirPath = "/daphne/test/runtime/local/io/generateMetaData/";

TEST_CASE("generated metadata matches saved metadata", "[metadata]") {
    for (int i = 1; i <= 5; ++i) {
        std::string rootPath = "\\\\wsl.localhost\\Ubuntu-CUDA\\home\\projects\\daphne\\test\\runtime\\local\\io\\";
        std::string csvFilename = dirPath + "generateMetaData" + std::to_string(i) + ".csv";

        // Read metadata from saved metadata file
        FileMetaData readMetaData = MetaDataParser::readMetaData(csvFilename);

        // Generate metadata from CSV file
        FileMetaData generatedMetaData = generateFileMetaData(csvFilename, false);

        // Check if the generated metadata matches the read metadata
        REQUIRE(generatedMetaData.numRows == readMetaData.numRows);
        REQUIRE(generatedMetaData.numCols == readMetaData.numCols);
        REQUIRE(generatedMetaData.isSingleValueType == readMetaData.isSingleValueType);
        REQUIRE(generatedMetaData.schema == readMetaData.schema);
        REQUIRE(generatedMetaData.labels == readMetaData.labels);
    }
}
/*
TEST_CASE("GenerateMetaData - AllUint8ExceptLastUint64", "[generateMetaData]") {
    std::string filename = "all_uint8_except_last_uint64.csv";
    createCSVFile(filename, "1,1,1,1,1,1,1,1,1,18446744073709551615");

    FileMetaData metaData = generateMetaData(filename);
    REQUIRE(metaData.numRows == 1);
    REQUIRE(metaData.numCols == 10);
    REQUIRE(metaData.columnTypes[9] == ValueTypeCode::UI64);

    removeCSVFile(filename);
}

TEST_CASE("GenerateMetaData - MixedTypes", "[generateMetaData]") {
    std::string filename = "mixed_types.csv";
    createCSVFile(filename, "1,1.0,hello,18446744073709551615");

    FileMetaData metaData = generateMetaData(filename);
    REQUIRE(metaData.numRows == 1);
    REQUIRE(metaData.numCols == 4);
    REQUIRE(metaData.columnTypes[0] == ValueTypeCode::UI8);
    REQUIRE(metaData.columnTypes[1] == ValueTypeCode::F64);
    REQUIRE(metaData.columnTypes[2] == ValueTypeCode::STR);
    REQUIRE(metaData.columnTypes[3] == ValueTypeCode::UI64);

    removeCSVFile(filename);
}

TEST_CASE("GenerateMetaData - LargeNumberOfColumns", "[generateMetaData]") {
    std::string filename = "large_number_of_columns.csv";
    std::string content = "1";
    for (int i = 0; i < 999; ++i) {
    content += ",1";
    }
    createCSVFile(filename, content);

    FileMetaData metaData = generateMetaData(filename);
    REQUIRE(metaData.numRows == 1);
    REQUIRE(metaData.numCols == 1000);
    for (int i = 0; i < 1000; ++i) {
    REQUIRE(metaData.columnTypes[i] == ValueTypeCode::UI8);
    }

    removeCSVFile(filename);
}

TEST_CASE("GenerateMetaData - EmptyValues", "[generateMetaData]") {
    std::string filename = "empty_values.csv";
    createCSVFile(filename, "1,,3\n4,5,");

    FileMetaData metaData = generateMetaData(filename);
    REQUIRE(metaData.numRows == 2);
    REQUIRE(metaData.numCols == 3);
    REQUIRE(metaData.columnTypes[0] == ValueTypeCode::UI8);
    REQUIRE(metaData.columnTypes[1] == ValueTypeCode::UI8); // Assuming empty values default to UI8
    REQUIRE(metaData.columnTypes[2] == ValueTypeCode::UI8);

    removeCSVFile(filename);
}*/