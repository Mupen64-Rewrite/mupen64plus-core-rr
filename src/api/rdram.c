#include <stdint.h>
#include "api/m64p_types.h"
#include "device/r4300/r4300_core.h"
#include "main/main.h"
#define M64P_RDRAM_PROTOTYPES
#include "m64p_rdram.h"

m64p_error RDRAM_ReadAligned(uint32_t addr, uint32_t* value) {
    if (value == NULL)
        return M64ERR_INPUT_ASSERT;
    if ((addr & 0x3) != 0)
        return M64ERR_INPUT_INVALID;
    
    struct device* const dev = &g_dev;
    
    if (!r4300_read_aligned_word(&dev->r4300, addr, value))
        return M64ERR_INTERNAL;
    return M64ERR_SUCCESS;
}
m64p_error RDRAM_WriteAligned(uint32_t addr, uint32_t value, uint32_t mask) {
    if ((addr & 0x3) != 0)
        return M64ERR_INPUT_INVALID;
    
    
    struct device* const dev = &g_dev;
    
    if (!r4300_write_aligned_word(&dev->r4300, addr, value, mask))
        return M64ERR_INTERNAL;
    return M64ERR_SUCCESS;
}