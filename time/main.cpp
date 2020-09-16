#include <cstdlib>
#include <iostream>

// https://stackoverflow.com/questions/12631856/difference-between-rdtscp-rdtsc-memory-and-cpuid-rdtsc
static inline uint64_t get_tsc(){
    uint64_t tsc;
    __asm__ __volatile__("rdtscp; "         // serializing read of tsc
                        "shl $32,%%rdx; "  // shift higher 32 bits stored in rdx up
                        "or %%rdx,%%rax"   // and or onto rax
                        : "=a"(tsc)        // output to tsc variable
                        :
                        : "%rcx", "%rdx"); // rcx and rdx are clobbered
    return tsc;
}

// https://www.cnblogs.com/cnmaizi/archive/2011/01/17/1937772.html
void access_counter(unsigned *hi, unsigned *lo)
{
        asm("rdtsc; movl %%edx, %0; movl %%eax, %1"
                        : "=r" (*hi), "=r" (*lo)
                        : /* No input */
                        : "%edx", "%eax");
        return;
}

static unsigned cyc_hi = 0;
static unsigned cyc_lo = 0;

uint64_t get_counter(void)
{
        unsigned int    ncyc_hi, ncyc_lo;
        unsigned int    hi, lo, borrow;
        uint64_t  result;
 
        /* Get cycle counter */
        access_counter(&ncyc_hi, &ncyc_lo);
 
        /* Do double precision subtraction */
        lo = ncyc_lo - cyc_lo;
        borrow = lo > ncyc_lo;
        hi = ncyc_hi - cyc_hi - borrow;
 
        result = (uint64_t)hi * (1 << 30) * 4 + lo;
        if (result < 0) {
                printf("Error: counter returns neg value: %.0f\n", result);
        }
        return result;
}

int main(){
    get_counter();
    auto start_ts = get_tsc();
    std::cout << start_ts << std::endl;
    std::cout << get_counter() << std::endl;
    start_ts = get_tsc();
    uint64_t a = 0;
    for (uint64_t i = 0; i < 3ULL*1000*1000*1000; i++){
        a++;
    }
    auto end_ts = get_tsc();
    std::cout << get_counter() << std::endl;
    std::cout << end_ts << std::endl;
    std::cout << end_ts - start_ts << std::endl;
    std::cout << a << std::endl;
}
