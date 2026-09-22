#include "hwy.hpp"
#include <hwy/contrib/sort/vqsort.h>

namespace floormat {

void vqsort(uint64_t* keys, uint32_t n) { hwy::VQSort(keys, n, hwy::SortAscending()); }

} // namespace floormat
