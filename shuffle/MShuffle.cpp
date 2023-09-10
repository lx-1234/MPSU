#include "MShuffle.h"
#include <fstream>

using namespace coproto;

void MShuffleParty::getShareCorrelation(std::string fileName){
    std::string inFileName = "./sc/" + fileName + "_" + std::to_string(mNumParties) + "_" + std::to_string(mNumElements) + "_P" + std::to_string(mIdx + 1);

    std::ifstream inFile;
    inFile.open(inFileName, std::ios::binary | std::ios::in);
    if (!inFile.is_open()){
        std::cout << "opening file failed" << inFileName << "\n";
        return;
    }

    // read permutation for each party
    inFile.read((char*)mPi.data(), mPi.size() * sizeof(u32));

    // read a for party 2 to party k
    if (mIdx != 0){
        inFile.read((char*)ma.data(), ma.size() * sizeof(block));
    }

    // read aprime for party 1 to party k-1
    if (mIdx != mNumParties - 1){
        inFile.read((char*)maprime.data(), maprime.size() * sizeof(block));
    }

    // read b for party 1 to party k-1
    if (mIdx != mNumParties - 1){
        inFile.read((char*)mb.data(), mb.size() * sizeof(block));
    }

    // read delta for party k
    if (mIdx == mNumParties - 1){
        inFile.read((char*)mdelta.data(), mdelta.size() * sizeof(block));
    }
}


Proto MShuffleParty::run(std::vector<Socket> &chl, std::vector<block> &data){
    // if (chl.size() != mNumParties - 1){
    //     std::cout << "number of channels is not correct" << std::endl;
    //     return Proto();
    // }
    MC_BEGIN(
        task<>, &chl, &data, this, i = u32{}, j = u32{}, 
        z = std::vector<std::vector<block>>{}, zprime = std::vector<block>{});

    // test
    // if (mIdx == 0){
    //     for (i = 1; i < mNumParties; ++i){
    //         // send pi to party i + 1
    //         std::cout << "send to P" << i + 1 << std::endl;
    //         str = "Hello, Party" + std::to_string(i + 1) + "!";
    //         MC_AWAIT(chl[i - 1].send(str));
    //     }
    // } else {
    //     // receive aprime from party idx - 1
    //     str.resize(14);
    //     MC_AWAIT(chl[0].recv(str));
    //     std::cout << "P"  << mIdx + 1 << " output:" << str << std::endl;
    // }

    // party{1} receive from all other parties, and send to party{2}
    // chl[i] is the channel with party{i + 2}(i = 0, 1, ..., mNumParties - 2)
    if (mIdx == 0){
        z.resize(mNumParties - 1);
        // todo: maybe parallelize this loop?
        for (i = 0; i < mNumParties - 1; ++i){
            // receive z{i + 1} from party i + 1
            z[i].resize(mNumElements);
            MC_AWAIT(chl[i].recv(z[i]));
        }
        // compute zprime
        zprime.resize(mNumElements, ZeroBlock);
        for (i = 0; i < mNumParties - 1; ++i){
            for (j = 0; j < mNumElements; ++j){
                zprime[j] ^= z[i][j];
            }
        }
        for (i = 0; i < mNumElements; ++i){
            zprime[i] ^= data[i];
        }
        permute(mPi, zprime);
        for (i = 0; i < mNumElements; ++i){
            zprime[i] ^= maprime[i];
        }
        // send zprime to party 2
        MC_AWAIT(chl[0].send(zprime));
        // output b1
        data = mb;

        // output communication cost
        // std::cout << zprime.size() << sizeof(zprime[0]) << std::endl;
        // std::cout << "P1 communication cost: " << chl[0].bytesSent() << std::endl;
    }
    // for 2 <= i <= k - 1, party{i} send to party{1} and party{i + 1},
    // receive from party{i - 1}, chl[i] for party{1, 2, ..., i - 1, i + 1, ..., k}
    else if (mIdx != mNumParties - 1){
        // z{i} = a{i} ^ x{i}, send to party{1}
        z.emplace_back(ma);
        for (i = 0; i < mNumElements; ++i){
            z[0][i] ^= data[i];
        }
        MC_AWAIT(chl[0].send(z[0]));
        // receive zprime{i - 1} from party{i - 1}
        zprime.resize(mNumElements);
        MC_AWAIT(chl[mIdx - 1].recv(zprime));
        // compute zprime{i}
        permute(mPi, zprime);
        for (i = 0; i < mNumElements; ++i){
            zprime[i] ^= maprime[i];
        }
        // send zprime{i} to party{i + 1}
        MC_AWAIT(chl[mIdx].send(zprime));
        // output b{i}
        data = mb;

        // output communication cost
        // std::cout << "P" << mIdx + 1 << " communication cost: " << chl[0].bytesSent() + chl[mIdx].bytesSent() << std::endl;
    }
    // party{k} send to party{1}, receive from party{k - 1}
    // chl[i] for party{i + 1}
    else {
        // z{k} = a{k} ^ x{k}, send to party{1}
        z.emplace_back(ma);
        for (i = 0; i < mNumElements; ++i){
            z[0][i] ^= data[i];
        }
        MC_AWAIT(chl[0].send(z[0]));
        // receive zprime{k - 1} from party{k - 1}
        zprime.resize(mNumElements);
        MC_AWAIT(chl[mIdx - 1].recv(zprime));
        // compute zprime{i}
        permute(mPi, zprime);
        for (i = 0; i < mNumElements; ++i){
            zprime[i] ^= mdelta[i];
        }
        // output zprime{k}
        data = zprime;

        // output communication cost
        // std::cout << "P" << mIdx + 1 << " communication cost: " << chl[0].bytesSent() << std::endl;
    }

    // std::cout << "all done" << std::endl;

    MC_END();
}
