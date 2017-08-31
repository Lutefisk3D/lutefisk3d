#pragma once
#include <stdint.h>

template<class Data,int IndexBits,int GenerationBits>
struct DataHandle
{
    constexpr DataHandle(uint64_t idx = invalidIdx(), uint64_t gen = 0, uint64_t ex = 0)
        : index(idx), generation(gen), extra_bits(ex)
    {
    }
    uint64_t index : IndexBits;
    uint64_t generation : GenerationBits;
    uint64_t extra_bits : sizeof(int64_t)*8 - GenerationBits - IndexBits;
    static_assert(IndexBits+GenerationBits < 8*sizeof(uint64_t), "Not enough bits available in uint64_t");
    static constexpr uint64_t invalidIdx() { return (1<<IndexBits)-1; }
    constexpr bool valid() const { return index!=invalidIdx(); }
    constexpr uint64_t value() const {
        return (index) | generation<<IndexBits | extra_bits << (IndexBits+GenerationBits);
    }
};
