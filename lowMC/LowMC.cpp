// Modified from https://github.com/LowMC/lowmc.git

#include <vector>
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <algorithm>

#include "parameter.h"
#include "LowMC.h"


/////////////////////////////
//     LowMC functions     //
/////////////////////////////

void LowMC::encrypt (mblock &message) {
    message ^= roundkeys[0];
    for (unsigned r = 1; r <= rounds; ++r) {
        message =  Substitution(message);
        message =  MultiplyWithGF2Matrix(LinMatrices[r-1], message);
        message ^= roundconstants[r-1];
        message ^= roundkeys[r];
    }
    return;
}


void LowMC::decrypt (mblock &message) {
    for (unsigned r = rounds; r > 0; --r) {
        message ^= roundkeys[r];
        message ^= roundconstants[r-1];
        message =  MultiplyWithGF2Matrix(invLinMatrices[r-1], message);
        message =  invSubstitution(message);
    }
    message ^= roundkeys[0];
    return;
}


void LowMC::set_key (keyblock k) {
    key = k;
    keyschedule();
}


void LowMC::print_matrices() {
    std::cout << "LowMC matrices and constants" << std::endl;
    std::cout << "============================" << std::endl;
    std::cout << "Block size: " << blocksize << std::endl;
    std::cout << "Key size: " << keysize << std::endl;
    std::cout << "Rounds: " << rounds << std::endl;
    std::cout << std::endl;

    std::cout << "Linear layer matrices" << std::endl;
    std::cout << "---------------------" << std::endl;
    for (unsigned r = 1; r <= rounds; ++r) {
        std::cout << "Linear layer " << r << ":" << std::endl;
        for (auto row: LinMatrices[r-1]) {
            std::cout << "[";
            for (unsigned i = 0; i < blocksize; ++i) {
                std::cout << row[i];
                if (i != blocksize - 1) {
                    std::cout << ", ";
                }
            }
            std::cout << "]" << std::endl;
        }
        std::cout << std::endl;
    }

    std::cout << "Round constants" << std::endl;
    std::cout << "---------------------" << std::endl;
    for (unsigned r = 1; r <= rounds; ++r) {
        std::cout << "Round constant " << r << ":" << std::endl;
        std::cout << "[";
        for (unsigned i = 0; i < blocksize; ++i) {
            std::cout << roundconstants[r-1][i];
            if (i != blocksize - 1) {
                std::cout << ", ";
            }
        }
        std::cout << "]" << std::endl;
        std::cout << std::endl;
    }
    
    std::cout << "Round key matrices" << std::endl;
    std::cout << "---------------------" << std::endl;
    for (unsigned r = 0; r <= rounds; ++r) {
        std::cout << "Round key matrix " << r << ":" << std::endl;
        for (auto row: KeyMatrices[r]) {
            std::cout << "[";
            for (unsigned i = 0; i < keysize; ++i) {
                std::cout << row[i];
                if (i != keysize - 1) {
                    std::cout << ", ";
                }
            }
            std::cout << "]" << std::endl;
        }
        if (r != rounds) {
            std::cout << std::endl;
        }
    }
}


/////////////////////////////
// LowMC private functions //
/////////////////////////////


mblock LowMC::Substitution (const mblock message) {
    mblock temp = 0;
    //Get the identity part of the message
    temp ^= (message >> 3*numofboxes);
    //Get the rest through the Sboxes
    for (unsigned i = 1; i <= numofboxes; ++i) {
        temp <<= 3;
        // look up table
        temp ^= Sbox[ ((message >> 3*(numofboxes-i))
                      & mblock(0x7)).to_ulong()];
    }
    return temp;
}


mblock LowMC::invSubstitution (const mblock message) {
    mblock temp = 0;
    //Get the identity part of the message
    temp ^= (message >> 3*numofboxes);
    //Get the rest through the invSboxes
    for (unsigned i = 1; i <= numofboxes; ++i) {
        temp <<= 3;
        temp ^= invSbox[ ((message >> 3*(numofboxes-i))
                         & mblock(0x7)).to_ulong()];
    }
    return temp;
}


mblock LowMC::MultiplyWithGF2Matrix
        (const std::vector<mblock> matrix, const mblock message) {
    mblock temp = 0;
    for (unsigned i = 0; i < blocksize; ++i) {
        temp[i] = (message & matrix[i]).count() % 2;
    }
    return temp;
}


mblock LowMC::MultiplyWithGF2Matrix_Key
        (const std::vector<keyblock> matrix, const keyblock k) {
    mblock temp = 0;
    for (unsigned i = 0; i < blocksize; ++i) {
        temp[i] = (k & matrix[i]).count() % 2;
    }
    return temp;
}

void LowMC::keyschedule () {
    roundkeys.clear();
    for (unsigned r = 0; r <= rounds; ++r) {
        roundkeys.push_back( MultiplyWithGF2Matrix_Key (KeyMatrices[r], key) );
    }
    return;
}

// todo: read matrix from file
void LowMC::instantiate_LowMC () {
    // Create LinMatrices and invLinMatrices
    LinMatrices.clear();
    invLinMatrices.clear();
    // for (unsigned r = 0; r < rounds; ++r) {
    //     // Create matrix
    //     std::vector<mblock> mat;
    //     // Fill matrix with random bits
    //     do {
    //         mat.clear();
    //         for (unsigned i = 0; i < blocksize; ++i) {
    //             mat.push_back( getrandblock () );
    //         }
    //     // Repeat if matrix is not invertible
    //     } while ( rank_of_Matrix(mat) != blocksize );
    //     LinMatrices.push_back(mat);
    //     invLinMatrices.push_back(invert_Matrix (LinMatrices.back()));
    // }
    LinMatrices.resize(rounds);
    for (u32 i = 0; i < blocksize; i++){
        LinMatrices[0].emplace_back((mblock(L_0[i][0]) << 64) | mblock(L_0[i][1]));
        LinMatrices[1].emplace_back((mblock(L_1[i][0]) << 64) | mblock(L_1[i][1]));
        LinMatrices[2].emplace_back((mblock(L_2[i][0]) << 64) | mblock(L_2[i][1]));
        LinMatrices[3].emplace_back((mblock(L_3[i][0]) << 64) | mblock(L_3[i][1]));
        LinMatrices[4].emplace_back((mblock(L_4[i][0]) << 64) | mblock(L_4[i][1]));
        LinMatrices[5].emplace_back((mblock(L_5[i][0]) << 64) | mblock(L_5[i][1]));
        LinMatrices[6].emplace_back((mblock(L_6[i][0]) << 64) | mblock(L_6[i][1]));
        LinMatrices[7].emplace_back((mblock(L_7[i][0]) << 64) | mblock(L_7[i][1]));
        LinMatrices[8].emplace_back((mblock(L_8[i][0]) << 64) | mblock(L_8[i][1]));
        LinMatrices[9].emplace_back((mblock(L_9[i][0]) << 64) | mblock(L_9[i][1]));
        LinMatrices[10].emplace_back((mblock(L_10[i][0]) << 64) | mblock(L_10[i][1]));
        LinMatrices[11].emplace_back((mblock(L_11[i][0]) << 64) | mblock(L_11[i][1]));
        LinMatrices[12].emplace_back((mblock(L_12[i][0]) << 64) | mblock(L_12[i][1]));
        LinMatrices[13].emplace_back((mblock(L_13[i][0]) << 64) | mblock(L_13[i][1]));
        LinMatrices[14].emplace_back((mblock(L_14[i][0]) << 64) | mblock(L_14[i][1]));
        LinMatrices[15].emplace_back((mblock(L_15[i][0]) << 64) | mblock(L_15[i][1]));
        LinMatrices[16].emplace_back((mblock(L_16[i][0]) << 64) | mblock(L_16[i][1]));
        LinMatrices[17].emplace_back((mblock(L_17[i][0]) << 64) | mblock(L_17[i][1]));
        LinMatrices[18].emplace_back((mblock(L_18[i][0]) << 64) | mblock(L_18[i][1]));
        LinMatrices[19].emplace_back((mblock(L_19[i][0]) << 64) | mblock(L_19[i][1]));
    }
    // invLinMatrices.resize(rounds);
    // for (u32 i = 0; i < blocksize; i++){
        // invLinMatrices[0].emplace_back((mblock(L_0_inv[i][0]) << 64) | mblock(L_0_inv[i][1]));
        // invLinMatrices[1].emplace_back((mblock(L_1_inv[i][0]) << 64) | mblock(L_1_inv[i][1]));
        // invLinMatrices[2].emplace_back((mblock(L_2_inv[i][0]) << 64) | mblock(L_2_inv[i][1]));
        // invLinMatrices[3].emplace_back((mblock(L_3_inv[i][0]) << 64) | mblock(L_3_inv[i][1]));
        // invLinMatrices[4].emplace_back((mblock(L_4_inv[i][0]) << 64) | mblock(L_4_inv[i][1]));
        // invLinMatrices[5].emplace_back((mblock(L_5_inv[i][0]) << 64) | mblock(L_5_inv[i][1]));
        // invLinMatrices[6].emplace_back((mblock(L_6_inv[i][0]) << 64) | mblock(L_6_inv[i][1]));
        // invLinMatrices[7].emplace_back((mblock(L_7_inv[i][0]) << 64) | mblock(L_7_inv[i][1]));
        // invLinMatrices[8].emplace_back((mblock(L_8_inv[i][0]) << 64) | mblock(L_8_inv[i][1]));
        // invLinMatrices[9].emplace_back((mblock(L_9_inv[i][0]) << 64) | mblock(L_9_inv[i][1]));
        // invLinMatrices[10].emplace_back((mblock(L_10_inv[i][0]) << 64) | mblock(L_10_inv[i][1]));
        // invLinMatrices[11].emplace_back((mblock(L_11_inv[i][0]) << 64) | mblock(L_11_inv[i][1]));
        // invLinMatrices[12].emplace_back((mblock(L_12_inv[i][0]) << 64) | mblock(L_12_inv[i][1]));
        // invLinMatrices[13].emplace_back((mblock(L_13_inv[i][0]) << 64) | mblock(L_13_inv[i][1]));
        // invLinMatrices[14].emplace_back((mblock(L_14_inv[i][0]) << 64) | mblock(L_14_inv[i][1]));
        // invLinMatrices[15].emplace_back((mblock(L_15_inv[i][0]) << 64) | mblock(L_15_inv[i][1]));
        // invLinMatrices[16].emplace_back((mblock(L_16_inv[i][0]) << 64) | mblock(L_16_inv[i][1]));
        // invLinMatrices[17].emplace_back((mblock(L_17_inv[i][0]) << 64) | mblock(L_17_inv[i][1]));
        // invLinMatrices[18].emplace_back((mblock(L_18_inv[i][0]) << 64) | mblock(L_18_inv[i][1]));
        // invLinMatrices[19].emplace_back((mblock(L_19_inv[i][0]) << 64) | mblock(L_19_inv[i][1]));
    // }
    for (u32 i = 0; i < rounds; ++i){
        invLinMatrices.emplace_back(invert_Matrix(LinMatrices[i]));
    }

    // Create roundconstants
    roundconstants.clear();
    // for (unsigned r = 0; r < rounds; ++r) {
    //     roundconstants.push_back( getrandblock () );
    // }
    roundconstants.emplace_back(mblock(C_0[0]) << 64 | mblock(C_0[1]));
    roundconstants.emplace_back(mblock(C_1[0]) << 64 | mblock(C_1[1]));
    roundconstants.emplace_back(mblock(C_2[0]) << 64 | mblock(C_2[1]));
    roundconstants.emplace_back(mblock(C_3[0]) << 64 | mblock(C_3[1]));
    roundconstants.emplace_back(mblock(C_4[0]) << 64 | mblock(C_4[1]));
    roundconstants.emplace_back(mblock(C_5[0]) << 64 | mblock(C_5[1]));
    roundconstants.emplace_back(mblock(C_6[0]) << 64 | mblock(C_6[1]));
    roundconstants.emplace_back(mblock(C_7[0]) << 64 | mblock(C_7[1]));
    roundconstants.emplace_back(mblock(C_8[0]) << 64 | mblock(C_8[1]));
    roundconstants.emplace_back(mblock(C_9[0]) << 64 | mblock(C_9[1]));
    roundconstants.emplace_back(mblock(C_10[0]) << 64 | mblock(C_10[1]));
    roundconstants.emplace_back(mblock(C_11[0]) << 64 | mblock(C_11[1]));
    roundconstants.emplace_back(mblock(C_12[0]) << 64 | mblock(C_12[1]));
    roundconstants.emplace_back(mblock(C_13[0]) << 64 | mblock(C_13[1]));
    roundconstants.emplace_back(mblock(C_14[0]) << 64 | mblock(C_14[1]));
    roundconstants.emplace_back(mblock(C_15[0]) << 64 | mblock(C_15[1]));
    roundconstants.emplace_back(mblock(C_16[0]) << 64 | mblock(C_16[1]));
    roundconstants.emplace_back(mblock(C_17[0]) << 64 | mblock(C_17[1]));
    roundconstants.emplace_back(mblock(C_18[0]) << 64 | mblock(C_18[1]));
    roundconstants.emplace_back(mblock(C_19[0]) << 64 | mblock(C_19[1]));

    // Create KeyMatrices
    KeyMatrices.clear();
    // for (unsigned r = 0; r <= rounds; ++r) {
    //     // Create matrix
    //     std::vector<keyblock> mat;
    //     // Fill matrix with random bits
    //     do {
    //         mat.clear();
    //         for (unsigned i = 0; i < blocksize; ++i) {
    //             mat.push_back( getrandkeyblock () );
    //         }
    //     // Repeat if matrix is not of maximal rank
    //     } while ( rank_of_Matrix_Key(mat) < std::min(blocksize, keysize) );
    //     KeyMatrices.push_back(mat);
    // }
    KeyMatrices.resize(rounds + 1);
    for (u32 i = 0; i < blocksize; i++){
        KeyMatrices[0].emplace_back((mblock(K_0[i][0]) << 64) | mblock(K_0[i][1]));
        KeyMatrices[1].emplace_back((mblock(K_1[i][0]) << 64) | mblock(K_1[i][1]));
        KeyMatrices[2].emplace_back((mblock(K_2[i][0]) << 64) | mblock(K_2[i][1]));
        KeyMatrices[3].emplace_back((mblock(K_3[i][0]) << 64) | mblock(K_3[i][1]));
        KeyMatrices[4].emplace_back((mblock(K_4[i][0]) << 64) | mblock(K_4[i][1]));
        KeyMatrices[5].emplace_back((mblock(K_5[i][0]) << 64) | mblock(K_5[i][1]));
        KeyMatrices[6].emplace_back((mblock(K_6[i][0]) << 64) | mblock(K_6[i][1]));
        KeyMatrices[7].emplace_back((mblock(K_7[i][0]) << 64) | mblock(K_7[i][1]));
        KeyMatrices[8].emplace_back((mblock(K_8[i][0]) << 64) | mblock(K_8[i][1]));
        KeyMatrices[9].emplace_back((mblock(K_9[i][0]) << 64) | mblock(K_9[i][1]));
        KeyMatrices[10].emplace_back((mblock(K_10[i][0]) << 64) | mblock(K_10[i][1]));
        KeyMatrices[11].emplace_back((mblock(K_11[i][0]) << 64) | mblock(K_11[i][1]));
        KeyMatrices[12].emplace_back((mblock(K_12[i][0]) << 64) | mblock(K_12[i][1]));
        KeyMatrices[13].emplace_back((mblock(K_13[i][0]) << 64) | mblock(K_13[i][1]));
        KeyMatrices[14].emplace_back((mblock(K_14[i][0]) << 64) | mblock(K_14[i][1]));
        KeyMatrices[15].emplace_back((mblock(K_15[i][0]) << 64) | mblock(K_15[i][1]));
        KeyMatrices[16].emplace_back((mblock(K_16[i][0]) << 64) | mblock(K_16[i][1]));
        KeyMatrices[17].emplace_back((mblock(K_17[i][0]) << 64) | mblock(K_17[i][1]));
        KeyMatrices[18].emplace_back((mblock(K_18[i][0]) << 64) | mblock(K_18[i][1]));
        KeyMatrices[19].emplace_back((mblock(K_19[i][0]) << 64) | mblock(K_19[i][1]));
        KeyMatrices[20].emplace_back((mblock(K_20[i][0]) << 64) | mblock(K_20[i][1]));
    }
    return;
}


/////////////////////////////
// Binary matrix functions //
/////////////////////////////


unsigned LowMC::rank_of_Matrix (const std::vector<mblock> matrix) {
    std::vector<mblock> mat; //Copy of the matrix 
    for (auto u : matrix) {
        mat.push_back(u);
    }
    unsigned size = mat[0].size();
    //Transform to upper triangular matrix
    unsigned row = 0;
    for (unsigned col = 1; col <= size; ++col) {
        if ( !mat[row][size-col] ) {
            unsigned r = row;
            while (r < mat.size() && !mat[r][size-col]) {
                ++r;
            }
            if (r >= mat.size()) {
                continue;
            } else {
                auto temp = mat[row];
                mat[row] = mat[r];
                mat[r] = temp;
            }
        }
        for (unsigned i = row+1; i < mat.size(); ++i) {
            if ( mat[i][size-col] ) mat[i] ^= mat[row];
        }
        ++row;
        if (row == size) break;
    }
    return row;
}


unsigned LowMC::rank_of_Matrix_Key (const std::vector<keyblock> matrix) {
    std::vector<keyblock> mat; //Copy of the matrix 
    for (auto u : matrix) {
        mat.push_back(u);
    }
    unsigned size = mat[0].size();
    //Transform to upper triangular matrix
    unsigned row = 0;
    for (unsigned col = 1; col <= size; ++col) {
        if ( !mat[row][size-col] ) {
            unsigned r = row;
            while (r < mat.size() && !mat[r][size-col]) {
                ++r;
            }
            if (r >= mat.size()) {
                continue;
            } else {
                auto temp = mat[row];
                mat[row] = mat[r];
                mat[r] = temp;
            }
        }
        for (unsigned i = row+1; i < mat.size(); ++i) {
            if ( mat[i][size-col] ) mat[i] ^= mat[row];
        }
        ++row;
        if (row == size) break;
    }
    return row;
}


std::vector<mblock> LowMC::invert_Matrix (const std::vector<mblock> matrix) {
    std::vector<mblock> mat; //Copy of the matrix 
    for (auto u : matrix) {
        mat.push_back(u);
    }
    std::vector<mblock> invmat(blocksize, 0); //To hold the inverted matrix
    for (unsigned i = 0; i < blocksize; ++i) {
        invmat[i][i] = 1;
    }

    unsigned size = mat[0].size();
    //Transform to upper triangular matrix
    unsigned row = 0;
    for (unsigned col = 0; col < size; ++col) {
        if ( !mat[row][col] ) {
            unsigned r = row+1;
            while (r < mat.size() && !mat[r][col]) {
                ++r;
            }
            if (r >= mat.size()) {
                continue;
            } else {
                auto temp = mat[row];
                mat[row] = mat[r];
                mat[r] = temp;
                temp = invmat[row];
                invmat[row] = invmat[r];
                invmat[r] = temp;
            }
        }
        for (unsigned i = row+1; i < mat.size(); ++i) {
            if ( mat[i][col] ) {
                mat[i] ^= mat[row];
                invmat[i] ^= invmat[row];
            }
        }
        ++row;
    }

    //Transform to identity matrix
    for (unsigned col = size; col > 0; --col) {
        for (unsigned r = 0; r < col-1; ++r) {
            if (mat[r][col-1]) {
                mat[r] ^= mat[col-1];
                invmat[r] ^= invmat[col-1];
            }
        }
    }

    return invmat;
}

///////////////////////
// Pseudorandom bits //
///////////////////////


mblock LowMC::getrandblock () {
    mblock tmp = 0;
    for (unsigned i = 0; i < blocksize; ++i) tmp[i] = getrandbit ();
    return tmp;
}

keyblock LowMC::getrandkeyblock () {
    keyblock tmp = 0;
    for (unsigned i = 0; i < keysize; ++i) tmp[i] = getrandbit ();
    return tmp;
}


// Uses the Grain LSFR as self-shrinking generator to create pseudorandom bits
// Is initialized with the all 1s state
// The first 160 bits are thrown away
bool LowMC::getrandbit () {
    static std::bitset<80> state; //Keeps the 80 bit LSFR state
    bool tmp = 0;
    //If state has not been initialized yet
    if (state.none ()) {
        state.set (); //Initialize with all bits set
        //Throw the first 160 bits away
        for (unsigned i = 0; i < 160; ++i) {
            //Update the state
            tmp =  state[0] ^ state[13] ^ state[23]
                       ^ state[38] ^ state[51] ^ state[62];
            state >>= 1;
            state[79] = tmp;
        }
    }
    //choice records whether the first bit is 1 or 0.
    //The second bit is produced if the first bit is 1.
    bool choice = false;
    do {
        //Update the state
        tmp =  state[0] ^ state[13] ^ state[23]
                   ^ state[38] ^ state[51] ^ state[62];
        state >>= 1;
        state[79] = tmp;
        choice = tmp;
        tmp =  state[0] ^ state[13] ^ state[23]
                   ^ state[38] ^ state[51] ^ state[62];
        state >>= 1;
        state[79] = tmp;
    } while (! choice);
    return tmp;
}



