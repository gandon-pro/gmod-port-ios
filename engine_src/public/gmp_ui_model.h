#pragma once
#include <cmath>
#include <cctype>
#include <cstdlib>
#include <cstring>
namespace gmpui {
inline float SliderValue(int p,float low,float high){return low+(high-low)*(p<0?0:p>1000?1000:p)/1000.0f;}
inline int SliderPosition(double n,float low,float high){if(!std::isfinite(n)||high<=low)return 0;double p=(n-low)/(high-low);return p<=0?0:p>=1?1000:static_cast<int>(p*1000+0.5);}
inline bool ParseNumber(const char *text,double low,double high,double &out){char *end=nullptr;double n=std::strtod(text,&end);if(end==text||*end||!std::isfinite(n))return false;out=n<low?low:n>high?high:n;return true;}
inline bool Contains(const char *hay,const char *needle,int size){
 for(;*hay;++hay){int i=0;while(i<size&&hay[i]&&std::tolower(static_cast<unsigned char>(hay[i]))==std::tolower(static_cast<unsigned char>(needle[i])))++i;if(i==size)return true;}return size==0;
}
inline bool Matches(const char *query,const char *label,const char *model){
 while(*query){while(*query&&std::isspace(static_cast<unsigned char>(*query)))++query;const char *begin=query;while(*query&&!std::isspace(static_cast<unsigned char>(*query)))++query;int n=static_cast<int>(query-begin);if(n&&!Contains(label,begin,n)&&!Contains(model,begin,n))return false;}return true;
}
struct Layout {
 int x,y,width,height,tab,tools,split,left,tree,search;
 Layout(int w,int h,bool safe,float userScale){
  float scale=std::fmax(.75f,std::fmin(2.0f,h/768.0f*userScale));
  int marginX=safe?w*6/100:((w>=1024&&h>=768)?static_cast<int>((w-1024)*.1f):0);
  int marginY=safe?h/30:((w>=1024&&h>=768)?static_cast<int>((h-768)*.1f):0);
  x=marginX;y=marginY;width=w-marginX*2;height=h-marginY-(safe?h/20:marginY);
  tab=static_cast<int>(28*scale);search=static_cast<int>(28*scale);split=static_cast<int>(6*scale);
  tools=static_cast<int>((w>=1024?460:300)*scale);
  tools=tools>width*3/5?width*3/5:tools;left=width-tools-split;
  tree=static_cast<int>(192*scale);if(tree>left/3)tree=left/3;
 }
};
}
