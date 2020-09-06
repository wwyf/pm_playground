#include <libpmemobj.h>
#include <iostream>

PMEMobjpool * pop;

int main(){
    std::string pool_filename = "/mnt/pmem/pool";
    std::remove(pool_filename.c_str());
    uint64_t capacity = 1024ULL*1024ULL*1024ULL*240ULL;
    pop = pmemobj_create(pool_filename.c_str(), "TEST", capacity, 0066);
    PMEMoid data_space;
    // for (int i = 10; i < 20; i++){
    //     if (pmemobj_alloc(pop, &data_space, 1024ULL*1024ULL*1024ULL*i, 0,0,NULL)){
    //         std::cout << i << " GB " << "alloc error!!" << std::endl;
    //     } else {
    //         std::cout << i << " GB " << "alloc success!!" << std::endl;
    //     }
    //     pmemobj_free(&data_space);
    // }

    // struct pobj_alloc_class_desc alloc_class;
    // for (int i = 0; i < 1; i++){
    //     char name[50] = {0};
    //     sprintf(name, "heap.alloc_class.%d.desc", i);
    //     pmemobj_ctl_get(pop, name, &alloc_class); 
    //     printf("%s : \n", name);
    //     printf("alloc_class.unit_size : %lu\n", alloc_class.unit_size);
    //     printf("alloc_class.alignment : %lu\n", alloc_class.alignment);
    //     printf("alloc_class.units_per_block : %lu\n", alloc_class.units_per_block);
    //     printf("alloc_class.header_type : %lu\n", alloc_class.header_type);
    //     printf("alloc_class.class_id : %lu\n", alloc_class.class_id);

    // }
    // if (pmemobj_ctl_get(pop, "heap.alloc_class.128.desc", &alloc_class) == -1){
    //     printf("128 not existed\n");
    //     // alloc_class.unit_size = 1024;
    //     // alloc_class.units_per_block = 1024*1024*4;
    //     alloc_class.unit_size = 1024ULL*1024ULL*2ULL;
    //     alloc_class.units_per_block = 1000;
    //     alloc_class.alignment = 0;
    //     alloc_class.header_type = POBJ_HEADER_COMPACT;
    //     alloc_class.class_id = 128;
    //     // alloc_class.header_type = POBJ_HEADER_NONE;
    //     // alloc_class.header_type = POBJ_HEADER_LEGACY;
    //     if (pmemobj_ctl_set(pop, "heap.alloc_class.128.desc", &alloc_class) == -1){
    //         printf("pmemobj_ctl_set no success\n");
    //     };
    //     struct pobj_alloc_class_desc new_alloc_class;
    //     if(pmemobj_ctl_get(pop, "heap.alloc_class.128.desc", &new_alloc_class) == 0){
    //         printf("128\n");
    //         printf("new_alloc_class.unit_size : %lu\n", new_alloc_class.unit_size);
    //         printf("new_alloc_class.alignment : %lu\n", new_alloc_class.alignment);
    //         printf("new_alloc_class.units_per_block : %lu\n", new_alloc_class.units_per_block);
    //         printf("new_alloc_class.header_type : %lu\n", new_alloc_class.header_type);
    //         printf("new_alloc_class.class_id : %lu\n", new_alloc_class.class_id);
    //     } 
    // }
    // size_t growth = (1 << 20) * 2000;
    // auto ret = pmemobj_ctl_set(pop, "heap.size.granularity", &growth);
    // // for (int i = 8; i <= 196; i+=8){
    // for (int i = 14; i <= 14; i+=1){
    //     // for (int j = 0; j < 1; j++){
    //     //     // if (pmemobj_xalloc(pop, &data_space, 1024ULL*1024ULL*1024ULL*i,0, POBJ_CLASS_ID(j)|POBJ_XALLOC_ZERO ,0, NULL)){
    //     //     if (pmemobj_xalloc(pop, &data_space, 1024ULL*1024ULL*1024ULL*i,0, POBJ_CLASS_ID(j)|POBJ_XALLOC_ZERO ,0, NULL)){
    //     //         std::cout << "alloc class " << j << " " << i << " GB " << "xalloc error!!" << std::endl;
    //     //     } else {
    //     //         std::cout << "alloc class " << j << " " << i << " GB " << "xalloc success!!" << std::endl;
    //     //     }
    //     //     pmemobj_free(&data_space);
    //     // }
    //     // if (pmemobj_zalloc(pop, &data_space, 1024ULL*1024ULL*1024ULL*i,0)){
    //     if (pmemobj_xalloc(pop, &data_space, 1024ULL*1024ULL*1024ULL*i,0, POBJ_CLASS_ID(128)|POBJ_XALLOC_ZERO ,0, NULL)){
    //     if (pmemobj_xalloc(pop, &data_space, 1024ULL*1024ULL*1024ULL*i,0, POBJ_CLASS_ID(128)|POBJ_XALLOC_ZERO ,0, NULL)){
    //         std::cout << "alloc class" << 128 << " " << i << " GB " << "xalloc error!!" << std::endl;
    //     } else {
    //         std::cout << "alloc class" << 128 << " " << i << " GB " << "xalloc success!!" << std::endl;
    //     }
    //     pmemobj_free(&data_space);
    // }

    uint64_t offsets[16];
    // PMEMoid data_space;
    pmemobj_zalloc(pop, &data_space, 1024ULL*1024ULL*1024ULL*8,0);
    offsets[0] = data_space.off;
    for (int i = 1; i <= 8; i++){
        pmemobj_zalloc(pop, &data_space, 1024ULL*1024ULL*1024ULL*8,0);
        offsets[i] = data_space.off;
        std::cout << "cur off : " << data_space.off << std::endl;
        std::cout << "diff : " <<  offsets[i]-offsets[i-1] << std::endl;
        std::cout << "next  : " <<  offsets[i] + offsets[i]-offsets[i-1] << std::endl;
    }
    std::remove(pool_filename.c_str());

    return 0;
}