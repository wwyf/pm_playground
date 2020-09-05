#include <libpmemobj.h>
#include <iostream>

PMEMobjpool * pop;

int main(){
    std::string pool_filename = "/mnt/pmem/pool";
    uint64_t capacity = 1024ULL*1024ULL*1024ULL*96ULL;
    pop = pmemobj_create(pool_filename.c_str(), "TEST_WRITE", capacity, 0066);
    PMEMoid data_space;
    if (pmemobj_alloc(pop, &data_space, 1024ULL*1024ULL*1024ULL*2, 0,0,NULL)){
        std::cout << "alloc error!" << std::endl;
    }

    std::remove(pool_filename.c_str());

    return 0;
}