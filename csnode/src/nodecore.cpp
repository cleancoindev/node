#include "nodecore.hpp"
#include <datastream.hpp>

namespace cs {
Zero::Zero() {
    hash.fill(0);
    signature.fill(0);
    key.fill(0);
    timeStamp = 0U;
}
TimeMoney::TimeMoney(uint64_t t, csdb::Amount am) {
    time = t;
    amount = am;
}
}  // namespace cs

namespace {
// the only purpose of the object is to init static members
static const cs::Zero zero;
}  // namespace

std::size_t std::hash<cs::TransactionsPacketHash>::operator()(const cs::TransactionsPacketHash& packetHash) const noexcept {
    const std::size_t p = 16777619;
    std::size_t hash = 2166136261;

    auto data = packetHash.toBinary();
    auto size = data.size();

    for (std::size_t i = 0; i < size; ++i) {
        hash = (hash ^ data[i]) * p;
    }

    hash += hash << 13;
    hash ^= hash >> 7;
    hash += hash << 3;
    hash ^= hash >> 17;
    hash += hash << 5;

    return hash;
}

cs::Bytes cs::RoundTable::toBinary() const {
    cs::Bytes bytes;
    cs::ODataStream stream(bytes);

    stream << round;
    stream << confidants;

    return bytes;
}
