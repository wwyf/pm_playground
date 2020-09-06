#!/bin/bash
# set -x

spack load gcc@9.3.0
spack load builtin.pmdk@1.9
# export PMEM_AVX512F=0
# export PMEM_AVX=0
# export PMEM_NO_MOVNT=1
#export PMEM_NO_GENERIC_MEMCPY=0
g++ main.cpp -o alloc -std=c++11  -lpmemobj

date
export PMEMOBJ_LOG_LEVEL=4
sudo PATH=${PATH} LD_LIBRARY_PATH=${LD_LIBRARY_PATH} PMEMOBJ_LOG_LEVEL=4 -E ./alloc | tee log

date

rm alloc