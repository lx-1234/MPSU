// Modified from https://github.com/LowMC/lowmc.git

#ifndef __LowMC_h__
#define __LowMC_h__

#include <bitset>
#include <vector>
#include <string>

const unsigned numofboxes = 10;    // Number of Sboxes
const unsigned blocksize = 128;   // Block size in bits
const unsigned keysize = 128; // Key size in bits
const unsigned rounds = 20; // Number of rounds

const unsigned identitysize = blocksize - 3*numofboxes;
                  // Size of the identity part in the Sbox layer

typedef std::bitset<blocksize> mblock; // Store messages and states
typedef std::bitset<keysize> keyblock;

class LowMC {
public:
    LowMC (keyblock k = 0) {
        key = k;
        instantiate_LowMC();
        keyschedule();   
    };

    void encrypt (mblock &message);
    void decrypt (mblock &message);
    void set_key (keyblock k);

    void print_matrices();
    
// private:
// LowMC private data members //
    // The Sbox and its inverse    
    const std::vector<unsigned> Sbox =
        {0x00, 0x01, 0x03, 0x06, 0x07, 0x04, 0x05, 0x02};
    const std::vector<unsigned> invSbox =
        {0x00, 0x01, 0x07, 0x02, 0x05, 0x06, 0x03, 0x04};
        // Stores the binary matrices for each round
    std::vector<std::vector<mblock>> LinMatrices;
        // Stores the inverses of LinMatrices
    std::vector<std::vector<mblock>> invLinMatrices;
        // Stores the round constants
    std::vector<mblock> roundconstants;
        //Stores the master key
    keyblock key = 0;
        // Stores the matrices that generate the round keys
    std::vector<std::vector<keyblock>> KeyMatrices;
        // Stores the round keys
    std::vector<mblock> roundkeys;
    
// LowMC private functions //
    mblock Substitution (const mblock message);
        // The substitution layer
    mblock invSubstitution (const mblock message);
        // The inverse substitution layer

    mblock MultiplyWithGF2Matrix
        (const std::vector<mblock> matrix, const mblock message);    
        // For the linear layer

    mblock MultiplyWithGF2Matrix_Key
        (const std::vector<keyblock> matrix, const keyblock k);
        // For generating the round keys

    void keyschedule ();
        //Creates the round keys from the master key

    void instantiate_LowMC ();
        //Fills the matrices and roundconstants with pseudorandom bits 
   
// Binary matrix functions //   
    unsigned rank_of_Matrix (const std::vector<mblock> matrix);
    unsigned rank_of_Matrix_Key (const std::vector<keyblock> matrix);
    std::vector<mblock> invert_Matrix (const std::vector<mblock> matrix);

// Random bits functions //
    mblock getrandblock ();
    keyblock getrandkeyblock ();
    bool  getrandbit ();

};

#endif
