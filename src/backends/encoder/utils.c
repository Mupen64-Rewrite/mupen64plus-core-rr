#include "utils.h"
#include <stdint.h>
#include <string.h>

#include "api/m64p_types.h"
#include "osal/files.h"

/**
 * FNV1a, 64-bit, as specified here:
 * http://www.isthe.com/chongo/tech/comp/fnv/
 */
static uint64_t hash_fnv1a_64(const char* str) {
  uint64_t hash = UINT64_C(0xCBF29CE484222325);
  for (; *str != '\0'; str++) {
    hash ^= (uint64_t) str[0];
    hash *= UINT64_C(0x100000001B3);
  }
  return hash;
}
// found using https://md5calc.com/hash/fnv1a64
#define MP4_HASH UINT64_C(0x07D59719173C1EA4)
#define WEBM_HASH UINT64_C(0x3D7D19F619EA0314)

m64p_encoder_format infer_encode_format(const char *path)  {
  size_t len;
  const char* ext = NULL;
  
  // find where file name starts, and also find strlen
  for (len = 0; path[len] != '\0'; len++) {
    if (strchr(OSAL_DIR_SEPARATORS, path[len])) {
      ext = path + len + 1;
    }
  }
  // check if the last slash was found, or if path ends in slash
  if (ext == NULL || *ext == '\0') 
    return M64FMT_NULL;
  // see if the path has an extension
  ext = strchr(ext, '.');
  if (!ext)
    return M64FMT_NULL;

  // Compare extension to known ones (using hash for efficiency)
  switch (hash_fnv1a_64(ext)) {
    case MP4_HASH:
      return (strcmp(ext, "mp4") == 0)? M64FMT_MP4 : 0;
    case WEBM_HASH:
      return (strcmp(ext, "webm") == 0)? M64FMT_WEBM : 0;
    default:
      return M64FMT_NULL;
  }
}