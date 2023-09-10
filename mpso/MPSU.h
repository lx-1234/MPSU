#pragma once

#include <volePSI/Paxos.h>
#include "../common/Defines.h"
#include "mqssPMT.h"


// set is 64-bit elements
std::vector<block> MPSUParty(u32 idx, u32 numParties, u32 numElements, std::vector<block> &set, u32 numThreads, bool fakeBase = true, bool fakeTriples = true);

// set is 128-bit elements
u32 MPSUCAParty(u32 idx, u32 numParties, u32 numElements, std::vector<block> &set, u32 numThreads, bool fakeBase = true, bool fakeTriples = true);