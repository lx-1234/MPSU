#include "Circuit.h"

BetaCircuit inverse_of_S_box_layer(u8 num_S_box)
{
    BetaCircuit cd;

    BetaBundle a(128);//初始128位为数据, 第129位为0, 第130位为1
    BetaBundle b(128);//初始128位为数据, 第129位为0, 第130位为1
    BetaBundle temp(5 * num_S_box);

    cd.addInputBundle(a);
    cd.addOutputBundle(b);
    cd.addTempWireBundle(temp);

    //for (u64 i = 1; i < bits; ++i)
    //    cd.addGate(a.mWires[i], a.mWires[i], oc::GateType::Nxor, a.mWires[i]);
    // auto ts = [](int s) {return std::to_string(s); };

    for (u8 i = 0; i < 128 - 3 * num_S_box; i++)
        cd.addCopy(a.mWires[i], b.mWires[i]);

    u8 j = 0;
    for (u8 i = 128 - 3 * num_S_box; i < 128; i+=3)
    {
        cd.addGate(a.mWires[i+1], a.mWires[i+2], GateType::And, temp.mWires[j]);//t0=z1*z2
        cd.addGate(a.mWires[i], a.mWires[i+1], GateType::Xor, temp.mWires[j+1]);//t1=z0+z1
        cd.addGate(temp.mWires[j], temp.mWires[j+1], GateType::Xor, b.mWires[i]);//x0=t0+t1
        
        cd.addGate(a.mWires[i], a.mWires[i+2], GateType::And, temp.mWires[j+2]);//t2=z0*z2
        cd.addGate(temp.mWires[j+2], a.mWires[i+1], GateType::Xor, b.mWires[i+1]);//x1=t2+z1

        cd.addGate(a.mWires[i], a.mWires[i+1], GateType::And, temp.mWires[j+3]);//t3=z0*z1
        cd.addGate(temp.mWires[j+3], temp.mWires[j+1], GateType::Xor, temp.mWires[j+4]);//t4=t3+t1
        cd.addGate(temp.mWires[j+4], a.mWires[i+2], GateType::Xor, b.mWires[i+2]);//x2=t4+z2

        j += 5;
    }

    cd.levelByAndDepth();
    return cd;
}

BetaCircuit lessthanN(u32 n){
    BetaCircuit cd;

    BetaBundle a(128);
    cd.addInputBundle(a);

    // 64-bit elements in a 128-bit bitset
    u32 bits = 128 - oc::log2ceil(n);

    u64 step = 1;

    while (step < bits){
        for (u64 i = 0; i + step < bits; i += step * 2){
            //std::cout << "a[" << i << "] &= a[" << (i + step) << "]" << std::endl;
            cd.addGate(a.mWires[i], a.mWires[i + step], oc::GateType::Or, a.mWires[i]);
        }
        step *= 2;
    }
    //cd.addOutputBundle()
    cd.addInvert(a[0]);
    a.mWires.resize(1);
    cd.mOutputs.push_back(a);

    cd.levelByAndDepth();
    return cd;
}
    

    // BetaCircuit inverse_of_linear_layer(u8** L)//L是一个128*16的二维字节数组, 每个bit对应矩阵的每一位, 每一行16个字节对应的128维向量与输入向量做内积得到输出向量的一位
    // {
    //     BetaCircuit cd;

    //     BetaBundle a(130);//初始128位为数据, 第129位为0, 第130位为1
    //     BetaBundle b(130);//初始128位为数据, 第129位为0, 第130位为1
    //     BetaBundle temp(128 * 128);

    //     cd.addInputBundle(a);
    //     cd.addOutputBundle(b);
    //     cd.addTempWireBundle(temp);

    //     //for (u64 i = 1; i < bits; ++i)
    //     //    cd.addGate(a.mWires[i], a.mWires[i], oc::GateType::Nxor, a.mWires[i]);
    //     // auto ts = [](int s) {return std::to_string(s); };

    //     for (int i = 128; i < 130; i++)
    //         cd.addCopy(a.mWires[i], b.mWires[i]);

    //     for (int i = 0; i < 128; i++)
    //     {
    //         int base = i * 128;

    //         int L_i[128];

    //         for (int j = 0; j < 16; j++){
    //             L_i[j * 8 + 0] = ((L[i][j] & (u8)(1<<7))!=0);
    //             L_i[j * 8 + 1] = ((L[i][j] & (u8)(1<<6))!=0);
    //             L_i[j * 8 + 2] = ((L[i][j] & (u8)(1<<5))!=0);
    //             L_i[j * 8 + 3] = ((L[i][j] & (u8)(1<<4))!=0);
    //             L_i[j * 8 + 4] = ((L[i][j] & (u8)(1<<3))!=0);
    //             L_i[j * 8 + 5] = ((L[i][j] & (u8)(1<<2))!=0);
    //             L_i[j * 8 + 6] = ((L[i][j] & (u8)(1<<1))!=0);
    //             L_i[j * 8 + 7] = ((L[i][j] & (u8)(1<<0))!=0);
    //         }

    //         if (L_i[0] != 0) {
    //             cd.addCopy(a.mWires[0], temp.mWires[base+0]);
    //         }else{
    //             cd.addCopy(a.mWires[128+0], temp.mWires[base+0]);
    //         }


    //         for (int j = 1; j < 128; j++){
    //             if (L_i[j] != 0) {
    //                 cd.addGate(a.mWires[j], temp.mWires[base+j-1], GateType::Xor, temp.mWires[base+j]);
    //             }else{
    //                 cd.addCopy(temp.mWires[base+j-1], temp.mWires[base+j]);
    //             }
    //         }

    //         cd.addCopy(temp.mWires[base+127], b.mWires[i]);

    //     }

    //     return cd;
    // }

    // BetaCircuit inverse_of_constant_addition_layer(u8* C)//C是一个长16的字节数组, 每个bit对应向量C的每一位
    // {
    //     BetaCircuit cd;

    //     BetaBundle a(130);//初始128位为数据, 第129位为0, 第130位为1
    //     BetaBundle b(130);//初始128位为数据, 第129位为0, 第130位为1

    //     cd.addInputBundle(a);
    //     cd.addOutputBundle(b);
        
    //     for (int i = 128; i < 130; i++)
    //         cd.addCopy(a.mWires[i], b.mWires[i]);

    //     int C_i[128];

    //     for (int i = 0; i < 16; i++){
    //         C_i[i * 8 + 0] = ((C[i] & (u8)(1<<7))!=0);
    //         C_i[i * 8 + 1] = ((C[i] & (u8)(1<<6))!=0);
    //         C_i[i * 8 + 2] = ((C[i] & (u8)(1<<5))!=0);
    //         C_i[i * 8 + 3] = ((C[i] & (u8)(1<<4))!=0);
    //         C_i[i * 8 + 4] = ((C[i] & (u8)(1<<3))!=0);
    //         C_i[i * 8 + 5] = ((C[i] & (u8)(1<<2))!=0);
    //         C_i[i * 8 + 6] = ((C[i] & (u8)(1<<1))!=0);
    //         C_i[i * 8 + 7] = ((C[i] & (u8)(1<<0))!=0);
    //     }


    //     for (int i = 0; i < 128; i++){
    //         if (C_i[i] != 0) {
    //             cd.addGate(a.mWires[i], a.mWires[128+1], GateType::Xor, b.mWires[i]);
    //         }else{
    //             cd.addGate(a.mWires[i], a.mWires[128+0], GateType::Xor, b.mWires[i]);
    //         }
    //     }

    //     return cd;
    // }

    // BetaCircuit inverse_of_key_addition_layer(u8* K)//K是一个长16的字节数组, 每个bit对应向量K的每一位
    // {
    //     BetaCircuit cd;

    //     BetaBundle a(130);//初始128位为数据, 第129位为0, 第130位为1
    //     BetaBundle b(130);//初始128位为数据, 第129位为0, 第130位为1

    //     cd.addInputBundle(a);
    //     cd.addOutputBundle(b);
        
    //     for (int i = 128; i < 130; i++)
    //         cd.addCopy(a.mWires[i], b.mWires[i]);

    //     int K_i[128];

    //     for (int i = 0; i < 16; i++){
    //         K_i[i * 8 + 0] = ((K[i] & (u8)(1<<7))!=0);
    //         K_i[i * 8 + 1] = ((K[i] & (u8)(1<<6))!=0);
    //         K_i[i * 8 + 2] = ((K[i] & (u8)(1<<5))!=0);
    //         K_i[i * 8 + 3] = ((K[i] & (u8)(1<<4))!=0);
    //         K_i[i * 8 + 4] = ((K[i] & (u8)(1<<3))!=0);
    //         K_i[i * 8 + 5] = ((K[i] & (u8)(1<<2))!=0);
    //         K_i[i * 8 + 6] = ((K[i] & (u8)(1<<1))!=0);
    //         K_i[i * 8 + 7] = ((K[i] & (u8)(1<<0))!=0);
    //     }


    //     for (int i = 0; i < 128; i++){
    //         if (K_i[i] != 0) {
    //             cd.addGate(a.mWires[i], a.mWires[128+1], GateType::Xor, b.mWires[i]);
    //         }else{
    //             cd.addGate(a.mWires[i], a.mWires[128+0], GateType::Xor, b.mWires[i]);
    //         }
    //     }

    //     return cd;
    // }

