#pragma once
#include <cstddef>
// Trim by glyph advance, keeping every caption within its own card.
template<class Width>void GMP_FitIconLabel(wchar_t *text,std::size_t capacity,int maxWidth,Width width)
{
    if(!text||!capacity)return;
    text[capacity-1]=0;int total=0;std::size_t size=0;
    while(size+1<capacity&&text[size]){total+=width(text[size]);++size;}
    if(total<=maxWidth)return;
    const int dots=width(L'.')*3;
    if(dots>maxWidth||capacity<4){text[0]=0;return;}
    int used=0;std::size_t keep=0;
    while(keep<size&&keep+4<=capacity&&used+width(text[keep])+dots<=maxWidth){used+=width(text[keep]);++keep;}
    text[keep++]=L'.';text[keep++]=L'.';text[keep++]=L'.';text[keep]=0;
}
