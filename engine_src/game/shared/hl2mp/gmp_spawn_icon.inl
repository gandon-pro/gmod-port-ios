#include "gmp_image.h"
#include "gmp_icon_label.h"
#include "vgui_controls/Label.h"
#include <cstring>
static int GMP_LoadPNGTexture(const char *path)
{
    CUtlBuffer data;void *pixels=NULL;int w=0,h=0;
    if(!filesystem->ReadFile(path,"GAME",data)||!GMPImage_DecodeRGBA(data.Base(),data.TellPut(),&pixels,&w,&h))return 0;
    int texture=vgui::surface()->CreateNewTextureID(true);
    vgui::surface()->DrawSetTextureRGBA(texture,static_cast<unsigned char *>(pixels),w,h,1,true);free(pixels);
    return texture;
}
// One cell owns its image and caption. Shared frame textures are owned by the grid.
class CGMPSpawnIconPanel : public CMDLPanel
{
    DECLARE_CLASS_SIMPLE(CGMPSpawnIconPanel,CMDLPanel);
public:
    CGMPSpawnIconPanel(vgui::Panel *parent):BaseClass(parent,"SpawnIcon"),m_Texture(0),m_Font(0),m_Prop(true),m_Normal(0),m_Hover(0),m_PropHover(0){
        m_Text[0]=0;
        m_Caption=new vgui::Label(this,"Caption","");
        m_Caption->SetProportional(false);m_Caption->SetMouseInputEnabled(false);m_Caption->SetKeyBoardInputEnabled(false);
        m_Caption->SetPaintBackgroundEnabled(false);m_Caption->SetPaintBorderEnabled(false);
        m_Caption->SetContentAlignment(vgui::Label::a_center);m_Caption->SetTextInset(0,0);m_Caption->SetVisible(false);
    }
    ~CGMPSpawnIconPanel(){ClearImage();}
    void ClearImage(){if(m_Texture)vgui::surface()->DestroyTextureID(m_Texture);m_Texture=0;}
    bool LoadImage(const char *path){ClearImage();m_Texture=GMP_LoadPNGTexture(path);return m_Texture!=0;}
    void ConfigureCaption(vgui::HFont font,const char *text,bool prop,int normal,int hover,int propHover){
        m_Font=font;m_Prop=prop;m_Normal=normal;m_Hover=hover;m_PropHover=propHover;
        g_pVGuiLocalize->ConvertANSIToUnicode(text,m_Text,sizeof(m_Text));
        LayoutCaption();
    }
    void PerformLayout() override {BaseClass::PerformLayout();LayoutCaption();}
    void Paint() override {
        if(m_Texture){
            const int inset=m_Prop?0:4;
            vgui::surface()->DrawSetColor(255,255,255,255);vgui::surface()->DrawSetTexture(m_Texture);
            vgui::surface()->DrawTexturedRect(inset,inset,GetWide()-inset,GetTall()-inset);
        }else BaseClass::Paint();
        int mx,my;vgui::input()->GetCursorPos(mx,my);ScreenToLocal(mx,my);
        const bool hovered=mx>=0&&my>=0&&mx<GetWide()&&my<GetTall();
        const int texture=m_Prop?(hovered?m_PropHover:0):(hovered&&m_Hover?m_Hover:m_Normal);
        if(texture){
            vgui::surface()->DrawSetColor(255,255,255,255);vgui::surface()->DrawSetTexture(texture);
            if(m_Prop){
                int x[]={0,5,GetWide()-5,GetWide()},y[]={0,5,GetTall()-5,GetTall()};
                float uv[]={4.f/64,9.f/64,55.f/64,60.f/64};
                for(int row=0;row<3;++row)for(int col=0;col<3;++col)
                    vgui::surface()->DrawTexturedSubRect(x[col],y[row],x[col+1],y[row+1],uv[col],uv[row],uv[col+1],uv[row+1]);
            }else vgui::surface()->DrawTexturedRect(0,0,GetWide(),GetTall());
        }
        // Native Label paints after this cell, with its own local origin and clip rectangle.
        if(m_Font)m_Caption->SetFont(m_Font);
        m_Caption->SetFgColor(texture?Color(255,255,255,255):Color(40,40,40,255));
    }
private:
    void LayoutCaption(){
        if(!m_Font){m_Caption->SetVisible(false);return;}
        const GMPIconCaptionRect rect=GMP_IconCaptionRect(GetWide(),GetTall(),vgui::surface()->GetFontTall(m_Font));
        wchar_t fitted[128];std::memcpy(fitted,m_Text,sizeof(fitted));
        GMP_FitIconLabel(fitted,ARRAYSIZE(fitted),rect.w,[&](wchar_t c){return vgui::surface()->GetCharacterWidth(m_Font,c);});
        m_Caption->SetFont(m_Font);m_Caption->SetText(fitted);m_Caption->SetBounds(rect.x,rect.y,rect.w,rect.h);
        m_Caption->SetVisible(!m_Prop&&fitted[0]&&rect.w>0&&rect.h>0);
    }
    int m_Texture;vgui::HFont m_Font;bool m_Prop;int m_Normal,m_Hover,m_PropHover;
    wchar_t m_Text[128];vgui::Label *m_Caption;
};
