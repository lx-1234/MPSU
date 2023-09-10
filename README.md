# Multi-party Private Set Union

### Environment

This code and following instructions is tested on Ubuntu 22.04, with g++ 11.3.0 and CMake 3.22.1

### Installation

```bash
git clone https://github.com/lx-1234/MPSU.git
cd MPSU

git clone https://github.com/Visa-Research/volepsi.git
cd volepsi
git checkout 63990d8b873622844bb0ac588ab19d2ca66f062e

# compile and install volepsi
python3 build.py -DVOLE_PSI_ENABLE_BOOST=ON -DVOLE_PSI_ENABLE_GMW=ON -DVOLE_PSI_ENABLE_CPSI=OFF -DVOLE_PSI_ENABLE_OPPRF=OFF
python3 python3 build.py --install=../libvolepsi
cp volepsi/out/build/linux/volePSI/config.h ../libvolepsi/include/volePSI/
```

### Compile MPSU

```bash
# in MPSU
mkdir build && cd build
cmake ..
make
```

### Running the code

##### Print help information

```bash
./main -h
```

##### MPSU

Compute PSU of 3 parties each with set size $2^{14}$ using 4 threads.

```bash
# generate share correlation
mkdir sc
./main -psu -k 3 -nn 14 -nt 4 -genSC
# generate boolean triples
mkdir triple
./main -psu -genTriple -k 3 -nn 14 -nt 4 -r 0 & ./main -psu -genTriple -k 3 -nn 14 -nt 4 -r 1 & ./main -psu -genTriple -k 3 -nn 14 -nt 4 -r 2
# run MPSU
./main -psu -k 3 -nn 14 -nt 4 -r 0 & ./main -psu -k 3 -nn 14 -nt 4 -r 1 & ./main -psu -k 3 -nn 14 -nt 4 -r 2
```

