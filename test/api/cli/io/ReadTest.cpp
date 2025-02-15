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

#include <api/cli/Utils.h>
#include <parser/metadata/MetaDataParser.h>
#include <tags.h>

#include <catch.hpp>

#include <string>

const std::string dirPath = "test/api/cli/io/";

// TODO Add script-level test cases for reading files in various formats.
// This test case used to read a COO file, but being a quick fix, it was not
// integrated cleanly. There should either be a seprate reader for COO, or we
// should do that via the respective Matrix Market format.
#if 0
TEST_CASE("readSparse", TAG_IO) {
    auto arg = "filename=\"" + dirPath + "readSparse.coo\"";
    compareDaphneToRef(dirPath + "readSparse.txt",
        dirPath + "readSparse.daphne",
        "--select-matrix-representations",
        "--args",
        arg.c_str());
}
#endif

TEST_CASE("readFrameFromCSV", TAG_IO) {
    compareDaphneToRef(dirPath + "testReadFrame.txt", dirPath + "testReadFrame.daphne");
}

TEST_CASE("readFrameFromCSVOptimized", TAG_IO) {
    std::string filename = dirPath + "testReadFrame.txt";
    std::filesystem::remove(filename + ".posmap");
    std::filesystem::remove(filename + ".dbdf");
    compareDaphneToRef(dirPath + "testReadFrame.txt", dirPath + "testReadFrame.daphne", "--second-read-opt");
}

TEST_CASE("readFrameWithSingleValueType", TAG_IO) {
    if (std::filesystem::exists(dirPath + "ReadCsv1-1.csv.meta")) {
        std::filesystem::remove(dirPath + "ReadCsv1-1.csv.meta");
    }
    compareDaphneToRef(dirPath + "testReadFrameWithNoMeta.txt", dirPath + "testReadFrameWithNoMeta.daphne");
    REQUIRE(std::filesystem::exists(dirPath + "ReadCsv1-1.csv.meta"));
    FileMetaData fmd = MetaDataParser::readMetaData(dirPath + "ReadCsv1-1.csv");
    REQUIRE(fmd.numRows == 2);
    REQUIRE(fmd.numCols == 4);
    REQUIRE(fmd.labels.empty());
    REQUIRE(fmd.schema.size() == 1);
    REQUIRE(fmd.schema[0] == ValueTypeCode::F32);

    std::filesystem::remove(dirPath + "ReadCsv1-1.csv.meta");
}

TEST_CASE("readFrameWithMixedType", TAG_IO) {
    if (std::filesystem::exists(dirPath + "ReadCsv3-1.csv.meta")) {
        std::filesystem::remove(dirPath + "ReadCsv3-1.csv.meta");
    }
    compareDaphneToRef(dirPath + "testReadStringIntoFrameNoMeta.txt", dirPath + "testReadFrameWithMixedTypes.daphne");
    REQUIRE(std::filesystem::exists(dirPath + "ReadCsv3-1.csv.meta"));
    FileMetaData fmd = MetaDataParser::readMetaData(dirPath + "ReadCsv3-1.csv");
    REQUIRE(fmd.numRows == 4);
    REQUIRE(fmd.numCols == 3);
    REQUIRE(fmd.labels.size() == 3);
    REQUIRE(fmd.labels.size() == fmd.schema.size());
    for (size_t i = 0; i < fmd.labels.size(); i++) {
        REQUIRE(fmd.labels[i] == "col_" + std::to_string(i));
    }
    std::filesystem::remove(dirPath + "ReadCsv3-1.csv.meta");
}

TEST_CASE("readStringValuesIntoFrameFromCSV", TAG_IO) {
    compareDaphneToRef(dirPath + "testReadStringIntoFrame.txt", dirPath + "testReadStringIntoFrame.daphne", "--second-read-opt");
}

TEST_CASE("readMatrixFromCSV", TAG_IO) {
    compareDaphneToRef(dirPath + "testReadMatrix.txt", dirPath + "testReadMatrix.daphne");
}

TEST_CASE("readStringMatrixFromCSV", TAG_IO) {
    compareDaphneToRef(dirPath + "testReadStringMatrix.txt", dirPath + "testReadStringMatrix.daphne");
}

// does not yet work!
// TEST_CASE("readReadMatrixFromCSV_DynamicPath", TAG_IO)
// {
//     compareDaphneToRef(dirPath + "testReadMatrix.txt", dirPath +
//     "testReadMatrix_DynamicPath.daphne");
// }