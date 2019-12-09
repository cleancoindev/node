#ifndef POOLSYNCHRONIZER_HPP
#define POOLSYNCHRONIZER_HPP

#include <csdb/pool.hpp>

#include <csnode/blockchain.hpp>
#include <csnode/nodecore.hpp>

#include <lib/system/timer.hpp>
#include <lib/system/signals.hpp>

#include <net/neighbourhood.hpp>

class Node;

namespace cs {
using PoolSynchronizerRequestSignal = cs::Signal<void(const cs::PublicKey& target, const PoolsRequestedSequences& sequences, std::size_t packet)>;

class PoolSynchronizer {
public:
    explicit PoolSynchronizer(Transport* transport, BlockChain* blockChain);

    void sync(cs::RoundNumber roundNum, cs::RoundNumber difference = roundDifferentForSync);
    void syncLastPool();

    // syncro get functions
    void getBlockReply(cs::PoolsBlock&& poolsBlock, std::size_t packetNum);

    // syncro send functions
    void sendBlockRequest();

    bool isSyncroStarted() const;

    static const cs::RoundNumber roundDifferentForSync = cs::values::kDefaultMetaStorageMaxSize;

public signals:
    PoolSynchronizerRequestSignal sendRequest;

private slots:
    void onTimeOut();

    void onWriteBlock(const csdb::Pool& pool);
    void onWriteBlock(const cs::Sequence sequence);
    void onRemoveBlock(const csdb::Pool& pool);

public slots:
    void onStoreBlockTimeElapsed();
    void onPingReceived(cs::Sequence sequence, const cs::PublicKey& publicKey);
    void onNeighbourAdded(const cs::PublicKey& publicKey, cs::Sequence sequence);
    void onNeighbourRemoved(const cs::PublicKey& publicKey);

private:
    enum class CounterType;
    enum class SequenceRemovalAccuracy;
    class NeighboursSetElemet;

    // pool sync progress
    bool showSyncronizationProgress(const cs::Sequence lastWrittenSequence) const;

    bool checkActivity(const CounterType counterType);

    void sendBlock(const NeighboursSetElemet& neighbour);

    bool getNeededSequences(NeighboursSetElemet& neighbour);

    void checkNeighbourSequence(const cs::Sequence sequence, const SequenceRemovalAccuracy accuracy);

    void removeExistingSequence(const cs::Sequence sequence, const SequenceRemovalAccuracy accuracy);

    bool isLastRequest() const;

    void synchroFinished();
    void printNeighbours(const std::string& funcName) const;

private:
    enum class CounterType {
        TIMER
    };

    enum class SequenceRemovalAccuracy {
        EXACT,
        LOWER_BOUND,
        UPPER_BOUND
    };

    class NeighboursSetElemet {
    public:
        NeighboursSetElemet() = default;

        explicit NeighboursSetElemet(const cs::PublicKey& publicKey)
        : key_(publicKey) {
        }

        explicit NeighboursSetElemet(const cs::PublicKey& publicKey, cs::Sequence blockPoolsCount)
        : key_(publicKey) {
            sequences_.reserve(blockPoolsCount);
        }

        inline bool removeSequnce(const cs::Sequence sequence, const SequenceRemovalAccuracy accuracy) {
            if (sequences_.empty()) {
                return false;
            }

            bool success = false;

            switch (accuracy) {
                case SequenceRemovalAccuracy::EXACT: {
                    auto it = std::find(sequences_.begin(), sequences_.end(), sequence);
                    if (it != sequences_.end()) {
                        sequences_.erase(it);
                        success = true;
                    }
                    break;
                }
                case SequenceRemovalAccuracy::LOWER_BOUND: {
                    auto it = std::upper_bound(sequences_.begin(), sequences_.end(), sequence);
                    if (it != sequences_.begin()) {
                        sequences_.erase(sequences_.begin(), it);
                        success = true;
                    }
                    break;
                }
                case SequenceRemovalAccuracy::UPPER_BOUND: {
                    auto it = std::lower_bound(sequences_.begin(), sequences_.end(), sequence);
                    if (it != sequences_.end()) {
                        sequences_.erase(it, sequences_.end());
                        success = true;
                    }
                    break;
                }
            }
            return success;
        }
        inline void setSequences(const PoolsRequestedSequences& sequences) {
            resetSequences();
            sequences_ = sequences;
        }
        inline void addSequences(const cs::Sequence sequence) {
            sequences_.push_back(sequence);
        }
        inline void reset() {
            resetSequences();
        }
        inline void resetSequences() {
            sequences_.clear();
        }
        inline void setPublicKey(const cs::PublicKey& publicKey) {
            key_ = publicKey;
        }
        inline void setMaxSequence(cs::Sequence sequence) {
            maxSequence_ = sequence;
        }

        inline const cs::PublicKey& publicKey() const {
            return key_;
        }
        inline const PoolsRequestedSequences& sequences() const{
            return sequences_;
        }
        inline cs::Sequence maxSequence() const {
            return maxSequence_;
        }

        bool operator<(const NeighboursSetElemet& other) const {
            return maxSequence_ < other.maxSequence_;
        }

        bool operator>(const NeighboursSetElemet& other) const {
            return !((*this) < other);
        }

        bool operator==(const NeighboursSetElemet& other) const {
            return key_ == other.key_;
        }

        bool operator!=(const NeighboursSetElemet& other) const {
            return !((*this) == other);
        }

        friend std::ostream& operator<<(std::ostream& os, const NeighboursSetElemet& el) {
            os << "seqs:";

            if (el.sequences_.empty()) {
                os << " empty";
            }
            else {
                for (const auto seq : el.sequences_) {
                    os << " " << seq;
                }
            }

            return os;
        }

    private:
        cs::Sequence maxSequence_ = 0;
        cs::PublicKey key_;                  // neighbour public key
        PoolsRequestedSequences sequences_;  // requested sequence
    };

private:
    Transport* transport_;
    BlockChain* blockChain_;

    // flag starting  syncronization
    bool isSyncroStarted_ = false;

    // [key] = sequence,
    // [value] =  packet counter
    // value: increase each new round
    std::map<cs::Sequence, cs::RoundNumber> requestedSequences_;
    std::vector<NeighboursSetElemet> neighbours_;

    cs::Timer timer_;

    friend std::ostream& operator<<(std::ostream&, const PoolSynchronizer::CounterType);
    friend std::ostream& operator<<(std::ostream&, const PoolSynchronizer::SequenceRemovalAccuracy);
};

inline std::ostream& operator<<(std::ostream& os, const PoolSynchronizer::CounterType type) {
    switch (type) {
        case PoolSynchronizer::CounterType::TIMER:
            os << "TIMER";
            break;
    }

    return os;
}

inline std::ostream& operator<<(std::ostream& os, const PoolSynchronizer::SequenceRemovalAccuracy type) {
    switch (type) {
        case PoolSynchronizer::SequenceRemovalAccuracy::EXACT:
            os << "EXACT";
            break;
        case PoolSynchronizer::SequenceRemovalAccuracy::LOWER_BOUND:
            os << "LOWER_BOUND";
            break;
        case PoolSynchronizer::SequenceRemovalAccuracy::UPPER_BOUND:
            os << "UPPER_BOUND";
            break;
    }

    return os;
}
}  // namespace cs
#endif  // POOLSYNCHRONIZER_HPP
