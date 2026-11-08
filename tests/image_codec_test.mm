#include "../engine_src/public/gmp_image.h"
#include <cassert>
#include <cstdlib>
int main(){
 const unsigned char png[]={137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,2,0,0,0,2,8,6,0,0,0,114,182,13,36,0,0,0,19,73,68,65,84,120,156,99,248,207,192,240,31,12,129,52,8,52,0,0,73,73,9,120,40,160,219,119,0,0,0,0,73,69,78,68,174,66,96,130};
 void *pixels=nullptr;int w=0,h=0;
 assert(GMPImage_DecodeRGBA(png,sizeof(png),&pixels,&w,&h)&&w==2&&h==2);
 const unsigned char *p=static_cast<unsigned char *>(pixels);
 assert(p[0]==255&&p[1]==0&&p[2]==0&&p[3]==255);
 assert(p[4]==0&&p[5]==255&&p[6]==0&&p[7]==255);
 assert(p[8]==0&&p[9]==0&&p[10]==255&&p[11]==255);
 assert(p[12]>=254&&p[13]>=254&&p[14]>=254&&p[15]==128);
 free(pixels);assert(!GMPImage_DecodeRGBA("x",1,&pixels,&w,&h)&&!pixels);
}
