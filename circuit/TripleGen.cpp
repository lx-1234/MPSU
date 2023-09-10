#include "TripleGen.h"


void twoPartyTripleGen(u32 myIdx, u32 idx, u32 numElements, u32 numThreads, Socket &chl, std::string fileName){
    std::string outFileName = "./triple/" + fileName + "_" + std::to_string(numElements) + "_P" + std::to_string(myIdx + 1) + std::to_string(idx + 1);

    std::ofstream outFile;
    outFile.open(outFileName, std::ios::binary | std::ios::out);
    if (!outFile.is_open()){
        std::cout << "Error opening file " << outFileName << std::endl;
        return;
    }

    BetaCircuit cir = inverse_of_S_box_layer(numofboxes);
    // generate triples
    for (u32 r = 0; r < rounds; r++){
        volePSI::Gmw gmw;
        gmw.init(numElements, cir, numThreads, myIdx > idx, sysRandomSeed());
        coproto::sync_wait(gmw.generateTriple(1 << 24, numThreads, chl));
        // write A, B, C, D
        outFile.write((char*)gmw.mA.data(), gmw.mA.size() * sizeof(block));
        outFile.write((char*)gmw.mB.data(), gmw.mB.size() * sizeof(block));
        outFile.write((char*)gmw.mC.data(), gmw.mC.size() * sizeof(block));
        outFile.write((char*)gmw.mD.data(), gmw.mD.size() * sizeof(block));
    }

    BetaCircuit cir2 = lessthanN(numElements);
    volePSI::Gmw gmw;
    gmw.init(numElements, cir2, numThreads, myIdx > idx, sysRandomSeed());
    coproto::sync_wait(gmw.generateTriple(1 << 24, numThreads, chl));
    // write A, B, C, D
    outFile.write((char*)gmw.mA.data(), gmw.mA.size() * sizeof(block));
    outFile.write((char*)gmw.mB.data(), gmw.mB.size() * sizeof(block));
    outFile.write((char*)gmw.mC.data(), gmw.mC.size() * sizeof(block));
    outFile.write((char*)gmw.mD.data(), gmw.mD.size() * sizeof(block));

    outFile.close();
    coproto::sync_wait(chl.flush());
}

void tripleGenParty(u32 idx, u32 numParties, u32 numElements, u32 numThreads){
    Timer timer;
    timer.setTimePoint("begin");
    // connect
    std::vector<Socket> chl;
    for (u32 i = 0; i < numParties; ++i){
        // PORT + senderId * 100 + receiverId
        if (i < idx){
            chl.emplace_back(coproto::asioConnect("localhost:" + std::to_string(PORT + idx * 100 + i), true));
        } else if (i > idx){
            chl.emplace_back(coproto::asioConnect("localhost:" + std::to_string(PORT + i * 100 + idx), false));
        }
    }

    timer.setTimePoint("connect done");

    // generate triples
    std::string fileName = "triple";
    fileName = fileName + "_" + std::to_string(numParties);
    std::vector<std::thread> tripleGenThrds(numParties - 1);
    for (u32 i = 0; i < numParties; ++i){
        if (i < idx){
            tripleGenThrds[i] = std::thread([&, i]() {
                twoPartyTripleGen(idx, i, numElements, numThreads, chl[i], fileName);
            });
        } else if (i > idx){
            tripleGenThrds[i - 1] = std::thread([&, i]() {
                twoPartyTripleGen(idx, i, numElements, numThreads, chl[i - 1], fileName);
            });
        }
    }
    for (auto& thrd : tripleGenThrds) thrd.join();

    timer.setTimePoint("generate triples done");

    // close channel
    for (u32 i = 0; i < chl.size(); ++i){
        chl[i].close();
    }

    if (idx == 0){
        std::cout << timer << std::endl;
    }
}