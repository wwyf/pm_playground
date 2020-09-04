#include <iostream>
#include <string>
#include <chrono>
#include <vector>
#include <cmath>
#include <random>
#include <cstring>
#include <algorithm>
#include <cstdlib>
#include <cassert>
#include <unistd.h>
#include <libpmemobj.h>

PMEMobjpool * pop;

// enum operation_t{
//     OP_READ,
//     OP_WRITE
// };

// enum access_pattern_t{
//     AP_RANDOM,
//     AP_SEQ
// };

// enum platform_t{
//     PF_DRAM_cached,
//     PF_PMEM
// };

double test_access_latency(uint8_t* data_space_ptr, std::string platform, std::string operation, std::string access_pattern, uint64_t io_size, uint64_t block_size, uint64_t data_space_size){
    
    assert(io_size <= block_size);

    uint64_t count = data_space_size/block_size;
    

    // offset
    std::vector<uint64_t> offsets;
    for (int i = 0; i < count; i++){
        offsets.push_back(i*block_size);
    }
    if (access_pattern == "random"){
        std::random_shuffle(offsets.begin(), offsets.end());
    } else if (access_pattern == "sequentail"){
        
    } else {
        exit(-1);
    }
    // memcpy
    typedef void * (*memcpy_fn_t)(void *dest,	const void *src, size_t len);
    memcpy_fn_t memcpy_fn;
    if (platform == "DRAM_cached"){
        memcpy_fn = memcpy;
    } else if (platform == "PMEM"){
        memcpy_fn = [](void *dest,	const void *src, size_t len){
            return pmemobj_memcpy(pop, dest, src, len, PMEMOBJ_F_MEM_TEMPORAL);
        };
    }

    uint8_t * dummy_data = (uint8_t*)malloc(io_size);
    for (uint64_t i = 0; i < io_size; i++){
        dummy_data[i] = (random()+i) % 256;
    }

    uint64_t duration = 0; // ns


    auto start_time = std::chrono::system_clock::now();
    if (operation == "write"){
        for (auto offset : offsets){
            memcpy_fn(data_space_ptr+offset, dummy_data ,io_size);
        }
    } else if (operation == "read"){
        for (auto offset : offsets){
            memcpy_fn(dummy_data, data_space_ptr+offset,io_size);
        }
    }
    duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::system_clock::now() - start_time).count();

    return (double)duration/count;


}

void warn_up(uint8_t* data_space_ptr, uint64_t alloc_size){
    test_access_latency(data_space_ptr, "PMEM", "write", "random", 8, 1024*1024, alloc_size);
}

void clean_cache(){
    uint64_t cache_size = 1024ULL*1024ULL*10ULL;
    std::vector<uint64_t> cache_array(cache_size,6);
    for (int i = 0; i < cache_size; i++){
        cache_array[i] += 2;
    }
    uint64_t random_index = random() % cache_size;
    std::cout << "clean cache ok! cache_array[" << random_index << "]:" << cache_array[random_index] << std::endl;
}

void bench_latency(std::string platform, uint8_t * data_space_ptr, uint64_t alloc_size){
    clean_cache();

    std::cout
    << "platform"
    << ",operation"
    << ",access_pattern"
    << ",io_size(B)"
    << ",blcok_size(B)"
    << ",data_space_size(MB)"
    << ",count"
    << ",avg_latency(ns)"
    << ",throughput(op/s)"
    << ",bandwidth(MB)"
    << std::endl;

    auto gen_sizes = [](uint64_t start_size, uint64_t max_size){
        std::vector<uint64_t> sizes;
        while(start_size <= max_size){
            sizes.push_back(start_size);
            start_size*=2;
        }
        return std::move(sizes);
    };



    std::vector<uint64_t> io_sizes = gen_sizes(8, 1024);
    std::vector<uint64_t> block_sizes = gen_sizes(8, 1024*1024);
    // std::vector<uint64_t> data_space_sizes = gen_sizes(1024*1024, 1024ULL*1024ULL*1024ULL*4ULL);
    std::vector<uint64_t> data_space_sizes = gen_sizes(1024*1024, 1024ULL*1024ULL*1024ULL);
    // std::vector<std::string> platforms = {"PMEM", "DRAM_cached"};
    std::vector<std::string> access_patterns = {"sequentail","random"};
    std::vector<std::string> operations = {"write", "read"};


    for (auto access_pattern : access_patterns){
        for (auto data_space_size : data_space_sizes){
            for (auto io_size : io_sizes){
                std::vector<uint64_t> block_sizes = gen_sizes(io_size, 1024*1024);
                for (auto block_size : block_sizes){
                    for (auto operation : operations){
                        auto avg_latency = test_access_latency(data_space_ptr,
                            platform,
                            operation,
                            access_pattern,
                            io_size,
                            block_size,
                            data_space_size
                        );
                        uint64_t throughput = 1000*1000*1000/avg_latency;
                        uint64_t bandwidth = io_size*throughput;
                        std::cout
                        << platform
                        << "," << operation
                        << "," << access_pattern
                        << "," << io_size
                        << "," << block_size
                        << "," << data_space_size/(1024*1024)
                        << "," << data_space_size/block_size
                        << "," << avg_latency
                        << "," << throughput
                        << "," << bandwidth/(1024*1024)
                        << std::endl;

                    }
                }
            }
        }
    }
}

int test_pm(){
    uint64_t capacity = 1024ULL*1024ULL*1024ULL*20ULL;
    uint64_t alloc_size = 1024ULL*1024ULL*1024ULL*2ULL;

    // init pm
    std::string pool_file = "/mnt/pmem/test_write_pool";
    std::remove(pool_file.c_str());
    sleep(2);
    pop = pmemobj_create(pool_file.c_str(), "TEST_WRITE", capacity, 0066);
    PMEMoid data_space;
    if(pmemobj_alloc(pop, &data_space, alloc_size, 0,0, NULL)){
        std::cout << "alloc error" << std::endl;
        std::remove(pool_file.c_str());
        return -1;
    }
    // uint8_t * data_space_ptr = (uint8_t*)pmemobj_direct(data_space) + 0ULL;
    uint8_t * data_space_ptr = (uint8_t*)pmemobj_direct(data_space) + 48ULL;
    alloc_size -= 48ULL;
    warn_up(data_space_ptr, alloc_size);
    bench_latency("PMEM", data_space_ptr, alloc_size);

    pmemobj_free(&data_space);
    pmemobj_close(pop);

    // close bench
    std::remove(pool_file.c_str());
    return 0;
}
int test_dram(){
    uint64_t capacity = 1024ULL*1024ULL*1024ULL*20ULL;
    uint64_t alloc_size = 1024ULL*1024ULL*1024ULL*2ULL;

    uint8_t * data_space_ptr = (uint8_t*)malloc(alloc_size);
    if (data_space_ptr != NULL){
        bench_latency("DRAM_cached", data_space_ptr, alloc_size);
    }

    free(data_space_ptr);

    return 0;
}

int main(){
    test_pm();
    test_dram();
    return 0;
}