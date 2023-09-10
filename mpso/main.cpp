#include <algorithm>
#include "../circuit/TripleGen.h"
#include "../shuffle/ShareCorrelationGen.h"
#include "MPSU.h"
#include "MPSI.h"


void MPSU_test(u32 idx, u32 numElements, u32 numParties, u32 numThreads, bool fakeBase, bool fakeTriples){
    // generate share correlation
    ShareCorrelation sc(numParties, (numParties - 1) * numElements);
    if (!sc.exist()){
        return;
    }
    std::vector<block> set(numElements);

    // generate set
    for (u32 i = 0; i < numElements; i++){
        set[i] = oc::toBlock(idx + i);
    }

    if (idx == 0){
        std::vector<block> out;
        out = MPSUParty(idx, numParties, numElements, set, numThreads, fakeBase, fakeTriples);
        std::cout << "union size: " << out.size() << std::endl;
    } else {
        MPSUParty(idx, numParties, numElements, set, numThreads, fakeBase, fakeTriples);
    }
}

void MPSUCA_test(u32 idx, u32 numElements, u32 numParties, u32 numThreads, bool fakeBase, bool fakeTriples){
    // generate share correlation
    ShareCorrelation sc(numParties, (numParties - 1) * numElements);
    if (!sc.exist()){
        return;
    }
    std::vector<block> set(numElements);

    // generate set
    for (u32 i = 0; i < numElements; i++){
        set[i] = oc::toBlock(idx + i);
    }

    if (idx == 0){
        u32 out;
        out = MPSUCAParty(idx, numParties, numElements, set, numThreads, fakeBase, fakeTriples);
        std::cout << "union size: " << out << std::endl;
    } else {
        MPSUCAParty(idx, numParties, numElements, set, numThreads, fakeBase, fakeTriples);
    }
}

void MPSI_test(u32 idx, u32 numElements, u32 numParties, u32 numThreads, bool fakeBase, bool fakeTriples){
    // generate share correlation
    ShareCorrelation sc(numParties, numElements);
    if (!sc.exist()){
        return;
    }
    std::vector<block> set(numElements);
    
    // generate set
    for (u32 i = 0; i < numElements; i++){
        set[i] = oc::toBlock(idx + i);
    }

    if (idx == 0){
        std::vector<block> out;
        std::cout << "jump into MPSIParty " << idx + 1 << std::endl;
        out = MPSIParty(idx, numParties, numElements, set, numThreads, fakeBase, fakeTriples);
        std::cout << "intersection size: " << out.size() << std::endl;
    } else {
        std::cout << "jump into MPSIParty " << idx + 1 << std::endl;
        MPSIParty(idx, numParties, numElements, set, numThreads, fakeBase, fakeTriples);
    }
}

void MPSICA_test(u32 idx, u32 numElements, u32 numParties, u32 numThreads, bool fakeBase, bool fakeTriples){
    // generate share correlation
    ShareCorrelation sc(numParties, numElements);
    if (!sc.exist()){
        return;
    }
    std::vector<block> set(numElements);

    // generate set
    for (u32 i = 0; i < numElements; i++){
        set[i] = oc::toBlock(idx + i);
    }


    if (idx == 0){
        u32 out;
        out = MPSICAParty(idx, numParties, numElements, set, numThreads, fakeBase, fakeTriples);
        std::cout << "intersection size: " << out << std::endl;
    } else {
        MPSICAParty(idx, numParties, numElements, set, numThreads, fakeBase, fakeTriples);
    }
}

int main(int agrc, char** argv){
    CLP cmd;
    cmd.parse(agrc, argv);
    u32 n = cmd.getOr("n", 1ull << cmd.getOr("nn", 10));
    u32 k = cmd.getOr("k", 3);
    u32 nt = cmd.getOr("nt", 1);

    bool psuTest = cmd.isSet("psu");
    bool psiTest = cmd.isSet("psi");
    bool psucaTest = cmd.isSet("psuca");
    bool psicaTest = cmd.isSet("psica");
    u32 idx = cmd.getOr("r", -1);

    bool scGen = cmd.isSet("genSC");
    bool tripleGen = cmd.isSet("genTriple");

    bool fakeBase = !(cmd.isSet("genBase"));
    bool fakeTriples = cmd.isSet("fakeTriple");

    bool help = cmd.isSet("h");

    if (help){
        std::cout << "protocols" << std::endl;
        std::cout << "    -psu:         multi-party private set union" << std::endl;
        std::cout << "    -psi:         multi-party private set intersection" << std::endl;
        std::cout << "    -psuca:       multi-party private set union cardinality" << std::endl;
        std::cout << "    -psica:       multi-party private set intersection cardinality" << std::endl;
        std::cout << "parameters" << std::endl;
        std::cout << "    -n:           number of elements in each set, default 1024" << std::endl;
        std::cout << "    -nn:          logarithm of the number of elements in each set, default 10" << std::endl;
        std::cout << "    -k:           number of parties, default 3" << std::endl;
        std::cout << "    -nt:          number of threads, default 1" << std::endl;
        std::cout << "    -r:           index of party" << std::endl;
        std::cout << "    -genSC:       generate share correlation" << std::endl;
        std::cout << "    -genTriple:   generate boolean triples" << std::endl;
        std::cout << "    -genBase:     generate base OTs" << std::endl;
        std::cout << "    -fakeTriple:  use fake boolean triples" << std::endl;
        return 0;
    }    

    if ((idx >= k || idx < 0) && !scGen){
        std::cout << "wrong idx of party, please use -h to print help information" << std::endl;
        return 0;
    }

    if (psuTest){
        if (scGen){
            std::cout << "generate sc begin" << std::endl;
            ShareCorrelation sc(k, (k - 1) * n);
            sc.generate();
            sc.writeToFile();
            sc.release();
            std::cout << "generate sc done" << std::endl;
            return 0;
        } else if (tripleGen){
            tripleGenParty(idx, k, n, nt);
        }
        else MPSU_test(idx, n, k, nt, fakeBase, fakeTriples);
    } else if (psiTest){
        if (scGen){
            std::cout << "generate sc begin" << std::endl;
            ShareCorrelation sc(k, n);
            sc.generate();
            sc.writeToFile();
            sc.release();
            std::cout << "generate sc done" << std::endl;
            return 0;
        }
        else MPSI_test(idx, n, k, nt, fakeBase, fakeTriples);
    } else if (psucaTest){
        if (scGen){
            std::cout << "generate sc begin" << std::endl;
            ShareCorrelation sc(k, (k - 1) * n);
            sc.generate();
            sc.writeToFile();
            sc.release();
            std::cout << "generate sc done" << std::endl;
            return 0;
        }
        else MPSUCA_test(idx, n, k, nt, fakeBase, fakeTriples);
    } else if (psicaTest) {
        if (scGen){
            std::cout << "generate sc begin" << std::endl;
            ShareCorrelation sc(k, n);
            sc.generate();
            sc.writeToFile();
            sc.release();
            std::cout << "generate sc done" << std::endl;
            return 0;
        }
        else MPSICA_test(idx, n, k, nt, fakeBase, fakeTriples);
    } else {
        std::cout << "no protocol chosen, please use -h to print help information" << std::endl;
    }
    return 0;
}