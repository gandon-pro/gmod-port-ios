#include "gmp_ui_model.h"
#include <cassert>
#include <limits>
#include <initializer_list>
#include <utility>
int main(){
 assert(gmpui::Matches(" BARREL blue ","Blue barrel","models/props/barrel.mdl"));
 assert(gmpui::Matches("props barrel","Blue barrel","models/props/barrel.mdl"));
 assert(!gmpui::Matches("blue chair","Blue barrel","barrel.mdl"));assert(gmpui::Matches(" \t ","",""));
 double value=0;assert(gmpui::ParseNumber("-23.5",-10,10,value)&&value==-10);
 for(auto text:{"nan","inf","1x","","--2","1;quit"})assert(!gmpui::ParseNumber(text,-100,100,value));
 assert(gmpui::SliderValue(500,-10,10)==0);assert(gmpui::SliderValue(2000,0,3)==3);
 assert(gmpui::SliderPosition(std::numeric_limits<double>::quiet_NaN(),0,1)==0);
 for(auto size:{std::pair<int,int>(667,375),{844,390},{1334,750},{1792,828},{2048,1536}}){
  gmpui::Layout l(size.first,size.second,true,1);
  assert(l.x>=0&&l.y>=0&&l.left>l.tree&&l.tools>0&&l.height>l.tab+l.search);
  assert(l.x+l.left+l.split+l.tools<=size.first);assert(l.y+l.height<=size.second);
 }
}
