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
#include <x86intrin.h>
#define unlikely(x)    __builtin_expect(!!(x), 0) 
#define likely(x)      __builtin_expect(!!(x), 1)

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

// https://stackoverflow.com/questions/12631856/difference-between-rdtscp-rdtsc-memory-and-cpuid-rdtsc
static inline uint64_t  get_start_tsc(){
    uint64_t tsc;
    __asm__ __volatile__(
                        "cpuid;"
                        "rdtsc;"         // serializing read of tsc
                        "shl $32,%%rdx;"  // shift higher 32 bits stored in rdx up
                        "or %%rdx,%%rax;"   // and or onto rax
                        : "=a"(tsc)        // output to tsc variable
                        :
                        :"%rbx" , "%rcx", "%rdx");

    // __asm__ __volatile__(
    //                     "cpuid;"
    //                     "rdtsc;"         // serializing read of tsc
    //                     "shl $32,%%rdx;"  // shift higher 32 bits stored in rdx up
    //                     "or %%rdx,%%rax;"   // and or onto rax
    //                     "mov %%rax, %0;"
    //                     : "=r"(tsc)        // output to tsc variable
    //                     :
    //                     :"%rax", "%rbx" , "%rcx", "%rdx");

    // uint32_t cycles_high, cycles_low;
    // asm volatile (
    // "CPUID\n\t"/*serialize*/
    // "RDTSC\n\t"/*read the clock*/
    // "mov %%edx, %0\n\t"
    // "mov %%eax, %1\n\t": "=r" (cycles_high), "=r"
    // (cycles_low):: "%rax", "%rbx", "%rcx", "%rdx");
    // tsc = (uint64_t(cycles_high) << 32) | cycles_low;
    return tsc;
}
static inline uint64_t  get_end_tsc(){
    uint64_t tsc;
    __asm__ __volatile__(
                        "rdtscp;"         // serializing read of tsc
                        "shl $32,%%rdx;"  // shift higher 32 bits stored in rdx up
                        "or %%rdx,%%rax;"   // and or onto rax
                        "mov %%rax, %0;"
                        "cpuid;"
                        : "=r"(tsc)        // output to tsc variable
                        :
                        :"%rax", "%rbx" , "%rcx", "%rdx");

    // uint32_t cycles_high, cycles_low;
    // asm volatile (
    // "RDTSCP\n\t"/*read the clock*/
    // "mov %%edx, %0\n\t"
    // "mov %%eax, %1\n\t"
    // "CPUID\n\t": "=r" (cycles_high), "=r"
    // (cycles_low):: "%rax", "%rbx", "%rcx", "%rdx");
    // tsc = (uint64_t(cycles_high) << 32) | cycles_low;

    return tsc;
}

void test_access_latency(uint8_t* data_space_ptr, std::string platform, std::string flush_type, std::string operation, std::string access_pattern, uint64_t io_size, uint64_t block_size, uint64_t data_space_size, uint64_t count){
    
    assert(io_size >= 8);
    assert(io_size <= block_size);

    std::vector<double> ps = {0.01, 0.05, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 0.95, 0.99, 0.999, 0.9999, 0.99999};
    std::vector<std::vector<double>> hist = std::vector<std::vector<double>>(ps.size(), std::vector<double>{0,0});

    // offset
    std::vector<uint64_t> addresses;
    for (int i = 1; i < data_space_size/block_size; i++){
        addresses.push_back((uint64_t)(data_space_ptr + i*block_size));
    }
    if (access_pattern == "random"){
        std::random_shuffle(addresses.begin(), addresses.end());
    } else if (access_pattern == "sequential"){
        
    } else {
        exit(-1);
    }
    // for (int i = 0; i < 10; i++){
    //     std::cout << " " << addresses[i];
    // }
    // std::cout << std::endl;
    // create array linked list
    uint64_t * cur_ptr = (uint64_t *)data_space_ptr;
    for (auto addr : addresses){
        *cur_ptr = addr;
        cur_ptr = (uint64_t *)addr;
    }
    *cur_ptr = 0;
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
        dummy_data[i] = (random()+i) % 256;
    }

    
    
    uint64_t cur_count = 0;
    uint64_t offset = 0;
    auto start_time = std::chrono::high_resolution_clock::now();
    auto end_time = std::chrono::high_resolution_clock::now();
    auto start_tsc = get_start_tsc();
    auto end_tsc = get_end_tsc();
    auto temp_start_time = std::chrono::high_resolution_clock::now();
    auto temp_end_time = std::chrono::high_resolution_clock::now();
    auto temp_start_tsc = get_start_tsc();
    auto temp_end_tsc = get_end_tsc();

// 1.1. ns per op
    if (clean_cache() != 8) {
        exit(-1);
    };
    for (uint64_t i = 0; i < io_size/sizeof(uint64_t); i++){
        dummy_data[i] = (random()+i) % 256;
    }
    cur_count = 0;
    offset = 0;
    uint64_t durations[30000] = {0};
    if (operation == "read"){
        cur_ptr = (uint64_t*)data_space_ptr;
        while(cur_count < count){
            start_time = std::chrono::high_resolution_clock::now();
            pmemobj_memcpy_persist(pop, dummy_data, cur_ptr, io_size); cur_ptr = (uint64_t*)*dummy_data;
            end_time = std::chrono::high_resolution_clock::now();
            durations[cur_count] = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count();
            cur_count++;
        }
    } else if (operation == "write"){
        if (access_pattern == "sequential"){
            while(likely(cur_count < count)){
                start_time = std::chrono::high_resolution_clock::now();
                pmemobj_memcpy_persist(pop, data_space_ptr+offset, dummy_data ,io_size);
                end_time = std::chrono::high_resolution_clock::now();
                durations[cur_count] = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count();
                offset += block_size;
                cur_count++;
            }
        } else if (access_pattern == "random"){
            while(likely(cur_count < count)){
                start_time = std::chrono::high_resolution_clock::now();
                pmemobj_memcpy_persist(pop, (uint64_t*)addresses[cur_count], dummy_data ,io_size);
                end_time = std::chrono::high_resolution_clock::now();
                durations[cur_count] = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count();
                cur_count++;
            }
        }
    }
 // 1.2. ns per op with total
    if (clean_cache() != 8) {
        exit(-1);
    };
    for (uint64_t i = 0; i < io_size/sizeof(uint64_t); i++){
        dummy_data[i] = (random()+i) % 256;
    }
    cur_count = 0;
    offset = 0;
    if (operation == "read"){
        cur_ptr = (uint64_t*)data_space_ptr;
        start_time = std::chrono::high_resolution_clock::now();
        while(cur_count < count){
            pmemobj_memcpy_persist(pop, dummy_data, cur_ptr, io_size); cur_ptr = (uint64_t*)*dummy_data;
            cur_count++;
        }
        end_time = std::chrono::high_resolution_clock::now();
    } else if (operation == "write"){
        if (access_pattern == "sequential"){
            uint64_t offset = 0;
            start_time = std::chrono::high_resolution_clock::now();
            while(likely(cur_count < count)){
                pmemobj_memcpy_persist(pop, data_space_ptr+offset, dummy_data ,io_size);
                offset += block_size;
                cur_count++;
            }
            end_time = std::chrono::high_resolution_clock::now();
        } else if (access_pattern == "random"){
            start_time = std::chrono::high_resolution_clock::now();
            while(likely(cur_count < count)){
                pmemobj_memcpy_persist(pop, (uint64_t*)addresses[cur_count], dummy_data ,io_size);
                cur_count++;
            }
            end_time = std::chrono::high_resolution_clock::now();
        }
    }

    std::cout << "ns per op with total : " <<  std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count()/count << std::endl;
// generate ns hists
    std::sort(durations, durations+(count));
    std::cout << "min ns: " << *std::min_element(durations, durations+(count)) << std::endl;
    std::cout << "avg ns: " << std::accumulate(durations, durations+(count), 0ULL)/count << std::endl;
    std::cout << "max ns: " << *std::max_element(durations, durations+(count)) << std::endl;
    for (int i = 0; i < ps.size(); i++){
        hist[i][0] = ps[i];
        hist[i][1] = durations[(uint64_t)((count-1)*ps[i])];
    }
    for (int i = 0; i < hist.size(); i++){
        std::cout << hist[i][0] << "\t:\t" << hist[i][1] << " ns" << std::endl;
    }


// 2.1 cycles per op
    if (clean_cache() != 8) {
        exit(-1);
    };
    for (uint64_t i = 0; i < io_size/sizeof(uint64_t); i++){
        dummy_data[i] = (random()+i) % 256;
    }
    cur_count = 0;
    offset = 0;
    uint64_t cycles[30000] = {0};
    if (operation == "read"){
        cur_ptr = (uint64_t*)data_space_ptr;
        while(cur_count < count){
            start_tsc = get_start_tsc();
            pmemobj_memcpy_persist(pop, dummy_data, cur_ptr, io_size); cur_ptr = (uint64_t*)*dummy_data;
            end_tsc = get_end_tsc();
            cycles[cur_count] = end_tsc - start_tsc;
            cur_count++;
        }
    } else if (operation == "write"){
        if (access_pattern == "sequential"){
            while(likely(cur_count < count)){
                start_tsc = get_start_tsc();
                pmemobj_memcpy_persist(pop, data_space_ptr+offset, dummy_data ,io_size);
                end_tsc = get_end_tsc();
                cycles[cur_count] = end_tsc - start_tsc;
                offset += block_size;
                cur_count++;
            }
        } else if (access_pattern == "random"){
            while(likely(cur_count < count)){
                start_tsc = get_start_tsc();
                pmemobj_memcpy_persist(pop, (uint64_t*)addresses[cur_count], dummy_data ,io_size);
                end_tsc = get_end_tsc();
                cycles[cur_count] = end_tsc - start_tsc;
                cur_count++;
            }
        }
    }
// 2.2 cycles per op with total
    if (clean_cache() != 8) {
        exit(-1);
    };
    for (uint64_t i = 0; i < io_size/sizeof(uint64_t); i++){
        dummy_data[i] = (random()+i) % 256;
    }
    cur_count = 0;
    offset = 0;
    if (operation == "read"){
        cur_ptr = (uint64_t*)data_space_ptr;
        start_tsc = get_start_tsc();
        while(cur_count < count){
            pmemobj_memcpy_persist(pop, dummy_data, cur_ptr, io_size); cur_ptr = (uint64_t*)*dummy_data;
            cur_count++;
        }
        end_tsc = get_end_tsc();
    } else if (operation == "write"){
        if (access_pattern == "sequential"){
            start_tsc = get_start_tsc();
            while(likely(cur_count < count)){
                pmemobj_memcpy_persist(pop, data_space_ptr+offset, dummy_data ,io_size);
                offset += block_size;
                cur_count++;
            }
            end_tsc = get_end_tsc();
        } else if (access_pattern == "random"){
            start_tsc = get_start_tsc();
            while(likely(cur_count < count)){
                pmemobj_memcpy_persist(pop, (uint64_t*)addresses[cur_count], dummy_data ,io_size);
                cur_count++;
            }
            end_tsc = get_end_tsc();
        }
    }
    std::cout << "cycles per op with total : " <<  (end_tsc-start_tsc)/count << std::endl;;
// generate cycle hists
    std::sort(cycles, cycles+(count));
    std::cout << "min cycles : " << *std::min_element(cycles, cycles+(count)) << std::endl;
    std::cout << "avg cycles : " << std::accumulate(cycles, cycles+(count), 0ULL)/count << std::endl;
    std::cout << "max cycles: " << *std::max_element(cycles, cycles+(count)) << std::endl;

    for (int i = 0; i < ps.size(); i++){
        hist[i][0] = ps[i];
        hist[i][1] = cycles[(uint64_t)((count-1)*ps[i])];
    }
    for (int i = 0; i < hist.size(); i++){
        std::cout << hist[i][0] << "\t:\t" << hist[i][1] << " cycles" << std::endl;
    }
// //////////////////////////////////////////////////////////////////////////


    // *(volatile  uint8_t *)(data_space_ptr+4);
    // *(volatile  uint8_t *)(dummy_data+1);
    free(dummy_data);

    return ;
}




void bench_rw_latency(std::string platform, uint8_t * data_space_ptr, uint64_t alloc_size){

    std::cout
    << "platform"
    << ",flush_type"
    << ",operation"
    << ",access_pattern"
    << ",io_size(B)"
    << ",blcok_size(B)"
    << ",data_space_size(MB)"
    << ",count"
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
    // std::vector<std::string> operations = {"read"};
    std::vector<std::string> operations = {"read", "write"};
    // std::vector<std::string> access_patterns = {"random"};
    std::vector<std::string> access_patterns = {"sequential"};
    // std::vector<std::string> access_patterns = {"sequential","random"};
    // std::vector<uint64_t> io_sizes = gen_sizes(8, 1024*64);
    // std::vector<uint64_t> io_sizes = gen_sizes(8,256);
    std::vector<uint64_t> io_sizes = {32,64};
    // std::vector<uint64_t> io_sizes = {32,64, 128, 256, 512, 1024};
    // std::vector<uint64_t> block_sizes = gen_sizes(8, 1024ULL*1024ULL);
    // std::vector<uint64_t> block_sizes = gen_sizes(256,256);
    // std::vector<uint64_t> block_sizes = {126976}; // 1024*128 - 4096
    // std::vector<uint64_t> block_sizes = {1024*128, 1024*128+4096, 64*1024*1024, 64*1024*1024+4096, 128*1024*1024}; // 1024*128 - 4096
    // std::vector<uint64_t> block_sizes = {1024*128, 1024*128+4096, 1024*1024, 1024*1024+4096};
    // std::vector<uint64_t> block_sizes = {1024*128*2+4096};
    // std::vector<uint64_t> block_sizes = {1024*1024};
    std::vector<uint64_t> block_sizes = {1024*1024+4096};
    std::vector<uint64_t> data_space_sizes = {1024ULL*1024ULL*1024*13ULL};
    // std::vector<std::string> flush_types = {"t"};
    std::vector<std::string> flush_types = {"nt"};
    // std::vector<std::string> flush_types = {"nt", "t"};


    for (auto operation : operations){
        for (auto access_pattern : access_patterns){
            for (auto flush_type : flush_types){
                for (auto io_size : io_sizes){
                    for (auto block_size : block_sizes){
                        if (block_size < io_size) continue;
                        // std::vector<uint64_t> data_space_sizes = gen_sizes(64*block_size, 1024ULL*1024ULL*1024ULL*8ULL);
                        for (auto data_space_size : data_space_sizes){
                            if (data_space_size < block_size) continue;

                            uint64_t count = (data_space_size/block_size) & (~63ULL);
                            if (count > 1024ULL*1024ULL*128ULL){
                                count = 1024ULL*1024ULL*128ULL;
                            }
                            if (count > 1024*12){
                                count = 1024*12;
                            }
                            std::cout
                            << platform
                            << "," << flush_type
                            << "," << operation
                            << "," << access_pattern
                            << "," << io_size
                            << "," << block_size
                            << "," << data_space_size/(1024*1024)
                            << "," << count
                            << std::endl;

                            test_access_latency(data_space_ptr,
                                platform,
                                flush_type,
                                operation,
                                access_pattern,
                                io_size,
                                block_size,
                                data_space_size,
                                count
                            );

                        }
                    }
                }
            }
        }
    }
}

int test_latency(){

    uint64_t capacity = 1024ULL*1024ULL*1024ULL*20ULL;
    uint64_t alloc_size = 1024ULL*1024ULL*1024ULL*14ULL;

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
    bench_rw_latency("PMEM", pmem_data_space_ptr, alloc_size);


    // for (uint64_t i = 0; i < alloc_size; i++){
    //     dram_data_space_ptr[i] = 0;
    // }
    // bench_rw_latency("DRAM_cached", dram_data_space_ptr, alloc_size);

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