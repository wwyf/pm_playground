#!/bin/bash
# set -x

source /mnt/wyf/env.sh
spack load gcc@9.3.0
spack load builtin.pmdk@1.9
# export PMEM_AVX512F=0
# export PMEM_AVX=0
# export PMEM_NO_MOVNT=1
#export PMEM_NO_GENERIC_MEMCPY=0
g++ main.cpp -o main -std=c++11 -Ofast -lpmemobj
# g++ main.cpp -o io_size -std=c++17 -Og -g -ggdb -fsanitize=address -lpmemobj  
date

sudo PATH=${PATH} LD_LIBRARY_PATH=${LD_LIBRARY_PATH} -E ./main | tee main.csv

date
cat main.csv | column -t -s , 

# rm main