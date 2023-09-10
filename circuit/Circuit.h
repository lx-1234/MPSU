#pragma once

#include <cryptoTools/Circuit/BetaCircuit.h>
#include <cryptoTools/Circuit/Gate.h>
#include "../common/Defines.h"


using BetaCircuit = oc::BetaCircuit;
using BetaBundle = oc::BetaBundle;
using GateType = oc::GateType;

BetaCircuit inverse_of_S_box_layer(u8 num_S_box);
// BetaCircuit inverse_of_linear_layer(u8** L);
// BetaCircuit inverse_of_constant_addition_layer(u8* C);
// BetaCircuit inverse_of_key_addition_layer(u8* K);

// n is power of 2, should evaluate on a 64-bit input
BetaCircuit lessthanN(u32 n);
