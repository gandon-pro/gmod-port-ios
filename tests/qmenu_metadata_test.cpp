#include <cassert>
#include <cstring>
#include <set>
#include <string>
#include <algorithm>
#ifndef _WIN32
#include <strings.h>
#endif
#include "gmp_ui_model.h"
#define ARRAYSIZE(x) (sizeof(x)/sizeof((x)[0]))
#ifdef _WIN32
#define Q_stricmp _stricmp
#else
#define Q_stricmp strcasecmp
#endif
#define MIN std::min
#define MAX_PATH 260
static void Q_strncpy(char *dst,const char *src,int n){if(n>0){std::strncpy(dst,src,n-1);dst[n-1]=0;}}
static void Q_FileBase(const char *src,char *dst,int n){const char *base=std::strrchr(src,'/');base=base?base+1:src;std::string value=base;size_t dot=value.rfind('.');if(dot!=std::string::npos)value.resize(dot);Q_strncpy(dst,value.c_str(),n); }
#define Q_strcmp strcmp
#include "gmp_tooldefs.inl"
int main(){
 std::set<std::string> modes;
 for(const auto &tool:s_GMPToolDefs){
  assert(tool.mode&&*tool.mode&&tool.name&&*tool.name&&modes.insert(tool.mode).second);
  assert(!strcmp(tool.category,"Constraints")||!strcmp(tool.category,"Construction")||!strcmp(tool.category,"Render"));
  std::set<std::string> names;
  for(int i=0;i<tool.numVars;i++){
   const auto &v=tool.vars[i];assert(v.name&&*v.name&&v.def&&v.label&&names.insert(v.name).second);
   assert(strchr("nbkst",v.kind));
   if(v.kind=='n'){double value;assert(v.max>v.min&&gmpui::ParseNumber(v.def,v.min,v.max,value));assert(value==atof(v.def));}
   if(v.kind=='b')assert(!strcmp(v.def,"0")||!strcmp(v.def,"1"));
   if(v.kind=='s'){
    assert(v.choices&&strlen(v.choices)<4096);bool found=false;std::string list=v.choices;size_t at=0;
    while(at<=list.size()){size_t end=list.find('|',at);if(end==std::string::npos)end=list.size();std::string item=list.substr(at,end-at);size_t eq=item.find('=');std::string value=item.substr(0,eq);if(value==v.def)found=true;if(end==list.size())break;at=end+1;}
    assert(found);
   }
  }
 }
 assert(modes.size()==29);
}
