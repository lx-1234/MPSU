#pragma once

#include "LowMC.h"
#include "../common/Defines.h"
#include <string>

void mblockReverse(mblock &x);

// Sender holds ciphertext, cipher could be any key, we only need to know the round constants
void lowMCDecryptGmwSend(LowMC &cipher, mMatrix<mblock> &in, Socket &chl, u32 numThreads, std::string triplePath);

// Receiver holds key
void lowMCDecryptGmwRecv(LowMC &cipher, mMatrix<mblock> &in, Socket &chl, u32 numThreads, std::string triplePath);