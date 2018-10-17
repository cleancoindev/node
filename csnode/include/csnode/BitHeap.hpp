#ifndef BIT_HEAP_H
#define BIT_HEAP_H

#include <bitset>
#include <limits>
#include <climits>

namespace cs
{
    template<typename T, size_t BitSize = sizeof(T) * CHAR_BIT, typename = std::enable_if<std::is_integral<T>::value>>
    class BitHeap
    {
    public:
        using MinMaxRange = std::pair<T,T>;
    public:
        BitHeap()
            : greatest_(std::numeric_limits<T>::max())
            , isValueSet_(false)
        {}
        void push(T val)
        {
            if (!isValueSet_)
            {
                greatest_ = val;
                isValueSet_ = true;
                return;
            }

            if (val > greatest_)
            {
                T shift = val - greatest_;
                bits_ <<= shift;
                // curr greatest
                size_t ind = shift - 1;
                if (ind < BitSize)
                    bits_.set(ind);
                // new greatest
                greatest_ = val;
            }
            else if (val < greatest_)
            {
                size_t ind = greatest_ - val - 1;
                if (ind < BitSize)
                    bits_.set(ind);
            }
        }

        bool empty() const { return !isValueSet_; }
        MinMaxRange minMaxRange() const { return std::make_pair(greatest_ - BitSize, greatest_); }

        bool contains(T val) const
        {
            if (val > greatest_)
                return false;
            else if (val == greatest_)
                return true;
            else
            {
                size_t ind = greatest_ - val - 1;
                if (ind < BitSize)
                    return bits_.test(ind) == 1;
                else
                    return false;
            }
        }

        size_t count() const
        {
            if (empty())
                return 0;
            else
                return 1 + bits_.count();
        }

    private:
        T greatest_;
        uint8_t isValueSet_;
        std::bitset<BitSize> bits_;
    };

}

#endif