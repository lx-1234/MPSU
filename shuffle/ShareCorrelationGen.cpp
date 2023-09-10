#include <algorithm>
#include <random>
#include <cryptoTools/Common/Timer.h>
#include <cryptoTools/Crypto/PRNG.h>
#include <fstream>
#include "ShareCorrelationGen.h"


bool ShareCorrelation::exist(std::string fileName){
    std::vector<std::string> inFileName(mNumParties);
    for (u32 i = 1; i <= mNumParties; i++){
        inFileName[i - 1] = "./sc/" + fileName + "_" + std::to_string(mNumParties) + "_" + std::to_string(mNumElements) + "_P" + std::to_string(i);
    }

    std::vector<std::ofstream> inFile(mNumParties);
    for (u32 i = 0; i < mNumParties; ++i){
        inFile[i].open(inFileName[i], std::ios::binary | std::ios::in);
        if (!inFile[i].is_open()){
            std::cout << "Error opening file " << inFileName[i] << std::endl;
            return false;
        }
    }
    // close file
    for (u32 i = 0; i < mNumParties; ++i){
        inFile[i].close();
    }
    return true;
}

// to do: realize from share translation
// when numParties and numElements grow, memory is not enough
void ShareCorrelation::generate(){
    mPi.resize(mNumParties);
    ma.resize(mNumParties - 1);
    maprime.resize(mNumParties - 1);
    mb.resize(mNumParties - 1);
    mdelta.resize(mNumElements);

    for (size_t i = 0; i < mNumParties; ++i){
        mPi[i].resize(mNumElements);
        for (size_t j = 0; j < mNumElements; ++j){
            mPi[i][j] = j;
        }
    }

    PRNG prng(sysRandomSeed());
        
    // generate random permutation
    for (size_t i = 0; i < mNumParties; ++i){
        u32 r = prng.get<u32>();
        std::shuffle(std::begin(mPi[i]), std::end(mPi[i]), std::default_random_engine(r));
    }

    for (size_t i = 0; i < mNumParties - 1; ++i){
        ma[i].resize(mNumElements);
        maprime[i].resize(mNumElements);
        mb[i].resize(mNumElements);
        prng.get<block>(ma[i].data(), ma[i].size());
        prng.get<block>(maprime[i].data(), maprime[i].size());
        prng.get<block>(mb[i].data(), mb[i].size());
    }

    // compute delta
    mdelta.assign(mNumElements, ZeroBlock);
    for (size_t i = 0; i < mNumParties - 1; ++i){
        for (size_t j = 0; j < mNumElements; ++j){
            mdelta[j] ^= ma[i][j];
        }
    }
    for (size_t i = 0; i < mNumParties - 1; ++i){
        permute(mPi[i], mdelta);
        for (size_t j = 0; j < mNumElements; ++j){
            mdelta[j] ^= maprime[i][j];
        }
    }
    permute(mPi[mNumParties - 1], mdelta);
    for (size_t i = 0; i < mNumParties - 1; ++i){
        for (size_t j = 0; j < mNumElements; ++j){
            mdelta[j] ^= mb[i][j];
        }
    }
}

void ShareCorrelation::writeToFile(std::string fileName){
    std::vector<std::string> outFileName(mNumParties);
    for (size_t i = 1; i <= mNumParties; i++){
        outFileName[i - 1] = "./sc/" + fileName + "_" + std::to_string(mNumParties) + "_" + std::to_string(mNumElements) + "_P" + std::to_string(i);
    }

    std::vector<std::ofstream> outFile(mNumParties);
    for (size_t i = 0; i < mNumParties; ++i){
        outFile[i].open(outFileName[i], std::ios::binary | std::ios::out);
        if (!outFile[i].is_open())
            std::cout << "Error opening file " << outFileName[i] << std::endl;
    }

    // write pi for party 1 to party k
    for (size_t i = 0; i < mNumParties; ++i){
        outFile[i].write((char*)mPi[i].data(), mPi[i].size() * sizeof(u32));
    }

    // write a for party 2 to party k
    for (size_t i = 0; i < mNumParties - 1; ++i){
        outFile[i + 1].write((char*)ma[i].data(), ma[i].size() * sizeof(block));
    }

    // write aprime for party 1 to party k-1
    for (size_t i = 0; i < mNumParties - 1; ++i){
        outFile[i].write((char*)maprime[i].data(), maprime[i].size() * sizeof(block));
    }

    // write b for party 1 to party k-1
    for (size_t i = 0; i < mNumParties - 1; ++i){
        outFile[i].write((char*)mb[i].data(), mb[i].size() * sizeof(block));
    }

    // write delta for party k
    outFile[mNumParties - 1].write((char*)mdelta.data(), mdelta.size() * sizeof(block));

    // close file
    for (size_t i = 0; i < mNumParties; ++i){
        outFile[i].close();
    }
}

void ShareCorrelation::release(){
    std::vector<std::vector<u32>>().swap(mPi);
    std::vector<std::vector<block>>().swap(ma);
    std::vector<std::vector<block>>().swap(maprime);
    std::vector<std::vector<block>>().swap(mb);
    std::vector<block>().swap(mdelta);
}