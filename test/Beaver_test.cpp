#include "../common/Defines.h"
#include "../common/util.h"
#include "../circuit/Circuit.h"
#include "../lowMC/LowMC.h"
#include <volePSI/GMW/SilentTripleGen.h>
#include <coproto/Socket/AsioSocket.h>

using volePSI::Mode;

void Beaver_2party_test(CLP &cmd){
    u32 numElements = cmd.getOr("n", 1ull << cmd.getOr("nn", 10));
    u32 numThreads = cmd.getOr("nt", 1);
    u32 idx = cmd.getOr("r", 0);

    if (idx != 0 && idx != 1) {
        std::cout << "idx must be 0 or 1" << std::endl;
        return;
    }

    Socket chl;
    chl = coproto::asioConnect("localhost:" + std::to_string(PORT + 101), idx);

    BetaCircuit sboxCircuit = inverse_of_S_box_layer(numofboxes);
    BetaCircuit compCircuit = lessthanN(numElements);
    u64 numTriplesSbox = oc::divCeil(numElements * rounds, 128) * 128 * sboxCircuit.mNonlinearGateCount;
    u64 numTriplesComp = oc::divCeil(numElements, 128) * 128 * compCircuit.mNonlinearGateCount;
    u64 numTriples = numTriplesSbox + numTriplesComp;

    u64 bs = 1 << 24;

    Timer timer;

    timer.setTimePoint("start");

    volePSI::SilentTripleGen t;
    PRNG prng(sysRandomSeed());

    t.init(numTriples * 2, bs, numThreads, idx ? Mode::Receiver : Mode::Sender, prng.get<block>());

    timer.setTimePoint("init");

    coproto::sync_wait(t.generateBaseOts(idx, prng, chl));
    
    double commBase = chl.bytesSent() + chl.bytesReceived();

    timer.setTimePoint("generate baseOT done");

    coproto::sync_wait(t.expand(chl));

    timer.setTimePoint("expand done");

    if (idx == 0){
        std::cout << timer << std::endl;
    }
    std::cout << "P" << idx + 1 << " comm cost  = " << chl.bytesSent() * 1.0 / 1024 / 1024 << " MB" << std::endl;

    coproto::sync_wait(chl.flush());
    chl.close();
    
    if (idx == 0){
        std::cout << "numTriplesSbox = " << numTriplesSbox << std::endl;
        std::cout << "numTriplesComp = " << numTriplesComp << std::endl;
    }
}

int main(int argc, char** argv){
    CLP cmd;
    cmd.parse(argc, argv);
    Beaver_2party_test(cmd);
    return 0;
}