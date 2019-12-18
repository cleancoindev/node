#include <gtest/gtest.h>

#include "csdb/address.hpp"

#include "packstream.hpp"
#include "datastream.hpp"

#include <lib/system/utils.hpp>

using DataPtr = std::shared_ptr<char>;

struct StreamData {
    boost::asio::mutable_buffer encoded;
    DataPtr data;
};

const cs::PublicKey kPublicKey = {0x53, 0x4b, 0xd3, 0xdf, 0x77, 0x29, 0xfd, 0xcf, 0xea, 0x4a, 0xcd, 0x0e, 0xcc, 0x14, 0xaa, 0x05,
                                  0x0b, 0x77, 0x11, 0x6d, 0x8f, 0xcd, 0x80, 0x4b, 0x45, 0x36, 0x6b, 0x5c, 0xae, 0x4a, 0x06, 0x82};

void displayRawData(const void* data, const size_t size) {
    std::cout << "data = {";

    for (auto i = 0u; i < size; i++) {
        std::cout << "0x" << std::hex << std::setw(2) << std::setfill('0') << int(static_cast<const uint8_t*>(data)[i]) << ", ";
    }

    std::cout << "}" << std::dec << std::endl;
}

void displayStreamData(cs::OPackStream& stream) {
    auto ptr = stream.getCurrentPtr();
    auto offset = stream.getCurrentSize();
    displayRawData(ptr - offset, offset);
}

auto getStreamData(cs::OPackStream& stream) {
    auto packets = stream.getPackets();
    DataPtr bufferData(new char[Packet::MaxSize], std::default_delete<char[]>());

    boost::asio::mutable_buffer buffer(bufferData.get(), Packet::MaxSize);
    auto encoded = packets->encode(buffer);

    StreamData streamData {
        encoded,
        bufferData
    };

    return streamData;
}

[[maybe_unused]] const std::size_t kPageSizeForAllocator = 1000;  // 109 is minimal stable

TEST(OPackStream, InitializationWithFragmentedAndNetworkMsgFlags) {
    RegionAllocator allocator;
    cs::OPackStream stream(&allocator, kPublicKey);

    const auto flags = BaseFlags(BaseFlags::Fragmented | BaseFlags::NetworkMsg);
    stream.init(flags);

    auto streamData = getStreamData(stream);
    auto encoded = streamData.encoded;

    const unsigned char encoded_expected[] = {flags, 0x00, 0x00, 0x01, 0x00};
    ASSERT_EQ(encoded.size(), sizeof encoded_expected);
    ASSERT_TRUE(0 == memcmp(encoded.data(), encoded_expected, encoded.size()));
}

TEST(OPackStream, InitializationWithFragmentedFlagOnly) {
    RegionAllocator allocator;
    cs::OPackStream stream(&allocator, kPublicKey);

    const auto flags = BaseFlags::Fragmented;
    stream.init(flags);

    auto streamData = getStreamData(stream);
    auto encoded = streamData.encoded;

    const unsigned char encoded_expected[] = {flags, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x53, 0x4b,
                                              0xd3,  0xdf, 0x77, 0x29, 0xfd, 0xcf, 0xea, 0x4a, 0xcd, 0x0e, 0xcc, 0x14, 0xaa, 0x05, 0x0b,
                                              0x77,  0x11, 0x6d, 0x8f, 0xcd, 0x80, 0x4b, 0x45, 0x36, 0x6b, 0x5c, 0xae, 0x4a, 0x06, 0x82};

    ASSERT_EQ(1u, stream.getPacketsCount());
    ASSERT_EQ(encoded.size(), sizeof encoded_expected);
    ASSERT_TRUE(0 == memcmp(encoded.data(), encoded_expected, encoded.size()));
}

TEST(OPackStream, WithoutInitializationPacketsCountIsZero) {
    RegionAllocator allocator;
    cs::OPackStream stream(&allocator, kPublicKey);

    ASSERT_EQ(0_sz, stream.getPacketsCount());
}

TEST(OPackStream, AfterClearPacketsCountIsZero) {
    RegionAllocator allocator;
    cs::OPackStream stream(&allocator, kPublicKey);

    stream.init(BaseFlags::Fragmented);
    stream.clear();

    ASSERT_EQ(0_sz, stream.getPacketsCount());
}

TEST(OPackStream, getPacketsCount) {
    RegionAllocator allocator;
    cs::OPackStream oPackStream(&allocator, kPublicKey);
    oPackStream.init(BaseFlags::Fragmented | BaseFlags::NetworkMsg);

    ASSERT_EQ(1u, oPackStream.getPacketsCount());
}

TEST(OPackStream, getCurrentPtr) {
    RegionAllocator allocator;
    cs::OPackStream oPackStream(&allocator, kPublicKey);
    oPackStream.init(BaseFlags::Fragmented | BaseFlags::NetworkMsg);

    ASSERT_EQ(1, static_cast<int>(*(oPackStream.getCurrentPtr() - 2)));
}

TEST(OPackStream, getCurrSize) {
    RegionAllocator allocator;
    cs::OPackStream oPackStream(&allocator, kPublicKey);
    oPackStream.init(BaseFlags::Fragmented | BaseFlags::NetworkMsg);

    ASSERT_EQ(5u, oPackStream.getCurrentSize());
}

template<typename T>
void TestConcreteTypeWriteToOPackStream(const T& value, const unsigned char* data, size_t size) {
    RegionAllocator allocator;

    cs::OPackStream stream(&allocator, kPublicKey);
    stream.init(BaseFlags::Fragmented | BaseFlags::NetworkMsg);
    stream << value;

    auto streamData = getStreamData(stream);
    auto encoded = streamData.encoded;

    displayStreamData(stream);

    ASSERT_EQ(1u, stream.getPacketsCount());
    ASSERT_EQ(encoded.size(), size * sizeof(unsigned char));
    ASSERT_TRUE(0 == memcmp(encoded.data(), data, encoded.size()));
}

template <typename T, size_t ArraySize>
void TestConcreteTypeWriteToOPackStream(const T& value, const unsigned char (&data)[ArraySize]) {
    TestConcreteTypeWriteToOPackStream(value, data, ArraySize);
}

TEST(OPackStream, IpAddressWrite) {
    const unsigned char expected[] = {0x03, 0x00, 0x00, 0x01, 0x00, 0x7f, 0x00, 0x00, 0x01};
    TestConcreteTypeWriteToOPackStream(boost::asio::ip::address_v4::from_string("127.0.0.1"), expected);
}

TEST(OPackStream, StdStringWrite) {
    const unsigned char expected[] = {0x03, 0x00, 0x00, 0x01, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x61,
                                      0x73, 0x63, 0x69, 0x69, 0x20, 0x73, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x20, 0x20, 0x20};
    TestConcreteTypeWriteToOPackStream(std::string("ascii string   "), expected);
}

TEST(OPackStream, BytesWrite) {
    const unsigned char expected[] = {0x03, 0x00, 0x00, 0x01, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x00};
    TestConcreteTypeWriteToOPackStream(cs::Bytes{1, 2, 3, 4, 5, 6, 7, 8, 9, 0}, expected);
}

TEST(OPackStream, DISABLED_EmptyPoolWrite) {
    const unsigned char expected[] = {0x03, 0x00, 0x00, 0x01, 0x00, 0x2d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    TestConcreteTypeWriteToOPackStream(csdb::Pool{}, expected);
}

TEST(OPackStream, EmptyTransactionsPacketHashWrite) {
    const unsigned char expected[] = {0x03, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    TestConcreteTypeWriteToOPackStream(cs::TransactionsPacketHash{}, expected);
}

//
// TODO: TransactionsPacket, TransactionsPacketHash, Pool
//

TEST(OPackStream, EmptyTransactionsPacketWrite) {
    const unsigned char expected[] = {0x03, 0x00, 0x00, 0x01, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

    TestConcreteTypeWriteToOPackStream(cs::TransactionsPacket{}, expected);
}

TEST(OPackStream, EmptyPoolHashWrite) {
    const unsigned char expected[] = {0x03, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    TestConcreteTypeWriteToOPackStream(csdb::PoolHash{}, expected);
}

TEST(OPackStream, GeneralVectorWrite) {
    const unsigned char expected[] = {0x03, 0x00, 0x00, 0x01, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x56,
                                      0x34, 0x12, 0x21, 0x43, 0x65, 0x87, 0xab, 0xab, 0xab, 0xab, 0xee, 0xee, 0xee, 0xee};
    std::vector<uint32_t> vector = {0x12345678, 0x87654321, 0xABABABAB, 0xEEEEEEEE};
    TestConcreteTypeWriteToOPackStream(vector, expected);
}

TEST(OPackStream, ByteArrayWrite) {
    const unsigned char expected[] = {0x03, 0x00, 0x00, 0x01, 0x00, 0x12, 0x34, 0x56, 0x78, 0x90, 0xab, 0xcd, 0xef, 0x01, 0x23};
    cs::ByteArray<10> array = {0x12, 0x34, 0x56, 0x78, 0x90, 0xAB, 0xCD, 0xEF, 0x01, 0x23};
    TestConcreteTypeWriteToOPackStream(array, expected);
}

TEST(OPackStream, GeneralIntegerWrite) {
    const unsigned char expected[] = {0x03, 0x00, 0x00, 0x01, 0x00, 0x44, 0x03, 0x62, 0x67};
    uint32_t integer = 0x67620344;
    TestConcreteTypeWriteToOPackStream(integer, expected);
}
