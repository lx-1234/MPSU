#pragma once

#include <fstream>
#include <volePSI/GMW/Gmw.h>
#include <coproto/Socket/AsioSocket.h>
#include "../circuit/Circuit.h"
#include "../lowMC/LowMC.h"


void twoPartyTripleGen(u32 myIdx, u32 idx, u32 numElements, u32 numThreads, Socket &chl, std::string fileName);

void tripleGenParty(u32 idx, u32 numParties, u32 numElements, u32 numThreads);