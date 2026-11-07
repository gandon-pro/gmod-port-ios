#pragma once
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Returns malloc-owned, straight-alpha RGBA8 pixels in top-to-bottom row order. */
int GMPImage_DecodeRGBA(const void *bytes,size_t count,void **pixels,int *width,int *height);
#ifdef __cplusplus
}
#endif
