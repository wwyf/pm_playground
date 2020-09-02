#include <iostream>
#include <string>
#include <chrono>
#include <vector>
#include <cmath>
#include <random>
#include <libpmemobj.h>

const uint64_t block_size = 1024*1024; // 1MB
const uint64_t count = 1*1024 * 8; 
// const uint64_t count = 8; 
const uint64_t data_space_size = block_size*count; // 8GB

PMEMobjpool * pop = NULL;

double test_write_pm(size_t io_size, uint8_t * data_space_ptr){
    // return latency

    uint8_t * dummy_data = (uint8_t*)malloc(io_size);
    for (uint64_t i = 0; i < io_size; i++){
        dummy_data[i] = (random()+i) % 256;
    }

    int i = 0;
    uint64_t offset = 0;
    uint64_t duration; //ns
    auto start_time = std::chrono::system_clock::now();

    while(i < count){
        pmemobj_memcpy_persist(pop, dummy_data ,data_space_ptr+offset, io_size);
        offset += block_size;
        i++;
    }

    duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now() - start_time).count();
    

    return (double)duration/count;
}

double test_write_pm_2(size_t io_size, uint8_t * data_space_ptr){
    // return latency

    uint8_t * dummy_data = (uint8_t*)malloc(io_size);
    for (uint64_t i = 0; i < io_size; i++){
        dummy_data[i] = (random()+i) % 256;
    }

    uint64_t offset = 0;
    uint64_t offsets[count];
    int i = 0;
    while(i < count){
        offsets[i] = offset;
        offset += block_size;
        i++;
    }
    uint64_t duration; //ns
    i = 0;
    auto start_time = std::chrono::system_clock::now();

    while(i < count){
        pmemobj_memcpy_persist(pop, dummy_data ,data_space_ptr+offsets[i], io_size);
        i++;
    }

    duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now() - start_time).count();
    

    return (double)duration/count;
}

int main(){
    // init io_sizes
    uint64_t l = 3; // 2^3 8Bytes
    // uint64_t h = 16; // 2^16 64KB
    uint64_t h = 18; // 2^18 256KB
    uint64_t start_io_size = 8;
    uint64_t repeated = 10;
    std::vector<int> io_sizes;
    for (;l <= h; l++){
        io_sizes.push_back(start_io_size);
        start_io_size *= 2;
    }

    // init pm
    std::string pool_file = "/mnt/pmem/test_write_pool";
    std::remove(pool_file.c_str());
    pop = pmemobj_create(pool_file.c_str(), "TEST_WRITE", 1024ULL*1024ULL*1024ULL*20ULL, 0066);
    PMEMoid data_space;
    if(pmemobj_alloc(pop, &data_space, data_space_size, 0,0, NULL)){
        std::cout << "alloc error" << std::endl;
        return -1;
    }
    uint8_t * data_space_ptr = (uint8_t*)pmemobj_direct(data_space) + 48ULL;

    std::cout << "io_size(Bytes),write_avg_latency(ns),throughput,bandwidth(MB)" << std::endl;

    for (auto io_size : io_sizes){
        double latency = 0;
        for (uint64_t i = 0; i < repeated; i++){
            // latency += test_write_pm(io_size, data_space_ptr);
            latency += test_write_pm_2(io_size, data_space_ptr);
        }
        double avg_latency = latency/repeated;
        uint64_t throughput = 1000*1000*1000/avg_latency;
        uint64_t bandwidth = io_size*throughput/(1024*1024);
        std::cout << io_size
        << "," << avg_latency
        << "," << throughput
        << "," << bandwidth
        << std::endl;
    }


    // close bench
    std::remove(pool_file.c_str());


    return 0;
}