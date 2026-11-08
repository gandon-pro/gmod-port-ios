#include "gmp_icon_label.h"
#include "gmp_local_files.h"
#include <cassert>
#include <cwchar>
#include <string>
int main(){
 auto width=[](wchar_t c){return c==L'W'?14:c==L'.'?3:7;};
 wchar_t title[128]=L"A very long weapon title that must not spill";
 GMP_FitIconLabel(title,128,64,width);int size=0;for(wchar_t c:std::wstring(title))size+=width(c);assert(size<=64);assert(std::wstring(title).substr(wcslen(title)-3)==L"...");
 wchar_t small[3]=L"WW";GMP_FitIconLabel(small,3,2,width);assert(!small[0]);
 wchar_t fits[20]=L"Tool Gun";GMP_FitIconLabel(fits,20,100,width);assert(std::wstring(fits)==L"Tool Gun");
 assert(gmp::LocalFile("dupes/my build.dupe","dupes",".dupe"));
 for(const char *bad:{"dupes/../cfg/config.cfg","dupes/sub/build.dupe","dupes/a.dupe;quit","dupes/a.dupe\n","/dupes/a.dupe","dupes/a.dupe.bak","dupes/.dupe","dupes/..x.dupe","dupes/a\\b.dupe"})assert(!gmp::LocalFile(bad,"dupes",".dupe"));
 assert(!gmp::LocalFile(std::string("dupes/a.dupe\0b",14),"dupes",".dupe"));
 assert(gmp::LocalDeletable("demos/recording_1.dem"));assert(gmp::LocalDeletable("dupes/build.jpg"));assert(!gmp::LocalDeletable("cfg/config.cfg"));
}
