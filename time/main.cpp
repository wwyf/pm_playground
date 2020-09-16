#include <cstdlib>
#include <iostream>
#include <chrono>

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

static inline uint64_t run_some_code(uint64_t input){
    // 24~26 cycle
    uint64_t var = input;
    uint64_t ret = 0;
    __asm__ __volatile__(
                        "mov  %0, %%rax;"
                        "inc %%rax;"
                        "mov  %%rax, %1;"
                        : "=r"(ret) 
                        : "r"(var)
                        :"%rax");
    return ret;
}

static inline void run_code(){
    run_some_code(run_some_code(5));
}




int main(){
    const uint64_t count = 1000ULL*1000ULL;
    auto start_time = std::chrono::high_resolution_clock::now();
    auto end_time = std::chrono::high_resolution_clock::now();
    auto temp_start_time = std::chrono::high_resolution_clock::now();
    auto temp_end_time = std::chrono::high_resolution_clock::now();

    uint64_t start_tsc, end_tsc;
    uint64_t temp_start_tsc, temp_end_tsc;

    std::cout << "get tsc overhead" << std::endl;

    start_tsc = get_start_tsc();
    for (uint64_t i = 0; i < count; i++){
        temp_start_tsc = get_start_tsc();
        temp_end_tsc = get_end_tsc();
    }
    end_tsc = get_end_tsc();
    std::cout << (end_tsc-start_tsc)/count << " cycles" << std::endl;

    start_time = std::chrono::high_resolution_clock::now();
    for (uint64_t i = 0; i < count; i++){
        temp_start_tsc = get_start_tsc();
        temp_end_tsc = get_end_tsc();
    }
    end_time = std::chrono::high_resolution_clock::now();
    std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count()/count << " ns" << std::endl;

    std::cout << "chrono clock  overhead" << std::endl;

    start_tsc = get_start_tsc();
    for (uint64_t i = 0; i < count; i++){
        temp_start_time = std::chrono::high_resolution_clock::now();
        temp_end_time = std::chrono::high_resolution_clock::now();
    }
    end_tsc = get_end_tsc();
    std::cout << (end_tsc-start_tsc)/count << " cycles" << std::endl;

    start_time = std::chrono::high_resolution_clock::now();
    for (uint64_t i = 0; i < count; i++){
        temp_start_time = std::chrono::high_resolution_clock::now();
        temp_end_time = std::chrono::high_resolution_clock::now();
    }
    end_time = std::chrono::high_resolution_clock::now();
    std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count()/count << " ns" << std::endl;

    // std::cout << "time code " << std::endl;

    // uint64_t temp_var = 0;
    // const uint64_t temp_count = 1ULL;
    // start_tsc = get_start_tsc();
    // run_code();
    // end_tsc = get_end_tsc();
    // std::cout << (end_tsc-start_tsc) << " cycles" << std::endl;

    // start_time = std::chrono::high_resolution_clock::now();
    // run_code();
    // end_time = std::chrono::high_resolution_clock::now();
    // std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count() << " ns" << std::endl;



}
