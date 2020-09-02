#!/bin/bash
# set -x

# spack load gcc@9.3.0
# spack load builtin.pmdk@1.9
# g++ main.cpp -o io_size -std=c++17 -Og -march=x86-64 -lpmemobj
g++ main.cpp -o io_size -std=c++11 -Ofast -lpmemobj
# g++ main.cpp -o io_size -std=c++17 -Og -g  -lpmemobj 

sudo PATH=${PATH} LD_LIBRARY_PATH=${LD_LIBRARY_PATH} -E ./io_size | tee io_size.csv

cat io_size.csv | column -t -s , 

# rm io_size