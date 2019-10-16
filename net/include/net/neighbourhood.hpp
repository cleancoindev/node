/* Send blaming letters to @yrtimd */
#ifndef NEIGHBOURHOOD_HPP
#define NEIGHBOURHOOD_HPP

#include <deque>

#include <boost/asio.hpp>

#include <lib/system/allocators.hpp>
#include <lib/system/cache.hpp>
#include <lib/system/common.hpp>

#include "packet.hpp"

namespace ip = boost::asio::ip;

class Network;
class Transport;

class BlockChain;

const uint32_t MaxMessagesToKeep = 128;
const uint32_t MaxResendTimes =
#if defined(WEB_WALLET_NODE)
8;
#else
4;
#endif // !WEB_WALLET_NODE
const cs::Sequence MaxSyncAttempts = 8;

const cs::Sequence BlocksToSync = 16;
const uint32_t WarnsBeforeRefill = 8;

struct Connection;
struct RemoteNode {
    __cacheline_aligned std::atomic<uint64_t> packets = {0};

    __cacheline_aligned std::atomic<uint32_t> strikes = {0};
    __cacheline_aligned std::atomic<bool> blackListed = {ATOMIC_FLAG_INIT};

    void addStrike() {
        strikes.fetch_add(1, std::memory_order_relaxed);
    }

    void setBlackListed(bool b) {
        blackListed.store(b, std::memory_order_relaxed);
    }

    bool isBlackListed() {
        return blackListed.load(std::memory_order_relaxed);
    }

    __cacheline_aligned std::atomic<Connection*> connection = {nullptr};
};

using RemoteNodePtr = MemPtr<TypedSlot<RemoteNode>>;

struct Connection {
    typedef uint64_t Id;

    Connection() = default;

    Connection(Connection&& rhs)
    : id(rhs.id)
    , lastBytesCount(rhs.lastBytesCount.load(std::memory_order_relaxed))
    , lastPacketsCount(rhs.lastPacketsCount)
    , attempts(rhs.attempts)
    , key(rhs.key)
    , in(std::move(rhs.in))
    , specialOut(rhs.specialOut)
    , out(std::move(rhs.out))
    , node(std::move(rhs.node))
    , isSignal(rhs.isSignal)
    , connected(rhs.connected)
    , msgRels(std::move(rhs.msgRels)) {
    }

    Connection(const Connection&) = delete;
    ~Connection() {
    }

    const ip::udp::endpoint& getOut() const {
        return specialOut ? out : in;
    }

    Id id = 0;

    static const uint32_t BytesLimit = 1 << 20;
    mutable std::atomic<uint32_t> lastBytesCount = {0};

    uint64_t lastPacketsCount = 0;
    uint32_t attempts = 0;

    cs::PublicKey key;
    ip::udp::endpoint in;

    bool specialOut = false;
    ip::udp::endpoint out;

    RemoteNodePtr node;

    bool isSignal = false;
    bool connected = false;

    bool isRequested = false;
    uint32_t syncNeighbourRetries = 0;

    struct MsgRel {
        uint32_t acceptOrder = 0;
        bool needSend = true;
    };

    FixedHashMap<cs::Hash, MsgRel, uint16_t, MaxMessagesToKeep> msgRels;

    cs::Sequence lastSeq = 0;

    bool operator!=(const Connection& rhs) const {
        return id != rhs.id || key != rhs.key || in != rhs.in || specialOut != rhs.specialOut || (specialOut && out != rhs.out);
    }
};

using ConnectionPtr = MemPtr<TypedSlot<Connection>>;
using Connections = std::vector<ConnectionPtr>;

class Neighbourhood {
public:
    const static uint32_t MinConnections = 1;
    const static uint32_t MaxConnections = 1024;
    const static uint32_t MaxNeighbours = 256;
    const static uint32_t MinNeighbours = 3;
    const static uint32_t MaxConnectAttempts = 64;

    explicit Neighbourhood(Transport*);

    void chooseNeighbours();
    void sendByNeighbours(const Packet*, bool separate = false);
    void sendByConfidant(const Packet* pack, ConnectionPtr conn);
    void sendByConfidants(const Packet* pack);

    void establishConnection(const ip::udp::endpoint&);
    ConnectionPtr addConfidant(const ip::udp::endpoint&, bool insert = true);
    bool isConfidants() { return confidants_.size() != 0; }
    void removeConfidants();
    void addSignalServer(const ip::udp::endpoint& in, const ip::udp::endpoint& out, RemoteNodePtr);
    bool updateSignalServer(const ip::udp::endpoint& in);

    void gotRegistration(Connection&&, RemoteNodePtr);
    void gotConfirmation(const Connection::Id& my, const Connection::Id& real, const ip::udp::endpoint&, const cs::PublicKey&, RemoteNodePtr);
    void gotRefusal(const Connection::Id&);

    void resendPackets();
    void checkPending(const uint32_t maxNeighbours);
    void checkSilent();
    void checkNeighbours();

    void refreshLimits();

    bool canHaveNewConnection();

    void neighbourHasPacket(RemoteNodePtr, const cs::Hash&, const bool isDirect);
    void neighbourSentPacket(RemoteNodePtr, const cs::Hash&);
    void neighbourSentRenounce(RemoteNodePtr, const cs::Hash&);

    void redirectByNeighbours(const Packet*);

    uint32_t size() const;
    uint32_t getNeighboursCountWithoutSS() const;

    // no thread safe
    Connections getNeigbours() const;
    Connections getNeighboursWithoutSS() const;

    // uses to iterate connections
    std::unique_lock< std::mutex > getNeighboursLock() const;

    // thread safe
    void forEachNeighbour(std::function<void(ConnectionPtr)> func);
    void forEachNeighbourWithoutSS(std::function<void(ConnectionPtr)> func);
    bool forRandomNeighbour(std::function<void(ConnectionPtr)> func);

    void pingNeighbours();
    bool isPingDone();
    bool validateConnectionId(RemoteNodePtr, const Connection::Id, const ip::udp::endpoint&, const cs::PublicKey&, const cs::Sequence);

    ConnectionPtr getConnection(const RemoteNodePtr);
    ConnectionPtr getNextRequestee(const cs::Hash&);
    ConnectionPtr getNeighbour(const std::size_t number);
    ConnectionPtr getRandomSyncNeighbour();
    ConnectionPtr getNeighbourByKey(const cs::PublicKey&);

    void resetSyncNeighbours();
    void registerDirect(const Packet*, ConnectionPtr);

private:
    struct BroadPackInfo {
        Packet pack;

        uint32_t attempts = 0;
        bool sentLastTime = false;

        Connection::Id receivers[MaxNeighbours];
        Connection::Id* recEnd = receivers;
    };

    struct DirectPackInfo {
        Packet pack;

        ConnectionPtr receiver;
        bool received = false;

        uint32_t attempts = 0;
    };

    bool isNewConnectionAvailable() const;
    bool dispatch(BroadPackInfo&, bool separate = false);
    bool dispatch(DirectPackInfo&);

    ConnectionPtr getConnection(const ip::udp::endpoint&);

    void connectNode(RemoteNodePtr, ConnectionPtr);
    void disconnectNode(ConnectionPtr*);

    int getRandomSyncNeighbourNumber(const std::size_t attemptCount = 0);
    ConnectionPtr getRandomNeighbour();

    Transport* transport_;

    TypedAllocator<Connection> connectionsAllocator_;

    //mutable cs::SpinLock nLockFlag_{ATOMIC_FLAG_INIT};
    mutable std::mutex nLockFlag_;
    //mutable cs::SpinLock mLockFlag_{ATOMIC_FLAG_INIT};
    mutable std::mutex mLockFlag_;

    std::deque<ConnectionPtr> neighbours_;
    std::vector<ConnectionPtr> selection_;
    std::vector<ConnectionPtr> confidants_;
    FixedHashMap<ip::udp::endpoint, ConnectionPtr, uint16_t, MaxConnections> connections_;

    struct SenderInfo {
        uint32_t totalSenders = 0;
        uint32_t reaskTimes = 0;
        ConnectionPtr prioritySender;
    };

    FixedHashMap<cs::Hash, SenderInfo, uint16_t, MaxMessagesToKeep> msgSenders_;
    FixedHashMap<cs::Hash, BroadPackInfo, uint32_t, MaxRememberPackets> msgBroads_;
    FixedHashMap<cs::Hash, DirectPackInfo, uint32_t, MaxRememberPackets> msgDirects_;
};

#endif  // NEIGHBOURHOOD_HPP
