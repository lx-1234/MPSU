#pragma once
// This file and the associated implementation has been placed in the public domain, waiving all copyright. No restrictions are placed on its use.


#include <cryptoTools/Common/CLP.h>
#include <iostream>
#include <bitset>
#include "Defines.h"

// permute data according to pi
void permute(std::vector<u32> &pi, std::vector<block> &data);

void printPermutation(std::vector<u32> &pi);

void blockToBitset(block &data, std::bitset<128> &out);

void bitsetToBlock(std::bitset<128> &data, block &out);