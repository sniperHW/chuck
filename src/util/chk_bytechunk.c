#include "util/chk_bytechunk.h"

#ifdef USE_BUFFER_POOL

chk_bytebuffer_pool *bytebuffer_pool = NULL;

int32_t lock_bytebuffer_pool = 0;

#endif