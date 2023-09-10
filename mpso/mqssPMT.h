#pragma once

#include "ssVODM.h"
#include "../common/Defines.h"
#include "../common/util.h"
#include <string> 

void mqssPMTSend(u32 numElements, LowMC &cipher, BitVector &out, std::vector<block> &okvs, Socket &chl, u32 numThreads, std::string triplePath = "");

// we don't need cipher's key, only use round constants
void mqssPMTRecv(std::vector<block> &set, LowMC &cipher, BitVector &out, Socket &chl, u32 numThreads, std::string triplePath = "");