#include <apihandler.hpp>
#include <serializer.hpp>

#include "gtest/gtest.h"

TEST(ThriftAPI, SCISerialization) {
    ASSERT_NE(9, 0);

    //std::string bytes = cs::Utils::hexToString("610000000B0001000000087472616E736665720F00020C000000020B00110000002C33334A5778414D78587A4D6D7575416F425A7741666333714850367870315754696D6F46716B64564655776B000B00110000000131000F00030B000000000200040000");
    //std::string bytes = cs::Utils::hexToString("0B0001000000087472616E736665720F00020C000000020B00110000002C33334A5778414D78587A4D6D7575416F425A7741666333714850367870315754696D6F46716B64564655776B000B00110000000131000F00030B000000000200040000");
    //api::SmartContractInvocation sci = cs::Serializer::deserialize<api::SmartContractInvocation>(std::move(bytes));

    std::string bytez = cs::Utils::hexToString("0B0001000000087472616E736665720F00020C000000020B00110000002C33334A5778414D78587A4D6D7575416F425A7741666333714850367870315754696D6F46716B64564655776B000B00110000000131000F00030B0000000002000400060006000100");
    api::SmartContractInvocation sci = cs::Serializer::deserialize<api::SmartContractInvocation>(std::move(bytez));
}

//TEST(ThriftAPI, TransactionSerialization) {
//    std::string bytes1 = cs::Utils::hexToString("0A000114000000000000001E4E57610E9461F703E3D0BEF1EC811A2DBA1D828B00B7AE2E793DBE398F8C63BC0B46FAA950462EA6FEF0311C292A4355FD92597C88D2FEA43FFF751682F57B000000000000000000000000664C0101610000000B0001000000087472616E736665720F00020C000000020B00110000002C33334A5778414D78587A4D6D7575416F425A7741666333714850367870315754696D6F46716B64564655776B000B00110000000131000F00030B000000000200040000");
//    api::Transaction t1 = cs::Serializer::deserialize<api::Transaction>(std::move(bytes1));
//
//    std::string bytes2 = cs::Utils::hexToString("1400000000001E4E57610E9461F703E3D0BEF1EC811A2DBA1D828B00B7AE2E793DBE398F8C63BC0B46FAA950462EA6FEF0311C292A4355FD92597C88D2FEA43FFF751682F57B000000000000000000000000664C0101660000000B0001000000087472616E736665720F00020C000000020B00110000002C33334A5778414D78587A4D6D7575416F425A7741666333714850367870315754696D6F46716B64564655776B000B00110000000131000F00030B0000000002000400060006000100");
//    api::Transaction t2 = cs::Serializer::deserialize<api::Transaction>(std::move(bytes2));
//}