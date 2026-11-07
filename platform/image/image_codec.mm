#import <Foundation/Foundation.h>
#import <ImageIO/ImageIO.h>
#include <cstdlib>

#include "../../engine_src/public/gmp_image.h"
extern "C" int GMPImage_DecodeRGBA(const void *bytes,size_t count,void **pixels,int *width,int *height){
    if(!bytes||!count||!pixels||!width||!height)return 0;
    *pixels=nullptr;*width=*height=0;
    NSData *data=[NSData dataWithBytesNoCopy:const_cast<void *>(bytes) length:count freeWhenDone:NO];
    CGImageSourceRef source=CGImageSourceCreateWithData((__bridge CFDataRef)data,nullptr);if(!source)return 0;
    CGImageRef image=CGImageSourceCreateImageAtIndex(source,0,nullptr);CFRelease(source);if(!image)return 0;
    size_t w=CGImageGetWidth(image),h=CGImageGetHeight(image);
    if(!w||!h||w>4096||h>4096){CGImageRelease(image);return 0;}
    unsigned char *rgba=static_cast<unsigned char *>(calloc(w*h,4));if(!rgba){CGImageRelease(image);return 0;}
    CGColorSpaceRef color=CGColorSpaceCreateDeviceRGB();
    CGContextRef context=CGBitmapContextCreate(rgba,w,h,8,w*4,color,kCGImageAlphaPremultipliedLast|kCGBitmapByteOrder32Big);
    CGColorSpaceRelease(color);if(!context){free(rgba);CGImageRelease(image);return 0;}
    CGContextDrawImage(context,CGRectMake(0,0,w,h),image);CGContextRelease(context);CGImageRelease(image);
    // VGUI expects straight RGBA, whereas CoreGraphics produces premultiplied RGBA.
    for(size_t i=0;i<w*h;i++){unsigned a=rgba[i*4+3];if(a&&a<255)for(int c=0;c<3;c++)rgba[i*4+c]=static_cast<unsigned char>((unsigned(rgba[i*4+c])*255+a/2)/a);}
    *pixels=rgba;*width=static_cast<int>(w);*height=static_cast<int>(h);return 1;
}
