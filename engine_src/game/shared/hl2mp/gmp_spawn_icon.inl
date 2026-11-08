#include "gmp_image.h"
static int GMP_LoadPNGTexture(const char *path)
{
    CUtlBuffer data;void *pixels=NULL;int w=0,h=0;
    if(!filesystem->ReadFile(path,"GAME",data)||!GMPImage_DecodeRGBA(data.Base(),data.TellPut(),&pixels,&w,&h))return 0;
    int texture=vgui::surface()->CreateNewTextureID(true);
    vgui::surface()->DrawSetTextureRGBA(texture,static_cast<unsigned char *>(pixels),w,h,1,true);free(pixels);
    return texture;
}
// Pool owns both the model and decoded PNG: scrolling never keeps unbounded GPU textures.
class CGMPSpawnIconPanel : public CMDLPanel
{
    DECLARE_CLASS_SIMPLE(CGMPSpawnIconPanel,CMDLPanel);
public:
    CGMPSpawnIconPanel(vgui::Panel *parent):BaseClass(parent,"SpawnIcon"),m_Texture(0){}
    ~CGMPSpawnIconPanel(){ClearImage();}
    void ClearImage(){if(m_Texture)vgui::surface()->DestroyTextureID(m_Texture);m_Texture=0;}
    bool LoadImage(const char *path){
        ClearImage();m_Texture=GMP_LoadPNGTexture(path);return m_Texture!=0;
    }

    void Paint() override {
        if(!m_Texture){BaseClass::Paint();return;}
        vgui::surface()->DrawSetColor(255,255,255,255);vgui::surface()->DrawSetTexture(m_Texture);
        vgui::surface()->DrawTexturedRect(0,0,GetWide(),GetTall());
    }
private:
    int m_Texture;
};
