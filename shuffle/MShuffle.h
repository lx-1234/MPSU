#include "../common/util.h"
#include "../common/Defines.h"
#include <vector>

class MShuffleParty{
public:
    MShuffleParty(u32 idx, u32 numParties, u32 numElements){
        mIdx = idx;
        mNumParties = numParties;
        mNumElements = numElements;
        if (idx == 0){
            maprime.resize(numElements);
            mb.resize(numElements);
            mPi.resize(numElements);
        } else if (idx == numParties - 1){
            ma.resize(numElements);
            mdelta.resize(numElements);
            mPi.resize(numElements);
        } else {
            ma.resize(numElements);
            maprime.resize(numElements);
            mb.resize(numElements);
            mPi.resize(numElements);
        }
    }
    ~MShuffleParty() = default;

    // get share correlation
    void getShareCorrelation(std::string fileName = "sc");

    // run multiparty shuffle protocol
    // @param chl: channels with all other parties
    Proto run(std::vector<coproto::Socket> &chl, std::vector<block> &data);

    u32 mIdx, mNumParties, mNumElements;
    std::vector<block> ma, maprime, mb, mdelta;
    std::vector<u32> mPi;
};