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

double test_merge_latency(
    uint8_t* data_space_ptr,
    std::string platform,
    std::string operation,
    std::string access_pattern,
    uint64_t io_size,
    uint64_t block_size,
    uint64_t data_space_size,
    uint64_t depth,
    uint64_t count
){
    

    assert(io_size >= 8);
    assert(io_size <= block_size);
    assert(count % depth == 0);
    // count = 64;

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
    // uint64_t * cur_ptr = (uint64_t *)data_space_ptr;
    // for (auto addr : addresses){
        // *cur_ptr = addr;
        // cur_ptr = (uint64_t *)addr;
    // }
    // *cur_ptr = 0;
    pmemobj_memset_persist(pop, data_space_ptr, 0, data_space_size);
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
            memcpy_fn = [](void *dest,	const void *src, size_t len){
                return pmemobj_memcpy(pop, dest, src, len, PMEMOBJ_F_MEM_TEMPORAL|PMEMOBJ_F_MEM_NODRAIN);
            };
        }
    }


    uint8_t * dummy_data = (uint8_t*)malloc(io_size*depth);
    addresses.insert(addresses.begin(), (uint64_t) data_space_ptr);
    // cache the addresses
    uint64_t temp;
    for (auto addr : addresses){
        temp ^= addr;
    }
    for (uint64_t i = 0; i < io_size*depth; i++){
        dummy_data[i] = (random()+i) % 256;
    }
    uint64_t duration = 0; // ns
    if (operation == "read"){

        uint64_t i = 0;
        auto start_time = std::chrono::system_clock::now();
        while(i < count){
            for (uint64_t j = 0; j < depth; j++){
                memcpy_fn(dummy_data+j*io_size, (uint64_t*)addresses[i+j], io_size);
            }
            for (uint64_t j = 0; j < depth; j++){
                i += *(dummy_data+j*io_size);
            }
            i+=depth;
        }
        duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::system_clock::now() - start_time).count();

    }
    //  else if (operation == "write"){
    //     if (access_pattern == "sequential"){
    //         uint64_t offset = 0;
    //         auto start_time = std::chrono::system_clock::now();
    //         while(offset < data_space_size){
    //             memcpy_fn(data_space_ptr+offset, dummy_data ,io_size);
    //             offset += block_size;
    //         }
    //         duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
    //                     std::chrono::system_clock::now() - start_time).count();
    //     } else if (access_pattern == "random"){
    //         auto start_time = std::chrono::system_clock::now();
    //         for (auto addr : addresses){
    //             memcpy_fn((uint64_t*)addr, dummy_data ,io_size);
    //         }
    //         duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
    //                     std::chrono::system_clock::now() - start_time).count();
    //     }
    // }
    // *(volatile  uint8_t *)(data_space_ptr+4);
    // *(volatile  uint8_t *)(dummy_data+1);
    // std::cout << temp << std::endl;
    free(dummy_data);
    return (double)duration/count;


}



void bench_test(std::string platform, uint8_t * data_space_ptr, uint64_t alloc_size){
    clean_cache();

    std::cout
    << "platform"
    << ",operation"
    << ",access_pattern"
    << ",io_size(B)"
    << ",depth"
    << ",blcok_size(B)"
    << ",data_space_size(MB)"
    << ",count"
    << ",throughput(op/s)"
    << ",bandwidth(MB)"
    << ",avg_latency(ns)"
    << ",merge_throughput(op/s)"
    << ",merge_latency(ns)"
    << std::endl;

    auto gen_sizes = [](uint64_t start_size, uint64_t max_size){
        std::vector<uint64_t> sizes;
        while(start_size <= max_size){
            sizes.push_back(start_size);
            start_size*=2;
        }
        return std::move(sizes);
    };



    std::vector<std::string> operations = {"read"};
    // std::vector<std::string> operations = {"write", "read"};
    std::vector<std::string> access_patterns = {"random"};
    // std::vector<std::string> access_patterns = {"sequential","random"};
    // std::vector<uint64_t> io_sizes = gen_sizes(8, 1024*64);
    std::vector<uint64_t> io_sizes = gen_sizes(8,64);
    // std::vector<uint64_t> block_sizes = {126976}; // 1024*128 - 4096
    // std::vector<uint64_t> block_sizes = {1024*128, 1024*128+4096, 64*1024*1024, 64*1024*1024+4096, 128*1024*1024}; // 1024*128 - 4096
    // std::vector<uint64_t> block_sizes = {8, 1024*128};
    // std::vector<uint64_t> block_sizes = {8, 1024*128};
    std::vector<uint64_t> block_sizes = {1024*128, 1024*128+4096};
    // std::vector<uint64_t> block_sizes = {1024*128, 1024*128+4096, 1024*1024, 1024*1024+4096};
    // std::vector<uint64_t> data_space_sizes = {1024ULL*1024ULL*2};
    std::vector<uint64_t> data_space_sizes = {1024ULL*1024ULL*1024*8ULL};
    // std::vector<uint64_t> block_sizes = gen_sizes(io_size, 1024ULL*1024ULL);
    // std::vector<uint64_t> depths = {1,2};
    std::vector<uint64_t> depths = {1,2,3,4,5,6};


    // std::vector<uint64_t> block_sizes = gen_sizes(8, 1024*1024*4);
    // std::vector<uint64_t> data_space_sizes = gen_sizes(1024*1024, 1024ULL*1024ULL*1024ULL);
    // std::vector<std::string> platforms = {"PMEM", "DRAM_cached"};
    // std::vector<std::string> operations = {"read"};
    for (auto operation : operations){
        for (auto access_pattern : access_patterns){
            for (auto io_size : io_sizes){
                for (auto block_size : block_sizes){
                    if (block_size < io_size) continue;
                    // std::vector<uint64_t> data_space_sizes = gen_sizes(64*block_size, 1024ULL*1024ULL*1024ULL*8ULL);
                    for (auto data_space_size : data_space_sizes){
                        if (data_space_size < block_size) continue;
                        for (auto depth : depths){
                            uint64_t count = data_space_size/block_size;
                            // 480 ： 1,2,3,4,5,6,64倍数
                            count = count - count % 480;
                            if (count > 1024ULL*1024ULL*120ULL){
                                count = 1024ULL*1024ULL*120ULL;
                            }
                            auto avg_latency = test_merge_latency(data_space_ptr,
                                platform,
                                operation,
                                access_pattern,
                                io_size,
                                block_size,
                                data_space_size,
                                depth,
                                count
                            );
                            uint64_t merge_latency = avg_latency*depth;
                            uint64_t throughput = 1000*1000*1000/avg_latency;
                            uint64_t merge_throughput = 1000*1000*1000/merge_latency;
                            uint64_t bandwidth = io_size*throughput;
                            std::cout
                            << platform
                            << "," << operation
                            << "," << access_pattern
                            << "," << io_size
                            << "," << depth
                            << "," << block_size
                            << "," << data_space_size/(1024*1024)
                            << "," << count
                            << "," << throughput
                            << "," << bandwidth/(1024*1024)
                            << "," << avg_latency
                            << "," << merge_throughput
                            << "," << merge_latency
                            << std::endl;
                        }
                    }
                }
            }
        }
    }
}


int test(){

    uint64_t capacity = 1024ULL*1024ULL*1024ULL*20ULL;
    uint64_t alloc_size = 1024ULL*1024ULL*1024ULL*9ULL;

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
    bench_test("PMEM", pmem_data_space_ptr, alloc_size);

    for (uint64_t i = 0; i < alloc_size; i++){
        dram_data_space_ptr[i] = 0;
    }
    bench_test("DRAM_cached", dram_data_space_ptr, alloc_size);

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

    test();
    free(cache_ptr);
    return 0;
}