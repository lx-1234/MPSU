#include <libOTe/Base/BaseOT.h>
#include <libOTe/TwoChooseOne/SoftSpokenOT/SoftSpokenShOtExt.h>
#include <coproto/Socket/AsioSocket.h>
#include "../common/util.h"
#include "../shuffle/ShareCorrelationGen.h"
#include "../shuffle/MShuffle.h"
#include "MPSU.h"


using namespace oc;

// set is 64-bit elements
std::vector<block> MPSUParty(u32 idx, u32 numParties, u32 numElements, std::vector<block> &set, u32 numThreads, bool fakeBase, bool fakeTriples){
    // shuffle k - 1 difference sets
    MShuffleParty shuffleParty(idx, numParties, numElements * (numParties - 1));
    shuffleParty.getShareCorrelation();

    // std::cout << "P" << idx + 1 << " get sc done" << std::endl;

    Timer timer;
    timer.setTimePoint("start");

    // connect
    std::vector<Socket> chl;
    for (u32 i = 0; i < numParties; ++i){
        // PORT + senderId * 100 + receiverId
        if (i < idx){
            chl.emplace_back(coproto::asioConnect("localhost:" + std::to_string(PORT + idx * 100 + i), true));
            // std::cout << "P" << idx + 1 << " connects to P" << i + 1 << " as sender at port " << PORT + idx * 100 + i << std::endl;
        } else if (i > idx){
            chl.emplace_back(coproto::asioConnect("localhost:" + std::to_string(PORT + i * 100 + idx), false));
            // std::cout << "P" << idx + 1 << " connects to P" << i + 1 << " as receiver at port " << PORT + i * 100 + idx << std::endl;
        }
    }
    
    timer.setTimePoint("connect done");
    // std::cout << "P" << idx + 1 << " connect done" << std::endl;

    PRNG prng(sysRandomSeed());
    LowMC cipher(prng.get<u64>());
    oc::RandomOracle hash64(8);
    std::vector<block> share((numParties - 1) * numElements, ZeroBlock);
    // mqssPMT, ROT and shuffle
    if (idx != numParties - 1){
        // lowMC encryption(only mqssPMT sender need to do this, i.e. party{1} to party{k-1})
        std::vector<block> ct(numElements);
        // multithread encryption: enc(0), ..., enc(numElements - 1)
        std::vector<std::thread> encThrds(numThreads);
        const int batch_size = numElements / numThreads;
        for (u32 i = 0; i < numThreads; i++){
            encThrds[i] = std::thread([&, i]() {
                const u32 start = i * batch_size;
                const u32 end = (i == numThreads - 1) ? numElements : start + batch_size;
                mblock tmp;
                for (u32 j = start; j < end; ++j){
                    tmp = j;
                    cipher.encrypt(tmp);
                    bitsetToBlock(tmp, ct[j]);
                }
            });
        }
        for (auto& thrd : encThrds) thrd.join();

        timer.setTimePoint("lowMC encrypt done");

        // std::cout << "P" << idx + 1 << " encrypt done" << std::endl;

        // construct OKVS(only mqssPMT sender need to do this, i.e. party{1} to party{k-1})
        Baxos baxos;
        baxos.init(numElements, binSize, w, ssp, volePSI::PaxosParam::GF128, ZeroBlock);
        std::vector<block> okvs(baxos.size());
        PRNG prngBaxos(prng.get<block>());
        // when using multi threads for okvs, there might be some problems
        baxos.solve<block>(set, ct, okvs, &prngBaxos, 1);

        timer.setTimePoint("construct okvs done");
    
        // std::cout << "P" << idx + 1 << " encode done" << std::endl;
        
        // mqssPMT with other parties
        std::vector<BitVector> choice(numParties - 1);
        std::vector<std::thread> mqssPMTThrds(numParties - 1);
        for (u32 i = 0; i < numParties; ++i){
            std::string triplePath = "";
            if (!fakeTriples && i != idx){
                triplePath = triplePath + "./triple/triple_" + std::to_string(numParties) + "_" + std::to_string(numElements) + "_P" + std::to_string(idx + 1) + std::to_string(i + 1);
            }
            if (i < idx){
                mqssPMTThrds[i] = std::thread([&, i, triplePath]() {
                    mqssPMTRecv(set, cipher, choice[i], chl[i], numThreads, triplePath);
                });
            } else if (i > idx){
                mqssPMTThrds[i - 1] = std::thread([&, i, triplePath]() {
                    mqssPMTSend(numElements, cipher, choice[i - 1], okvs, chl[i - 1], numThreads, triplePath);
                });
            }
        }
        for (auto& thrd : mqssPMTThrds) thrd.join();

        timer.setTimePoint("mqssPMT done");

        // std::cout << "P" << idx + 1 << " mqssPMT done" << std::endl;

        // add MAC to the end of set elements
        // each party initialize his own element's share except party{1}
        u32 offset = (idx - 1) * numElements;
        if (idx != 0){
            std::copy(set.begin(), set.end(), share.begin() + offset);
            for (size_t i = 0; i < set.size(); i++){
                hash64.Update(share[offset + i].mData[0]);
                hash64.Final(share[offset + i].mData[1]);
                hash64.Reset();
            } 
        }

        timer.setTimePoint("compute MAC done");

        // std::cout << "P" << idx + 1 << " compute mac done" << std::endl;

        // std::vector<AlignedVector<block>> sendShare(idx);
        // random OT extension
        std::vector<std::thread> otThrds(numParties - 1);
        for (u32 i = 0; i < numParties; ++i){
            if (i < idx){
                otThrds[i] = std::thread([&, i]() {
                    SoftSpokenShOtSender<> sender;
                    sender.init(fieldBits, true, numThreads);
                    const size_t numBaseOTs = sender.baseOtCount();
                    PRNG prngOT(prng.get<block>());
                    AlignedVector<block> baseMsg;
                    // choice bits for baseOT
                    BitVector baseChoice;
                    if (fakeBase) {
                        baseMsg.resize(numBaseOTs, ZeroBlock);
                        baseChoice.resize(numBaseOTs, 0);
                    } else {
                        // OTE's sender is base OT's receiver
                        DefaultBaseOT base;
                        baseMsg.resize(numBaseOTs);
                        // randomize the base OT's choice bits
                        baseChoice.resize(numBaseOTs);
                        baseChoice.randomize(prngOT);
                        // perform the base ot, call sync_wait to block until they have completed.
                        coproto::sync_wait(base.receive(baseChoice, baseMsg, prngOT, chl[i]));
                    } 
                    sender.setBaseOts(baseMsg, baseChoice);
                    // perform random ots
                    AlignedVector<std::array<block, 2>> sMsgs(numElements);
                    coproto::sync_wait(sender.send(sMsgs, prngOT, chl[i]));
                    // record own element's share, party who enter this branch can't be party{1}
                    for (size_t j = 0; j < numElements; j++){
                        share[offset + j] ^= sMsgs[j][choice[i][j]];
                    }

                    // for (size_t j = 0; j < numElements; j++){
                    //     sendShare[i].emplace_back(sMsgs[j][choice[i][j]]);
                    // }
                });
            } else if (i > idx){
                otThrds[i - 1] = std::thread([&, i]() {
                    SoftSpokenShOtReceiver<> receiver;
                    receiver.init(fieldBits, true, numThreads);
                    const size_t numBaseOTs = receiver.baseOtCount();
                    AlignedVector<std::array<block, 2>> baseMsg(numBaseOTs);
                    PRNG prngOT(prng.get<block>());
                    if (fakeBase) {
                        baseMsg.resize(numBaseOTs);
                        for(auto & blk : baseMsg) {
                            blk[0] = ZeroBlock;
                            blk[1] = ZeroBlock;
                        }
                    } else {
                        // OTE's receiver is base OT's sender
                        DefaultBaseOT base;
                        // perform the base ot, call sync_wait to block until they have completed.
                        coproto::sync_wait(base.send(baseMsg, prngOT, chl[i - 1]));
                    }
                    receiver.setBaseOts(baseMsg);
                    // perform random OTs, the results will be written to rMsgs
                    AlignedVector<block> rMsgs(numElements);
                    coproto::sync_wait(receiver.receive(choice[i - 1], rMsgs, prngOT, chl[i - 1]));
                    // set share
                    std::copy(rMsgs.begin(), rMsgs.end(), share.begin() + (i - 1) * numElements);
                });
            }
        }
        for (auto& thrd : otThrds) thrd.join();

        timer.setTimePoint("random ot done");

        // std::cout << "P" << idx + 1 << " random ot done" << std::endl;

        // update own element's share, party who enter this branch can't be party{1}
        // for (u32 i = 0; i < idx; i++){
        //     for (size_t j = 0; j < numElements; j++){
        //         share[offset + j] ^= sendShare[i][j];
        //     }
        // }


        // mshuffle
        coproto::sync_wait(shuffleParty.run(chl, share));

        timer.setTimePoint("mshuffle done");

        // std::cout << "P" << idx + 1 << " mshuffle done" << std::endl;
    }
    // party{k} is always mqssPMT Receiver

    else {
        // mqssPMT with other parties
        std::vector<BitVector> choice(numParties - 1);
        std::vector<std::thread> mqssPMTThrds(numParties - 1);
        for (u32 i = 0; i < numParties - 1; ++i){
            std::string triplePath = "";
            if (!fakeTriples){
                triplePath = triplePath + "./triple/triple_" + std::to_string(numParties) + "_" + std::to_string(numElements) + "_P" + std::to_string(idx + 1) + std::to_string(i + 1);
            }
            mqssPMTThrds[i] = std::thread([&, i, triplePath]() {
                mqssPMTRecv(set, cipher, choice[i], chl[i], numThreads, triplePath);
            });
        }
        for (auto& thrd : mqssPMTThrds) thrd.join();


        // add MAC to the end of set elements
        // each party initialize his own element's share except party{1}
        const u32 offset = (idx - 1) * numElements;
        std::copy(set.begin(), set.end(), share.begin() + offset);
        for (size_t i = 0; i < set.size(); i++){
            hash64.Update(share[offset + i].mData[0]);
            hash64.Final(share[offset + i].mData[1]);
            hash64.Reset();
        }

        // random OT extension
        // std::vector<AlignedVector<block>> sendShare(idx);
        std::vector<std::thread> otThrds(numParties - 1);
        for (u32 i = 0; i < numParties - 1; ++i){
            otThrds[i] = std::thread([&, i]() {
                SoftSpokenShOtSender<> sender;
                sender.init(fieldBits, true, numThreads);
                const size_t numBaseOTs = sender.baseOtCount();
                PRNG prngOT(prng.get<block>());
                AlignedVector<block> baseMsg;
                // choice bits for baseOT
                BitVector baseChoice;
                if (fakeBase) {
                    baseMsg.resize(numBaseOTs, ZeroBlock);
                    baseChoice.resize(numBaseOTs, 0);
                } else {
                    // OTE's sender is base OT's receiver
                    DefaultBaseOT base;
                    baseMsg.resize(numBaseOTs);
                    // randomize the base OT's choice bits
                    baseChoice.resize(numBaseOTs);
                    baseChoice.randomize(prngOT);
                    // perform the base ot, call sync_wait to block until they have completed.
                    coproto::sync_wait(base.receive(baseChoice, baseMsg, prngOT, chl[i]));
                }
                sender.setBaseOts(baseMsg, baseChoice);
                // perform random ots
                AlignedVector<std::array<block, 2>> sMsgs(numElements);
                coproto::sync_wait(sender.send(sMsgs, prngOT, chl[i]));
                // record own element's share, party who enter this branch can't be party{1}
                for (size_t j = 0; j < numElements; j++){
                    share[offset + j] ^= sMsgs[j][choice[i][j]];
                }

                // for (size_t j = 0; j < numElements; j++){
                //     sendShare[i].emplace_back(sMsgs[j][choice[i][j]]);
                // }
            });
        }
        for (auto& thrd : otThrds) thrd.join();

        // // update own element's share, party who enter this branch can't be party{1}
        // for (u32 i = 0; i < idx; i++){
        //     for (size_t j = 0; j < numElements; j++){
        //         share[offset + j] ^= sendShare[i][j];
        //     }
        // }
        // mshuffle
        coproto::sync_wait(shuffleParty.run(chl, share));
    }

    // reconstruct output
    if (idx != 0){
        coproto::sync_wait(chl[0].send(share));
        double comm = 0;
        for (u32 i = 0; i < chl.size(); ++i){
            comm += chl[i].bytesSent() + chl[i].bytesReceived();
        }

        std::cout << "P" << idx + 1 << " communication cost = " << comm / 1024 / 1024 << " MB" << std::endl;
        // close sockets
        for (u32 i = 0; i < chl.size(); ++i){
            chl[i].close();
        }
        return std::vector<block>();
    }
    // party{1} receive, reconstruct and check MAC
    else {
        // receive shares from other parties
        std::vector<std::vector<block>> recvShares(numParties - 1);
        std::vector<std::thread> recvThrds(numParties - 1);
        for (u32 i = 0; i < numParties - 1; ++i){
            recvThrds[i] = std::thread([&, i]() {
                recvShares[i].resize(share.size());
                coproto::sync_wait(chl[i].recv(recvShares[i]));
            });
        }
        for (auto& thrd : recvThrds) thrd.join();

        timer.setTimePoint("recv share done");

        // std::cout << "P" << idx + 1 << " recv share done" << std::endl;

        // reconstruct
        for (size_t i = 0; i < numParties - 1; i++){
            for (size_t j = 0; j < share.size(); j++){
                share[j] ^= recvShares[i][j];
            }
        }

        timer.setTimePoint("reconstruct done");

        // check MAC
        std::vector<block> setUnion(set);

        // test, output share
        // std::cout << "share = " << std::endl;
        // for (size_t i = 0; i < share.size(); i++){
        //     std::cout << share[i].mData[0] << std::endl;
        // }

        for (size_t i = 0; i < share.size(); i++){
            long long mac;
            hash64.Update(share[i].mData[0]);
            hash64.Final(mac);
            hash64.Reset();
            if (share[i].mData[1] == mac){
                // std::cout << "add element to union: " << share[i].mData[0] << std::endl;
                setUnion.emplace_back(share[i].mData[0]);
            }
        }

        timer.setTimePoint("check MAC and output done");

        double comm = 0;
        for (u32 i = 0; i < chl.size(); ++i){
            comm += chl[i].bytesSent() + chl[i].bytesReceived();
        }

        std::cout << "P1 communication cost = " << comm / 1024 / 1024 << " MB" << std::endl;

        // close sockets
        for (u32 i = 0; i < chl.size(); ++i){
            chl[i].close();
        }
    
        timer.setTimePoint("end");

        std::cout << timer << std::endl;

        return setUnion;
    }
    return std::vector<block>();
}


u32 MPSUCAParty(u32 idx, u32 numParties, u32 numElements, std::vector<block> &set, u32 numThreads, bool fakeBase, bool fakeTriples){
    // shuffle k - 1 difference sets
    MShuffleParty shuffleParty(idx, numParties, numElements * (numParties - 1));
    shuffleParty.getShareCorrelation();

    Timer timer;
    timer.setTimePoint("start");

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

    PRNG prng(sysRandomSeed());
    LowMC cipher(prng.get<u64>());
    oc::RandomOracle hash64(8);
    std::vector<block> share((numParties - 1) * numElements, ZeroBlock);
    // party{1} chooses a random string s for identity union elements
    block s = prng.get<block>();
    if (idx == 0){
        std::vector<std::thread> sThrds(numParties - 1);
        for (u32 i = 0; i < numParties - 1; ++i){
            sThrds[i] = std::thread([&, i]() {
               coproto::sync_wait(chl[i].send(s));
            });
        }
        for (auto& thrd : sThrds) thrd.join();
    } else {
        coproto::sync_wait(chl[0].recv(s));
    }
    // mqssPMT, ROT and shuffle
    if (idx != numParties - 1){
        // lowMC encryption(only mqssPMT sender need to do this, i.e. party{1} to party{k-1})
        std::vector<block> ct(numElements);
        // multithread encryption: enc(0), ..., enc(numElements - 1)
        std::vector<std::thread> encThrds(numThreads);
        const int batch_size = numElements / numThreads;
        for (u32 i = 0; i < numThreads; i++){
            encThrds[i] = std::thread([&, i]() {
                const u32 start = i * batch_size;
                const u32 end = (i == numThreads - 1) ? numElements : start + batch_size;
                mblock tmp;
                for (u32 j = start; j < end; ++j){
                    tmp = j;
                    cipher.encrypt(tmp);
                    bitsetToBlock(tmp, ct[j]);
                }
            });
        }
        for (auto& thrd : encThrds) thrd.join();

        timer.setTimePoint("lowMC encrypt done");

        // std::cout << "P" << idx + 1 << " encrypt done" << std::endl;

        // construct OKVS(only mqssPMT sender need to do this, i.e. party{1} to party{k-1})
        Baxos baxos;
        baxos.init(numElements, binSize, w, ssp, volePSI::PaxosParam::GF128, ZeroBlock);
        std::vector<block> okvs(baxos.size());
        PRNG prngBaxos(prng.get<block>());
        baxos.solve<block>(set, ct, okvs, &prngBaxos, 1);

        timer.setTimePoint("construct okvs done");
    
        // std::cout << "P" << idx + 1 << " encode done" << std::endl;
        
        // mqssPMT with other parties
        std::vector<BitVector> choice(numParties - 1);
        std::vector<std::thread> mqssPMTThrds(numParties - 1);
        for (u32 i = 0; i < numParties; ++i){
            std::string triplePath = "";
            if (!fakeTriples){
                triplePath = triplePath + "./triple/triple_" + std::to_string(numParties) + "_" + std::to_string(numElements) + "_P" + std::to_string(idx + 1) + std::to_string(i + 1);
            }
            if (i < idx){
                mqssPMTThrds[i] = std::thread([&, i, triplePath]() {
                    mqssPMTRecv(set, cipher, choice[i], chl[i], numThreads, triplePath);
                });
            } else if (i > idx){
                mqssPMTThrds[i - 1] = std::thread([&, i, triplePath]() {
                    mqssPMTSend(numElements, cipher, choice[i - 1], okvs, chl[i - 1], numThreads, triplePath);
                });
            }
        }
        for (auto& thrd : mqssPMTThrds) thrd.join();

        timer.setTimePoint("mqssPMT done");

        // std::cout << "P" << idx + 1 << " mqssPMT done" << std::endl;

        // each party share s
        u32 offset = (idx - 1) * numElements;
        if (idx != 0){
            std::fill(share.begin() + offset, share.begin() + offset + numElements, s);
        }


        // random OT extension
        std::vector<std::thread> otThrds(numParties - 1);
        for (u32 i = 0; i < numParties; ++i){
            if (i < idx){
                otThrds[i] = std::thread([&, i]() {
                    SoftSpokenShOtSender<> sender;
                    sender.init(fieldBits, true, numThreads);
                    const size_t numBaseOTs = sender.baseOtCount();
                    PRNG prngOT(prng.get<block>());
                    AlignedVector<block> baseMsg;
                    // choice bits for baseOT
                    BitVector baseChoice;
                    if (fakeBase) {
                        baseMsg.resize(numBaseOTs, ZeroBlock);
                        baseChoice.resize(numBaseOTs, 0);
                    } else {
                        // OTE's sender is base OT's receiver
                        DefaultBaseOT base;
                        baseMsg.resize(numBaseOTs);
                        // randomize the base OT's choice bits
                        baseChoice.resize(numBaseOTs);
                        baseChoice.randomize(prngOT);
                        // perform the base ot, call sync_wait to block until they have completed.
                        coproto::sync_wait(base.receive(baseChoice, baseMsg, prngOT, chl[i]));
                    }
                    sender.setBaseOts(baseMsg, baseChoice);
                    // perform random ots
                    AlignedVector<std::array<block, 2>> sMsgs(numElements);
                    coproto::sync_wait(sender.send(sMsgs, prngOT, chl[i]));
                    // record own element's share, party who enter this branch can't be party{1}
                    for (size_t j = 0; j < numElements; j++){
                        share[offset + j] ^= sMsgs[j][choice[i][j]];
                    }
                });
            } else if (i > idx){
                otThrds[i - 1] = std::thread([&, i]() {
                    SoftSpokenShOtReceiver<> receiver;
                    receiver.init(fieldBits, true, numThreads);
                    const size_t numBaseOTs = receiver.baseOtCount();
                    AlignedVector<std::array<block, 2>> baseMsg(numBaseOTs);
                    PRNG prngOT(prng.get<block>());
                    if (fakeBase) {
                        baseMsg.resize(numBaseOTs);
                        for(auto & blk : baseMsg) {
                            blk[0] = ZeroBlock;
                            blk[1] = ZeroBlock;
                        }
                    } else {
                        // OTE's receiver is base OT's sender
                        DefaultBaseOT base;
                        // perform the base ot, call sync_wait to block until they have completed.
                        coproto::sync_wait(base.send(baseMsg, prngOT, chl[i - 1]));
                    }
                    receiver.setBaseOts(baseMsg);
                    // perform random OTs, the results will be written to rMsgs
                    AlignedVector<block> rMsgs(numElements);
                    coproto::sync_wait(receiver.receive(choice[i - 1], rMsgs, prngOT, chl[i - 1]));
                    // set share
                    std::copy(rMsgs.begin(), rMsgs.end(), share.begin() + (i - 1) * numElements);
                });
            }
        }
        for (auto& thrd : otThrds) thrd.join();

        timer.setTimePoint("random ot done");

        // mshuffle
        coproto::sync_wait(shuffleParty.run(chl, share));

        timer.setTimePoint("mshuffle done");
    }
    // party{k} is always mqssPMT Receiver
    else {
        // mqssPMT with other parties
        std::vector<BitVector> choice(numParties - 1);
        std::vector<std::thread> mqssPMTThrds(numParties - 1);
        for (u32 i = 0; i < numParties - 1; ++i){
            std::string triplePath;
            if (!fakeTriples){
                triplePath = triplePath + "./triple/triple_" + std::to_string(numParties) + "_" + std::to_string(numElements) + "_P" + std::to_string(idx + 1) + std::to_string(i + 1);
            }   
            mqssPMTThrds[i] = std::thread([&, i, triplePath]() {
                mqssPMTRecv(set, cipher, choice[i], chl[i], numThreads, triplePath);
            });
        }
        for (auto& thrd : mqssPMTThrds) thrd.join();


        // add MAC to the end of set elements
        // each party initialize his own element's share except party{1}
        const u32 offset = (idx - 1) * numElements;
        std::fill(share.begin() + offset, share.begin() + offset + numElements, s);

        // random OT extension
        std::vector<std::thread> otThrds(numParties - 1);
        for (u32 i = 0; i < numParties - 1; ++i){
            otThrds[i] = std::thread([&, i]() {
                SoftSpokenShOtSender<> sender;
                sender.init(fieldBits, true, numThreads);
                const size_t numBaseOTs = sender.baseOtCount();
                PRNG prngOT(prng.get<block>());
                AlignedVector<block> baseMsg;
                // choice bits for baseOT
                BitVector baseChoice;
                if (fakeBase) {
                    baseMsg.resize(numBaseOTs, ZeroBlock);
                    baseChoice.resize(numBaseOTs, 0);
                } else {
                    // OTE's sender is base OT's receiver
                    DefaultBaseOT base;
                    baseMsg.resize(numBaseOTs);
                    // randomize the base OT's choice bits
                    baseChoice.resize(numBaseOTs);
                    baseChoice.randomize(prngOT);
                    // perform the base ot, call sync_wait to block until they have completed.
                    coproto::sync_wait(base.receive(baseChoice, baseMsg, prngOT, chl[i]));
                }
                sender.setBaseOts(baseMsg, baseChoice);
                // perform random ots
                AlignedVector<std::array<block, 2>> sMsgs(numElements);
                coproto::sync_wait(sender.send(sMsgs, prngOT, chl[i]));
                // record own element's share, party who enter this branch can't be party{1}
                for (size_t j = 0; j < numElements; j++){
                    share[offset + j] ^= sMsgs[j][choice[i][j]];
                }
            });
        }
        for (auto& thrd : otThrds) thrd.join();

        // mshuffle
        coproto::sync_wait(shuffleParty.run(chl, share));
    }

    // reconstruct output
    if (idx != 0){
        coproto::sync_wait(chl[0].send(share));
        double comm = 0;
        for (u32 i = 0; i < chl.size(); ++i){
            comm += chl[i].bytesSent() + chl[i].bytesReceived();
        }

        std::cout << "P" << idx + 1 << " communication cost = " << comm / 1024 / 1024 << " MB" << std::endl;
        // close sockets
        for (u32 i = 0; i < chl.size(); ++i){
            chl[i].close();
        }
        return 0;
    }
    // party{1} receive, reconstruct and check MAC
    else {
        // receive shares from other parties
        std::vector<std::vector<block>> recvShares(numParties - 1);
        std::vector<std::thread> recvThrds(numParties - 1);
        for (u32 i = 0; i < numParties - 1; ++i){
            recvThrds[i] = std::thread([&, i]() {
                recvShares[i].resize(share.size());
                coproto::sync_wait(chl[i].recv(recvShares[i]));
            });
        }
        for (auto& thrd : recvThrds) thrd.join();

        timer.setTimePoint("recv share done");

        // std::cout << "P" << idx + 1 << " recv share done" << std::endl;

        // reconstruct
        for (size_t i = 0; i < numParties - 1; i++){
            for (size_t j = 0; j < share.size(); j++){
                share[j] ^= recvShares[i][j];
            }
        }

        timer.setTimePoint("reconstruct done");

        // check
        u32 unionSize = set.size();

        // test, output share
        // std::cout << "share = " << std::endl;
        // for (size_t i = 0; i < share.size(); i++){
        //     std::cout << share[i].mData[0] << std::endl;
        // }

        for (size_t i = 0; i < share.size(); i++){
            unionSize += (share[i] == s);
        }

        timer.setTimePoint("compute union size done");

        double comm = 0;
        for (u32 i = 0; i < chl.size(); ++i){
            comm += chl[i].bytesSent() + chl[i].bytesReceived();
        }

        std::cout << "P1 communication cost = " << comm / 1024 / 1024 << " MB" << std::endl;

        // close sockets
        for (u32 i = 0; i < chl.size(); ++i){
            chl[i].close();
        }
    
        timer.setTimePoint("end");

        std::cout << timer << std::endl;

        return unionSize;
    }
    return 0;
}