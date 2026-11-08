#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* read() returns a malloc-owned buffer. Called on UIKit's main thread. */
typedef int (*GMPHTML_ReadAsset)(const char *,void **,size_t *);
void GMPHTML_SetAssetReader(GMPHTML_ReadAsset read);
uint64_t GMPHTML_Create(const char *url);
void GMPHTML_Destroy(uint64_t id);
/* Invalidate handles synchronously, dispose UIKit views on the main queue. */
void GMPHTML_Shutdown(void);
void GMPHTML_Show(uint64_t id,int visible);
/* Normalized game viewport coordinates; no dependency on render resolution. */
void GMPHTML_Bounds(uint64_t id,double x,double y,double width,double height);
int GMPHTML_ImageSize(const void *bytes,size_t size,int *width,int *height);
void GMPHTML_BackgroundFrame(uint64_t id,const char *json,int width,int height);
void GMPHTML_Viewport(uint64_t id,int width,int height);
void GMPHTML_Reply(uint64_t id,uint64_t request,const char *json);
void GMPHTML_Notify(uint64_t id,const char *message);
void GMPHTML_OpenURL(const char *url);
void GMPHTML_Call(uint64_t id,const char *javascript);
void GMPHTML_AddFunction(uint64_t id,const char *object,const char *function);
/* 1 = complete message, 0 = none, -1 = destination too small (message retained). */
int GMPHTML_Poll(uint64_t id,char *name,size_t nameSize,char *json,size_t jsonSize);
#ifdef __cplusplus
}
#endif
