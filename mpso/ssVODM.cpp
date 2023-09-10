#include "ssVODM.h"
#include <fstream>

void ssVODMSend(LowMC &cipher, mMatrix<mblock> &in, BitVector &out, Socket &chl, u32 numThreads, std::string triplePath){
    lowMCDecryptGmwSend(cipher, in, chl, numThreads, triplePath);
    u32 numElements = in.rows();
    BetaCircuit cir = lessthanN(numElements);
    volePSI::Gmw gmw;
    gmw.init(numElements, cir, numThreads, 0, sysRandomSeed());
    // reverse
    for (u32 i = 0; i < numElements; ++i){
        mblockReverse(in(i, 0));
    }

    std::vector<block> a, b, c, d;
    u64 numTriples = gmw.mNumOts / 2;
    a.resize(numTriples / 128, ZeroBlock);
    b.resize(numTriples / 128, ZeroBlock);
    c.resize(numTriples / 128, ZeroBlock);
    d.resize(numTriples / 128, ZeroBlock);

    std::ifstream tripleFile;
    if (!triplePath.empty()){
        tripleFile.open(triplePath, std::ios::binary | std::ios::in);
        if (!tripleFile.is_open()){
            std::cout << "Error opening file " << triplePath << ", using fake triples" << std::endl;
        }
    }

    gmw.setInput(0, in);
    if (tripleFile.is_open()){
        BetaCircuit cir = inverse_of_S_box_layer(numofboxes);
        u64 numCostTriples = oc::divCeil(numElements, 128) * 128 * cir.mNonlinearGateCount;
        tripleFile.seekg(numCostTriples / 128 * sizeof(block) * rounds * 4);
        tripleFile.read((char*)a.data(), a.size() * sizeof(block));
        tripleFile.read((char*)b.data(), b.size() * sizeof(block));
        tripleFile.read((char*)c.data(), c.size() * sizeof(block));
        tripleFile.read((char*)d.data(), d.size() * sizeof(block));
    }
    gmw.setTriples(a, b, c, d);
    coproto::sync_wait(gmw.run(chl));

    mMatrix<u8> mOut;
    mOut.resize(numElements, 1);
    gmw.getOutput(0, mOut);

    out.resize(numElements);
    for (u32 i = 0; i < numElements; ++i){
        out[i] = mOut(i, 0) & 1;
    }
}

// todo: reconstruct the output will get the opposite result, why?
void ssVODMRecv(LowMC &cipher, mMatrix<mblock> &in, BitVector &out, Socket &chl, u32 numThreads, std::string triplePath){
    lowMCDecryptGmwRecv(cipher, in, chl, numThreads, triplePath);
    u32 numElements = in.rows();
    BetaCircuit cir = lessthanN(numElements);
    volePSI::Gmw gmw;
    gmw.init(numElements, cir, numThreads, 1, sysRandomSeed());
    // reverse
    for (u32 i = 0; i < numElements; ++i){
        mblockReverse(in(i, 0));
    }

    std::vector<block> a, b, c, d;
    u64 numTriples = gmw.mNumOts / 2;
    a.resize(numTriples / 128, ZeroBlock);
    b.resize(numTriples / 128, ZeroBlock);
    c.resize(numTriples / 128, ZeroBlock);
    d.resize(numTriples / 128, ZeroBlock);

    std::ifstream tripleFile;
    if (!triplePath.empty()){
        tripleFile.open(triplePath, std::ios::binary | std::ios::in);
        if (!tripleFile.is_open()){
            std::cout << "Error opening file " << triplePath << ", using fake triples" << std::endl;
        }
    }

    gmw.setInput(0, in);
    if (tripleFile.is_open()){
        BetaCircuit cir = inverse_of_S_box_layer(numofboxes);
        u64 numCostTriples = oc::divCeil(numElements, 128) * 128 * cir.mNonlinearGateCount;
        tripleFile.seekg(numCostTriples /128 * sizeof(block) * rounds * 4);
        tripleFile.read((char*)a.data(), a.size() * sizeof(block));
        tripleFile.read((char*)b.data(), b.size() * sizeof(block));
        tripleFile.read((char*)c.data(), c.size() * sizeof(block));
        tripleFile.read((char*)d.data(), d.size() * sizeof(block));
    }
    gmw.setTriples(a, b, c, d);

    coproto::sync_wait(gmw.run(chl));

    mMatrix<u8> mOut;
    mOut.resize(numElements, 1);
    gmw.getOutput(0, mOut);

    out.resize(numElements);
    for (u32 i = 0; i < numElements; ++i){
        out[i] = mOut(i, 0) & 1;
    }
    out = ~out;
}