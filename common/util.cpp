#include "util.h"

void permute(std::vector<u32> &pi, std::vector<block> &data){
    std::vector<block> res(data.size());
    for (size_t i = 0; i < pi.size(); ++i){
        res[i] = data[pi[i]];
    }
    data.assign(res.begin(), res.end());
}

void printPermutation(std::vector<u32> &pi){
    for (size_t i = 0; i < pi.size(); ++i){
        std::cout << pi[i] << " ";
    }
    std::cout << std::endl;
}

void blockToBitset(block &data, std::bitset<128> &out){
    out = 0;
    out ^= data.get<u64>(1);
    out = (out << 64) ^ std::bitset<128>(data.get<u64>(0));
}

void bitsetToBlock(std::bitset<128> &data, block &out){
    out = oc::toBlock(((data >> 64) & std::bitset<128>(0xFFFFFFFFFFFFFFFF)).to_ullong(), 
                       (data & std::bitset<128>(0xFFFFFFFFFFFFFFFF)).to_ullong());
}
