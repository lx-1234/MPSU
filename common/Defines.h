#pragma once


#include <cryptoTools/Common/Defines.h>
#include <cryptoTools/Crypto/PRNG.h>
#include <cryptoTools/Common/CLP.h>
#include <cryptoTools/Common/Timer.h>
#include <coproto/coproto.h>
#include <volePSI/Paxos.h>


using u8 = oc::u8;
using u16 = oc::u16;
using u32 = oc::u32;
using u64 = oc::u64;

using CLP = oc::CLP;
using Proto = coproto::task<void>;
using block = oc::block;
using PRNG = oc::PRNG;
using BitVector = oc::BitVector;
using Timer = oc::Timer;

template<typename T>
using mMatrix = oc::Matrix<T>;

using Socket = coproto::Socket;

using oc::sysRandomSeed;
using oc::ZeroBlock;

template<typename T>
using Paxos = volePSI::Paxos<T>;

using Baxos = volePSI::Baxos;

using PaxosParam = volePSI::PaxosParam;

constexpr u16 PORT = 1212;

// okvs parameter
constexpr u64 binSize = 1 << 14;
constexpr u64 w = 3;
constexpr u64 ssp = 40;

// softspokenOT parameter
constexpr u64 fieldBits = 5;
