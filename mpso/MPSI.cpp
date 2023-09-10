#include <libOTe/Base/BaseOT.h>
#include <libOTe/TwoChooseOne/SoftSpokenOT/SoftSpokenShOtExt.h>
#include <coproto/Socket/AsioSocket.h>
#include <algorithm>
#include "../common/util.h"
#include "../shuffle/ShareCorrelationGen.h"
#include "../shuffle/MShuffle.h"
#include "MPSI.h"

// WARNING: in the implementation or random OT, we set P1 as sender and other Pi as receivers, which needs to be reversed. But it doesn'a affect the performance.

using namespace oc;

// set is 128-bit elements
std::vector<block> MPSIParty(u32 idx, u32 numParties, u32 numElements, std::vector<block> &set, u32 numThreads, bool fakeBase, bool fakeTriples){
    // shuffle receiver's set
    MShuffleParty shuffleParty(idx, numParties, numElements);
    shuffleParty.getShareCorrelation();

    // std::cout << "P" << idx + 1 << " get sc done" << std::endl;

    Timer timer;
    timer.setTimePoint("start");

    // mpsi connect
    std::vector<Socket> chl(numParties - 1);
    // PORT + senderId * 100 + receiverId
    if (idx == 0){
        for (u32 i = 0; i < numParties - 1; ++i){
            chl[i] = coproto::asioConnect("localhost:" + std::to_string(PORT + i + 1), true);
        } 
    } else if (idx == 1){
        // connect to party{1}
        chl[0] = coproto::asioConnect("localhost:" + std::to_string(PORT + idx), false);
        if (numParties > 2){
            // connect to party{idx + 2}
            chl[idx] = coproto::asioConnect("localhost:" + std::to_string(PORT + idx * 100 + idx + 1), true);
        }
    } else if (idx != numParties - 1){
        // connect to party{1}
        chl[0] = coproto::asioConnect("localhost:" + std::to_string(PORT + idx), false);
        // connect to party{idx}
        chl[idx - 1] = coproto::asioConnect("localhost:" + std::to_string(PORT + (idx - 1) * 100 + idx), false);
        // connect to party{idx + 2}
        chl[idx] = coproto::asioConnect("localhost:" + std::to_string(PORT + idx * 100 + idx + 1), true);
    } else {
        // connect to party{1}
        chl[0] = coproto::asioConnect("localhost:" + std::to_string(PORT + idx), false);
        // connect to party{idx}
        chl[idx - 1] = coproto::asioConnect("localhost:" + std::to_string(PORT + (idx - 1) * 100 + idx), false);
    }
    
    // std::cout << "P" << idx + 1 << " connect done" << std::endl;

    timer.setTimePoint("connect done");
    // std::cout << "P" << idx + 1 << " connect done" << std::endl;

    PRNG prng(sysRandomSeed());
    LowMC cipher(prng.get<u64>());
    std::vector<block> share(numElements, ZeroBlock);
    // mqssPMT, ROT and shuffle
    if (idx != 0){
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

        // construct OKVS(only mqssPMT sender need to do this, i.e. party{2} to party{k})
        Baxos baxos;
        baxos.init(numElements, binSize, w, ssp, volePSI::PaxosParam::GF128, ZeroBlock);
        std::vector<block> okvs(baxos.size());
        PRNG prngBaxos(prng.get<block>());
        baxos.solve<block>(set, ct, okvs, &prngBaxos, 1);
    
        // std::cout << "P" << idx + 1 << " encode done" << std::endl;
        
        // mqssPMT with other parties
        BitVector choice;
        mqssPMTSend(numElements, cipher, choice, okvs, chl[0], numThreads);

        // // output choice for test
        // std::cout << "P" << idx + 1 << " choice: " << choice << std::endl;

        // std::cout << "P" << idx + 1 << " mqssPMT done" << std::endl;

        // random OT extension receiver
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
            coproto::sync_wait(base.send(baseMsg, prngOT, chl[0]));
        }
        // std::cout << "P" << idx + 1 << " baseOT done" << std::endl;

        receiver.setBaseOts(baseMsg);
        // perform random OTs, the results will be written to rMsgs
        AlignedVector<block> rMsgs(numElements);
        coproto::sync_wait(receiver.receive(choice, rMsgs, prngOT, chl[0]));

        // std::cout << "P" << idx + 1 << " random OT done" << std::endl;

        std::copy(rMsgs.begin(), rMsgs.end(), share.begin());

        // mshuffle
        coproto::sync_wait(shuffleParty.run(chl, share));

        // std::cout << "P" << idx + 1 << " mshuffle done" << std::endl;
    }
    // party{1} is always mqssPMT Receiver
    else {
        // mqssPMT with other parties
        std::vector<BitVector> choice(numParties - 1);
        std::vector<std::thread> mqssPMTThrds(numParties - 1);
        for (u32 i = 0; i < numParties - 1; ++i){
            mqssPMTThrds[i] = std::thread([&, i]() {
                mqssPMTRecv(set, cipher, choice[i], chl[i], numThreads);
            });
        }
        for (auto& thrd : mqssPMTThrds) thrd.join();

        // // output choice for test
        // for (u32 i = 0; i < numParties - 1; ++i){
        //     std::cout << "P" << i + 2 << " choice: " << choice[i] << std::endl;
        // }

        // std::cout << "P" << idx + 1 << " mqssPMT done" << std::endl;

        timer.setTimePoint("mqssPMT done");

        // random OT extension
        share = set;
        std::vector<std::thread> otThrds(numParties - 1);
        std::vector<std::vector<block>> sendShare(numParties - 1);
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
                // record update own element's share
                sendShare[i].resize(numElements);
                for (size_t j = 0; j < numElements; j++){
                    sendShare[i][j] = sMsgs[j][1 - choice[i][j]];
                }

                // for (size_t j = 0; j < numElements; j++){
                //     share[j] ^= sMsgs[j][1 - choice[i][j]];
                // }
            });
        }
        for (auto& thrd : otThrds) thrd.join();

        for (size_t i = 0; i < numParties - 1; i++){
            for (size_t j = 0; j < numElements; j++){
                share[j] ^= sendShare[i][j];
            }
        }
        std::vector<std::vector<block>>().swap(sendShare);

        // std::cout << "P" << idx + 1 << " random ot done" << std::endl;

        timer.setTimePoint("random ot done");

        coproto::sync_wait(shuffleParty.run(chl, share));

        // std::cout << "P" << idx + 1 << " mshuffle done" << std::endl;

        timer.setTimePoint("mshuffle done");
    }

    // reconstruct output
    if (idx != 0){
        coproto::sync_wait(chl[0].send(share));
        double comm = 0;
        if (idx == 1){
            comm += chl[0].bytesSent() + chl[0].bytesReceived();
            if (numParties > 2){
                comm += chl[idx].bytesSent() + chl[idx].bytesReceived();
            }
        } else if (idx != numParties - 1){
            comm += chl[0].bytesSent() + chl[0].bytesReceived();
            comm += chl[idx - 1].bytesSent() + chl[idx - 1].bytesReceived();
            comm += chl[idx].bytesSent() + chl[idx].bytesReceived();
        } else {
            comm += chl[0].bytesSent() + chl[0].bytesReceived();
            comm += chl[idx - 1].bytesSent() + chl[idx - 1].bytesReceived();
        }

        std::cout << "P" << idx + 1 << " communication cost = " << comm / 1024 / 1024 << " MB" << std::endl;
        // std::cout << "P" << idx + 1 << " send share done" << std::endl;

        if (idx == 1){
            chl[0].close();
            if (numParties > 2){
                chl[idx].close();
            }
        } else if (idx != numParties - 1){
            chl[0].close();
            chl[idx - 1].close();
            chl[idx].close();
        } else {
            chl[0].close();
            chl[idx - 1].close();
        }

        // std::cout << "P" << idx + 1 << " closes sockets" << std::endl;

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

        // // output share
        // std::cout << "------ share ------" << std::endl;
        // for (size_t i = 0; i < share.size(); i++){
        //     std::cout << share[i] << std::endl;
        // }

        timer.setTimePoint("reconstruct done");

        std::vector<block> setIntersection;

        std::sort(set.begin(), set.end());
        std::sort(share.begin(), share.end());

        std::set_intersection(set.begin(), set.end(), share.begin(), share.end(), std::back_inserter(setIntersection));
        
        timer.setTimePoint("compute intersection and output done");
        
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

        return setIntersection;
    }
    return std::vector<block>();
}


// set is 128-bit elements
u32 MPSICAParty(u32 idx, u32 numParties, u32 numElements, std::vector<block> &set, u32 numThreads, bool fakeBase, bool fakeTriples){
    // shuffle receiver's set
    MShuffleParty shuffleParty(idx, numParties, numElements);
    shuffleParty.getShareCorrelation();

    // std::cout << "P" << idx + 1 << " get sc done" << std::endl;

    Timer timer;
    timer.setTimePoint("start");

    // mpsi connect
    std::vector<Socket> chl(numParties - 1);
    // PORT + senderId * 100 + receiverId
    if (idx == 0){
        for (u32 i = 0; i < numParties - 1; ++i){
            chl[i] = coproto::asioConnect("localhost:" + std::to_string(PORT + i + 1), true);
        } 
    } else if (idx == 1){
        // connect to party{1}
        chl[0] = coproto::asioConnect("localhost:" + std::to_string(PORT + idx), false);
        if (numParties > 2){
            // connect to party{idx + 2}
            chl[idx] = coproto::asioConnect("localhost:" + std::to_string(PORT + idx * 100 + idx + 1), true);
        }
    } else if (idx != numParties - 1){
        // connect to party{1}
        chl[0] = coproto::asioConnect("localhost:" + std::to_string(PORT + idx), false);
        // connect to party{idx}
        chl[idx - 1] = coproto::asioConnect("localhost:" + std::to_string(PORT + (idx - 1) * 100 + idx), false);
        // connect to party{idx + 2}
        chl[idx] = coproto::asioConnect("localhost:" + std::to_string(PORT + idx * 100 + idx + 1), true);
    } else {
        // connect to party{1}
        chl[0] = coproto::asioConnect("localhost:" + std::to_string(PORT + idx), false);
        // connect to party{idx}
        chl[idx - 1] = coproto::asioConnect("localhost:" + std::to_string(PORT + (idx - 1) * 100 + idx), false);
    }

    timer.setTimePoint("connect done");
    // std::cout << "P" << idx + 1 << " connect done" << std::endl;

    PRNG prng(sysRandomSeed());
    LowMC cipher(prng.get<u64>());
    // 64 bits is enough for collision resistance (\lambda + log n)
    // but we use 128-bit block to be consistent with other functions
    std::vector<block> share(numElements, ZeroBlock);
    // party{1} chooses a random string s for identity intersection elements
    block s = prng.get<block>();
    // mqssPMT, ROT and shuffle
    if (idx != 0){
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

        // construct OKVS(only mqssPMT sender need to do this, i.e. party{2} to party{k})
        Baxos baxos;
        baxos.init(numElements, binSize, w, ssp, volePSI::PaxosParam::GF128, ZeroBlock);
        std::vector<block> okvs(baxos.size());
        PRNG prngBaxos(prng.get<block>());
        baxos.solve<block>(set, ct, okvs, &prngBaxos, 1);
    
        // std::cout << "P" << idx + 1 << " encode done" << std::endl;
        
        // mqssPMT with other parties
        BitVector choice;
        mqssPMTSend(numElements, cipher, choice, okvs, chl[0], numThreads);

        // // output choice for test
        // std::cout << "P" << idx + 1 << " choice: " << choice << std::endl;

        // std::cout << "P" << idx + 1 << " mqssPMT done" << std::endl;

        // random OT extension receiver
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
            coproto::sync_wait(base.send(baseMsg, prngOT, chl[0]));
        }
        // std::cout << "P" << idx + 1 << " baseOT done" << std::endl;

        receiver.setBaseOts(baseMsg);
        // perform random OTs, the results will be written to rMsgs
        AlignedVector<block> rMsgs(numElements);
        coproto::sync_wait(receiver.receive(choice, rMsgs, prngOT, chl[0]));

        // std::cout << "P" << idx + 1 << " random OT done" << std::endl;

        std::copy(rMsgs.begin(), rMsgs.end(), share.begin());
        // mshuffle
        coproto::sync_wait(shuffleParty.run(chl, share));

        // std::cout << "P" << idx + 1 << " mshuffle done" << std::endl;
    }
    // party{1} is always mqssPMT Receiver
    else {
        // mqssPMT with other parties
        std::vector<BitVector> choice(numParties - 1);
        std::vector<std::thread> mqssPMTThrds(numParties - 1);
        for (u32 i = 0; i < numParties - 1; ++i){
            mqssPMTThrds[i] = std::thread([&, i]() {
                mqssPMTRecv(set, cipher, choice[i], chl[i], numThreads);
            });
        }
        for (auto& thrd : mqssPMTThrds) thrd.join();

        timer.setTimePoint("mqssPMT done");

        // random OT extension
        share.assign(numElements, s);
        std::vector<std::thread> otThrds(numParties - 1);
        std::vector<std::vector<block>> sendShare(numParties - 1);
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
                // record update own element's share
                sendShare[i].resize(numElements);
                for (size_t j = 0; j < numElements; j++){
                    sendShare[i][j] = sMsgs[j][1 - choice[i][j]];
                }

                // for (size_t j = 0; j < numElements; j++){
                //     share[j] ^= sMsgs[j][1 - choice[i][j]];
                // }
            });
        }
        for (auto& thrd : otThrds) thrd.join();

        for (size_t i = 0; i < numParties - 1; i++){
            for (size_t j = 0; j < numElements; j++){
                share[j] ^= sendShare[i][j];
            }
        }
        std::vector<std::vector<block>>().swap(sendShare);

        // std::cout << "P" << idx + 1 << " random ot done" << std::endl;

        timer.setTimePoint("random ot done");

        coproto::sync_wait(shuffleParty.run(chl, share));

        // std::cout << "P" << idx + 1 << " mshuffle done" << std::endl;

        timer.setTimePoint("mshuffle done");
    }

    // reconstruct output
    if (idx != 0){
        coproto::sync_wait(chl[0].send(share));
        double comm = 0;
        if (idx == 1){
            comm += chl[0].bytesSent() + chl[0].bytesReceived();
            if (numParties > 2){
                comm += chl[idx].bytesSent() + chl[idx].bytesReceived();
            }
        } else if (idx != numParties - 1){
            comm += chl[0].bytesSent() + chl[0].bytesReceived();
            comm += chl[idx - 1].bytesSent() + chl[idx - 1].bytesReceived();
            comm += chl[idx].bytesSent() + chl[idx].bytesReceived();
        } else {
            comm += chl[0].bytesSent() + chl[0].bytesReceived();
            comm += chl[idx - 1].bytesSent() + chl[idx - 1].bytesReceived();
        }

        std::cout << "P" << idx + 1 << " communication cost = " << comm / 1024 / 1024 << " MB" << std::endl;

        if (idx == 1){
            chl[0].close();
            if (numParties > 2){
                chl[idx].close();
            }
        } else if (idx != numParties - 1){
            chl[0].close();
            chl[idx - 1].close();
            chl[idx].close();
        } else {
            chl[0].close();
            chl[idx - 1].close();
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

        // // output share
        // std::cout << "------ share ------" << std::endl;
        // for (size_t i = 0; i < share.size(); i++){
        //     std::cout << share[i] << std::endl;
        // }

        timer.setTimePoint("reconstruct done");

        u32 intersectionSize = 0;

        for (size_t i = 0; i < share.size(); i++){
            intersectionSize += (share[i] == s);
        }
        
        timer.setTimePoint("compute intersection cardinality done");
        
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

        return intersectionSize;
    }
    return 0;
}
