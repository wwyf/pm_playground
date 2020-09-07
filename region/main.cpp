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

uint64_t * cache_ptr;
uint64_t cache_size;

uint64_t clean_cache(){
    for (int i = 0; i < cache_size; i++){
        cache_ptr[i] = 6;
    }
    for (int c = 0; c < 2; c++){
        for (int i = 0; i < cache_size; i++){
            cache_ptr[i] += 1;
        }
    }
    uint64_t random_index = random() % cache_size;
    // std::cout << "clean cache ok! cache_array[" << random_index << "]:" << cache_array[random_index] << std::endl;
    return cache_ptr[random_index];
}

double test_access_latency(uint8_t* data_space_ptr, std::string platform, std::string flush_type, std::string operation, std::string access_pattern, uint64_t io_size, uint64_t block_size, uint64_t data_space_size, uint64_t count){
    
    assert(io_size >= 8);
    assert(io_size <= block_size);
    assert(io_size*2 == block_size);

    // offset
    std::vector<uint64_t> addresses;
    for (int i = 1; i < count; i++){
        addresses.push_back((uint64_t)(data_space_ptr + i*block_size));
    }
    if (access_pattern == "random"){
        std::random_shuffle(addresses.begin(), addresses.end());
    } else if (access_pattern == "sequential"){
        
    } else {
        exit(-1);
    }
    uint64_t * cur_ptr;
    cur_ptr = (uint64_t *)data_space_ptr;
    for (auto addr : addresses){
        *cur_ptr = addr;
        cur_ptr = (uint64_t *)addr;
    }
    *cur_ptr = 0;
    cur_ptr = (uint64_t *)(data_space_ptr+io_size);
    for (auto addr : addresses){
        *cur_ptr = addr+io_size;
        cur_ptr = (uint64_t *)*cur_ptr;
    }
    *cur_ptr = 0;

    // for (int i = 0; i < 10; i++){
    //     std::cout << " " << addresses[i];
    // }
    // std::cout << std::endl;
    // for (int i = 0; i < 10; i++){
    //     std::cout << " " << addresses[i]+io_size;
    // }
    // std::cout << std::endl;

    // memcpy
    typedef void * (*memcpy_fn_t)(void *dest,	const void *src, size_t len);
    memcpy_fn_t memcpy_fn;
    if (clean_cache() != 8) {
        exit(-1);
    };
    if (operation == "read"){
        memcpy_fn = memcpy;
    } else if (operation == "write"){
        if (platform == "DRAM_cached"){
            memcpy_fn = memcpy;
        } else if (platform == "PMEM"){
            if (flush_type == "t"){
                memcpy_fn = [](void *dest,	const void *src, size_t len){
                    return pmemobj_memcpy(pop, dest, src, len, PMEMOBJ_F_MEM_TEMPORAL);
                };
            } else if (flush_type == "nt"){
                memcpy_fn = [](void *dest,	const void *src, size_t len){
                    return pmemobj_memcpy_persist(pop, dest, src, len);
                };
            }
        }
    }

    uint64_t * dummy_data = (uint64_t*)malloc(io_size);
    for (uint64_t i = 0; i < io_size/sizeof(uint64_t); i++){
        dummy_data[i] = (random()+i);
    }

    uint64_t duration = 0; // ns
    // uint64_t repeated = 1024*8;
    uint64_t repeated = 1024;
    if (operation == "read"){
        int i = 0;
        // int load_num = 0;
        auto start_time = std::chrono::system_clock::now();
        while(i < repeated){
            // load_num = 0;
            cur_ptr = (uint64_t*)data_space_ptr;
            while(cur_ptr){
                memcpy_fn(dummy_data, cur_ptr, io_size); cur_ptr = (uint64_t*)*dummy_data;
                // std::cout << " " << *dummy_data;
                // load_num++;
            }
            // std::cout << std::endl;
            // std::cout << load_num << std::endl;
            // load_num = 0;
            cur_ptr = (uint64_t*)(data_space_ptr+io_size);
            while(cur_ptr){
                memcpy_fn(dummy_data, cur_ptr, io_size); cur_ptr = (uint64_t*)*dummy_data;
                // std::cout << " " << *dummy_data;
                // load_num++;
            }
            // std::cout << std::endl;
            // std::cout << load_num << std::endl;
            i++;
        }
        duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::system_clock::now() - start_time).count();
    }
    free(dummy_data);

    return (double)duration/(count*repeated);


}



void bench_region(std::string platform, uint8_t * data_space_ptr, uint64_t alloc_size){
    clean_cache();

    std::cout
    << "platform"
    << ",flush_type"
    << ",operation"
    << ",access_pattern"
    << ",io_size(B)"
    << ",blcok_size(B)"
    << ",data_space_size(B)"
    << ",count"
    << ",throughput(op/s)"
    << ",bandwidth(MB)"
    << ",avg_latency(ns)"
    << std::endl;

    auto gen_sizes = [](uint64_t start_size, uint64_t max_size){
        std::vector<uint64_t> sizes;
        while(start_size <= max_size){
            sizes.push_back(start_size);
            start_size*=2;
        }
        return std::move(sizes);
    };



    // std::vector<std::string> operations = {"write"};
    std::vector<std::string> operations = {"read"};
    // std::vector<std::string> operations = {"read", "write"};
    std::vector<std::string> access_patterns = {"random"};
    // std::vector<std::string> access_patterns = {"sequential"};
    // std::vector<std::string> access_patterns = {"sequential","random"};
    // std::vector<uint64_t> io_sizes = gen_sizes(8, 1024*64);
    // std::vector<uint64_t> io_sizes = gen_sizes(8,256);
    std::vector<uint64_t> io_sizes = gen_sizes(32,512);
    // std::vector<uint64_t> block_sizes = gen_sizes(8, 1024ULL*1024ULL);
    // std::vector<uint64_t> block_sizes = gen_sizes(16,16);
    // std::vector<uint64_t> block_sizes = {126976}; // 1024*128 - 4096
    // std::vector<uint64_t> block_sizes = {1024*128, 1024*128+4096, 64*1024*1024, 64*1024*1024+4096, 128*1024*1024}; // 1024*128 - 4096
    // std::vector<uint64_t> block_sizes = {1024*128, 1024*128+4096, 1024*1024, 1024*1024+4096};
    // std::vector<uint64_t> block_sizes = {1024*128, 1024*128+4096};
    std::vector<uint64_t> data_space_sizes = {1024, 8*1024, 16*1024, 32*1024, 1024*1024, 8*1024*1024, 16*1024*1024, 27*1024*1024, 32*1024*1024, 64*1024*1024, 128*1024*1024};
    // std::vector<uint64_t> data_space_sizes = {64, 128};
    // std::vector<uint64_t> data_space_sizes = {1024ULL*1024ULL*1024*8ULL};
    // std::vector<std::string> flush_types = {"nt", "t"};
    std::vector<std::string> flush_types = {"t"};
    // std::vector<std::string> flush_types = {"nt"};

    // std::vector<uint64_t> block_sizes = gen_sizes(8, 1024*1024*4);
    // std::vector<uint64_t> data_space_sizes = gen_sizes(1024*1024, 1024ULL*1024ULL*1024ULL);
    // std::vector<std::string> platforms = {"PMEM", "DRAM_cached"};
    // std::vector<std::string> operations = {"read"};
    for (auto operation : operations){
        for (auto access_pattern : access_patterns){
            for (auto flush_type : flush_types){
                for (auto io_size : io_sizes){
                    auto block_size = io_size * 2;
                    // std::vector<uint64_t> data_space_sizes = gen_sizes(64*block_size, 1024ULL*1024ULL*1024ULL*8ULL);
                    for (auto data_space_size : data_space_sizes){
                        if (data_space_size < block_size) continue;

                        uint64_t count = (data_space_size/block_size) & (~3ULL);
                        if (count > 1024ULL*1024ULL*128ULL){
                            count = 1024ULL*1024ULL*128ULL;
                        }
                        auto avg_latency = test_access_latency(data_space_ptr,
                            platform,
                            flush_type,
                            operation,
                            access_pattern,
                            io_size,
                            block_size,
                            data_space_size,
                            count
                        );
                        uint64_t throughput = 1000*1000*1000/avg_latency;
                        uint64_t bandwidth = io_size*throughput;
                        std::cout
                        << platform
                        << "," << flush_type
                        << "," << operation
                        << "," << access_pattern
                        << "," << io_size
                        << "," << block_size
                        << "," << data_space_size
                        << "," << count
                        << "," << throughput
                        << "," << bandwidth/(1024*1024)
                        << "," << avg_latency
                        << std::endl;
                    }
                }
            }
        }
    }
}


int test_latency(){

    uint64_t capacity = 1024ULL*1024ULL*1024ULL*20ULL;
    uint64_t alloc_size = 1024ULL*1024ULL*256ULL;
    // uint64_t alloc_size = 1024ULL*1024ULL*1024ULL*9ULL;

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
    uint8_t * pmem_data_space_ptr = (uint8_t*)pmemobj_direct(data_space) + 48ULL;
    alloc_size -= 48ULL;


    uint8_t * dram_data_space_ptr = (uint8_t*)malloc(alloc_size);


    for (uint64_t i = 0; i < alloc_size; i++){
        pmem_data_space_ptr[i] = 0;
    }
    bench_region("PMEM", pmem_data_space_ptr, alloc_size);


    // for (uint64_t i = 0; i < alloc_size; i++){
    //     dram_data_space_ptr[i] = 0;
    // }
    // bench_region("DRAM_cached", dram_data_space_ptr, alloc_size);

    // close
    pmemobj_free(&data_space);
    pmemobj_close(pop);
    std::remove(pool_file.c_str());
    free(dram_data_space_ptr);
    return 0;
}


int main(){
    cache_size = 1024ULL*1024ULL*10ULL;
    cache_ptr = (uint64_t*)malloc((cache_size+6)*sizeof(uint64_t));

    test_latency();
    free(cache_ptr);
    return 0;
}