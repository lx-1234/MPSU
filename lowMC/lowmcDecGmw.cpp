#include <volePSI/GMW/Gmw.h>
#include <coproto/Socket/AsioSocket.h>
#include "lowmcDecGmw.h"
#include "../circuit/Circuit.h"
#include <fstream>

void mblockReverse(mblock &x){
    u64 l = (x & mblock(0xFFFFFFFFFFFFFFFF)).to_ullong();
    u64 h = (x >> 64).to_ullong();
    // reverse h and l
    l = ((l >> 1) & 0x5555555555555555) | ((l & 0x5555555555555555) << 1);
    l = ((l >> 2) & 0x3333333333333333) | ((l & 0x3333333333333333) << 2);
    l = ((l >> 4) & 0x0F0F0F0F0F0F0F0F) | ((l & 0x0F0F0F0F0F0F0F0F) << 4);
    l = ((l >> 8) & 0x00FF00FF00FF00FF) | ((l & 0x00FF00FF00FF00FF) << 8);
    l = ((l >> 16) & 0x0000FFFF0000FFFF) | ((l & 0x0000FFFF0000FFFF) << 16);
    l = (l >> 32) | (l << 32);

    h = ((h >> 1) & 0x5555555555555555) | ((h & 0x5555555555555555) << 1);
    h = ((h >> 2) & 0x3333333333333333) | ((h & 0x3333333333333333) << 2);
    h = ((h >> 4) & 0x0F0F0F0F0F0F0F0F) | ((h & 0x0F0F0F0F0F0F0F0F) << 4);
    h = ((h >> 8) & 0x00FF00FF00FF00FF) | ((h & 0x00FF00FF00FF00FF) << 8);
    h = ((h >> 16) & 0x0000FFFF0000FFFF) | ((h & 0x0000FFFF0000FFFF) << 16);
    h = (h >> 32) | (h << 32);
    x = (mblock(l) << 64) | mblock(h);
}

// Sender holds ciphertext, cipher could be any key, we only need to know the round constants
void lowMCDecryptGmwSend(LowMC &cipher, mMatrix<mblock> &in, Socket &chl, u32 numThreads, std::string triplePath){
    BetaCircuit cir = inverse_of_S_box_layer(numofboxes);
    u32 numElements = in.rows();
    // fake triples
    std::vector<block> a, b, c, d;
    u64 numTriples = oc::divCeil(numElements, 128) * 128 * cir.mNonlinearGateCount;
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
    for (u16 r = rounds; r > 0; --r) {
        for (u32 i = 0; i < numElements; ++i){
            // only one party need to do this
            in(i, 0) ^= cipher.roundconstants[r-1];
            // each party need to do this
            in(i, 0) =  cipher.MultiplyWithGF2Matrix(cipher.invLinMatrices[r-1], in(i, 0));
            // in(i, 0) =  cipher.invSubstitution(in(i, 0));
            mblockReverse(in(i, 0));
        }
    // only support single thread
    volePSI::Gmw gmw;
    gmw.init(numElements, cir, numThreads, 0, sysRandomSeed());
        gmw.setInput(0, in);
        if (tripleFile.is_open()){
            tripleFile.read((char*)a.data(), a.size() * sizeof(block));
            tripleFile.read((char*)b.data(), b.size() * sizeof(block));
            tripleFile.read((char*)c.data(), c.size() * sizeof(block));
            tripleFile.read((char*)d.data(), d.size() * sizeof(block));
        }
        gmw.setTriples(a, b, c, d);
        coproto::sync_wait(gmw.run(chl));
        // coproto::sync_wait(chl.flush());
        gmw.getOutput(0, in);
        for (u32 i = 0; i < numElements; ++i){
            mblockReverse(in(i, 0));
        }
    }

    tripleFile.close();
}

// Receiver holds key
void lowMCDecryptGmwRecv(LowMC &cipher, mMatrix<mblock> &in, Socket &chl, u32 numThreads, std::string triplePath){
    BetaCircuit cir = inverse_of_S_box_layer(numofboxes);
    u32 numElements = in.rows();
    // fake triples
    std::vector<block> a, b, c, d;
    u64 numTriples = oc::divCeil(numElements, 128) * 128 * cir.mNonlinearGateCount;
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
    for (u16 r = rounds; r > 0; --r) {
        for (u32 i = 0; i < numElements; ++i){
            // only receiver need to do this
            in(i, 0) ^= cipher.roundkeys[r];
            in(i, 0) =  cipher.MultiplyWithGF2Matrix(cipher.invLinMatrices[r-1], in(i, 0));
            // reverse to satisfy the gmw in volePSI
            mblockReverse(in(i, 0));
        }
    // only support single thread
    volePSI::Gmw gmw;
    gmw.init(numElements, cir, numThreads, 1, sysRandomSeed());
        gmw.setInput(0, in);
        if (tripleFile.is_open()){
            tripleFile.read((char*)a.data(), a.size() * sizeof(block));
            tripleFile.read((char*)b.data(), b.size() * sizeof(block));
            tripleFile.read((char*)c.data(), c.size() * sizeof(block));
            tripleFile.read((char*)d.data(), d.size() * sizeof(block));
        }
        gmw.setTriples(a, b, c, d);
        coproto::sync_wait(gmw.run(chl));
        gmw.getOutput(0, in);
        // todo
        // gmw.setTriples(a, b, c, d);
        for (u32 i = 0; i < numElements; ++i){
            mblockReverse(in(i, 0));
        }
    }
    for (u32 i = 0; i < numElements; ++i){
        in(i, 0) ^= cipher.roundkeys[0];
    }

    tripleFile.close();
}
