#include "mqssPMT.h"

void mqssPMTSend(u32 numElements, LowMC &cipher, BitVector &out, std::vector<block> &okvs, Socket &chl, u32 numThreads, std::string triplePath){
    // send okvs
    coproto::sync_wait(chl.send(okvs));
    // vodm receiver's input is zeroshare of ciphertext, and SKE's key
    mMatrix<mblock> in;
    in.resize(numElements, 1);
    ssVODMRecv(cipher, in, out, chl, numThreads, triplePath);
}

// we don't need cipher's key, only use round constants
void mqssPMTRecv(std::vector<block> &set, LowMC &cipher, BitVector &out, Socket &chl, u32 numThreads, std::string triplePath){
    // receive okvs
    std::vector<block> okvs;
    coproto::sync_wait(chl.recvResize(okvs));

    std::vector<block> ciphertext(set.size());
    Baxos pax;
    // okvs must have the same seed to decode correctly
    pax.init(set.size(), binSize, w, ssp, volePSI::PaxosParam::GF128, ZeroBlock);
    pax.decode<block>(set, ciphertext, okvs, numThreads);

    mMatrix<mblock> ct;
    ct.resize(set.size(), 1);
    for (u32 i = 0; i < ct.rows(); ++i){
        blockToBitset(ciphertext[i], ct(i, 0));
    }
    // vodm sender's input is ciphertext
    ssVODMSend(cipher, ct, out, chl, numThreads, triplePath);
}