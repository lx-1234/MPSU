#pragma once

#include <volePSI/GMW/Gmw.h>
#include "../circuit/Circuit.h"
#include "../lowMC/lowmcDecGmw.h"
#include <string>


void ssVODMSend(LowMC &cipher, mMatrix<mblock> &in, BitVector &out, Socket &chl, u32 numThreads, std::string triplePath);

// todo: reconstruct the output will get the opposite result, why?
void ssVODMRecv(LowMC &cipher, mMatrix<mblock> &in, BitVector &out, Socket &chl, u32 numThreads, std::string triplePath);