#ifndef __ROTATE_DEFS_H
#define __ROTATE_DEFS_H

#include <stdint.h>

#define U8V(v) ((uint8_t)(v) & 0xFFU)
#define U16V(v) ((uint16_t)(v) & 0xFFFFU)
#define U32V(v) ((uint32_t)(v) & 0xFFFFFFFFU)
#define U64V(v) ((uint64_t)(v) & 0xFFFFFFFFFFFFFFFFU)

#define ROTL32(v, n) \
  (U32V((uint32_t)(v) << (n)) | ((uint32_t)(v) >> (32 - (n))))

#define ROTL64(v, n) \
  (U64V((uint64_t)(v) << (n)) | ((uint64_t)(v) >> (64 - (n))))

#define ROTR32(v, n) ROTL32(v, 32 - (n))
#define ROTR64(v, n) ROTL64(v, 64 - (n))

#define ROTL8(v, n) \
  (U8V((uint8_t)(v) << (n)) | ((uint8_t)(v) >> (8 - (n))))

#define ROTL16(v, n) \
  (U16V((uint16_t)(v) << (n)) | ((uint16_t)(v) >> (16 - (n))))

#define ROTR8(v, n) ROTL8(v, 8 - (n))
#define ROTR16(v, n) ROTL16(v, 16 - (n))

#endif
