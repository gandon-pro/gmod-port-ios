#pragma once
#include <cstddef>
// Pinned derma/init.lua: DejaVu Sans 14 on POSIX. ContentIcon is 128px,
// so captions must not scale with the fullscreen HUD/list font.
constexpr int GMP_CONTENT_CAPTION_FONT_TALL=14;
struct GMPIconCaptionRect {int x,y,w,h;};
inline GMPIconCaptionRect GMP_IconCaptionRect(int width,int height,int fontTall){
    const int margin=width<20?width/2:10;
    const int room=height>9?height-9:0;
    const int tall=fontTall<room?fontTall:room;
    return {margin,room-tall,width-2*margin,tall};
}
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
