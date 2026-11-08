// Runs Facepunch's pinned cl_viewscreen.lua against a real 256x256 material RT.
#include "igamesystem.h"
#include "VGuiMatSurface/IMatSystemSurface.h"
#include "materialsystem/MaterialSystemUtil.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/itexture.h"
#include <map>
#include <string>

static std::map<std::string,std::string> s_GMPToolLanguage;
static void GMP_LoadToolLanguage(const char *language)
{
    char path[192]; Q_snprintf(path,sizeof(path),"resource/localization/%s/tool.properties",language);
    CUtlBuffer data;
    if(!filesystem->ReadFile(path,"GAME",data))return;
    std::string text(static_cast<const char *>(data.Base()),data.TellPut());
    size_t start=0;
    while(start<text.size()){
        size_t end=text.find('\n',start); if(end==std::string::npos)end=text.size();
        std::string line=text.substr(start,end-start);start=end+1;
        if(!line.empty()&&line.back()=='\r')line.pop_back();
        size_t equals=line.find('=');
        if(equals!=std::string::npos&&line.compare(0,5,"tool.")==0)s_GMPToolLanguage[line.substr(0,equals)]=line.substr(equals+1);
    }
}
static const char *GMP_ToolPhrase(const char *key)
{
    auto found=s_GMPToolLanguage.find(key[0]=='#'?key+1:key);
    return found==s_GMPToolLanguage.end()?key:found->second.c_str();
}
class CGMPToolScreenPanel : public vgui::Panel
{
public:
    CGMPToolScreenPanel():vgui::Panel(NULL,"GModToolgunScreen"),m_Font(0),m_Background(0),m_Ready(false){
        SetBounds(0,0,256,256);SetMouseInputEnabled(false);SetKeyBoardInputEnabled(false);
        SetPaintBackgroundEnabled(false);SetPaintBorderEnabled(false);
    }
    bool Start(){
        glua::RuntimeConfig config{glua::REALM_CLIENT,[](const std::string &path,std::string &out){
            CUtlBuffer data;std::string name=path.compare(0,10,"gamemodes/")==0?path:"lua/"+path;
            if(!filesystem->ReadFile(name.c_str(),"GAME",data))return false;
            out.assign(static_cast<const char *>(data.Base()),data.TellPut());return true;
        },[](const std::string &s){Warning("Tool screen: %s\n",s.c_str());},[this](lua_State *L){
            lua_newtable(L);lua_pushlightuserdata(L,this);lua_pushcclosure(L,Invoke,1);lua_setfield(L,-2,"Invoke");lua_setglobal(L,"toolscreen");
        }};
        m_VM.reset(new glua::Runtime(config));m_Ready=m_VM->File("port/tool_screen.lua");return m_Ready;
    }
    ~CGMPToolScreenPanel(){if(m_Background)vgui::surface()->DestroyTextureID(m_Background);}
    void Paint() override {
        if(!m_Ready)return;
        lua_State *L=m_VM->State();int top=lua_gettop(L);
        lua_getglobal(L,"toolscreen");lua_getfield(L,-1,"Paint");
        if(lua_pcall(L,0,0,0)){Warning("Tool screen paint failed: %s\n",lua_tostring(L,-1));m_Ready=false;}
        lua_settop(L,top);
    }
private:
    static int Invoke(lua_State *L){
        CGMPToolScreenPanel *self=static_cast<CGMPToolScreenPanel *>(lua_touserdata(L,lua_upvalueindex(1)));
        const char *op=luaL_checkstring(L,1);vgui::ISurface *s=vgui::surface();
        if(!strcmp(op,"Mode")){lua_pushstring(L,gmod_toolmode.GetString());return 1;}
        if(!strcmp(op,"Time")){lua_pushnumber(L,Plat_FloatTime());return 1;}
        if(!strcmp(op,"GetTextureID")){
            const char *name=luaL_checkstring(L,2);
            if(strcmp(name,"models/weapons/v_toolgun/screen_bg"))return luaL_error(L,"Unsupported tool screen texture");
            self->m_Background=s->CreateNewTextureID();s->DrawSetTextureFile(self->m_Background,name,1,false);
            lua_pushinteger(L,self->m_Background);return 1;
        }
        if(!strcmp(op,"CreateFont")){
            luaL_checktype(L,3,LUA_TTABLE);
            lua_getfield(L,3,"font");std::string family=luaL_checkstring(L,-1);lua_pop(L,1);
            lua_getfield(L,3,"size");int size=luaL_checkinteger(L,-1);lua_pop(L,1);
            lua_getfield(L,3,"weight");int weight=luaL_checkinteger(L,-1);lua_pop(L,1);
            self->m_Font=s->CreateFont();
            if(!s->SetFontGlyphSet(self->m_Font,family.c_str(),size,weight,0,0,vgui::ISurface::FONTFLAG_ANTIALIAS))return luaL_error(L,"Tool screen font unavailable: %s",family.c_str());
            return 0;
        }
        if(!strcmp(op,"SetFont")){s->DrawSetTextFont(self->m_Font);return 0;}
        if(!strcmp(op,"DrawText")||!strcmp(op,"GetTextSize")){
            wchar_t wide[1024];g_pVGuiLocalize->ConvertANSIToUnicode(GMP_ToolPhrase(luaL_checkstring(L,2)),wide,sizeof(wide));
            if(!strcmp(op,"GetTextSize")){int w,h;s->GetTextSize(self->m_Font,wide,w,h);lua_pushinteger(L,w);lua_pushinteger(L,h);return 2;}
            s->DrawPrintText(wide,wcslen(wide));return 0;
        }
        if(!strcmp(op,"SetTextPos")){s->DrawSetTextPos(luaL_checknumber(L,2),luaL_checknumber(L,3));return 0;}
        if(!strcmp(op,"SetTexture")){s->DrawSetTexture(luaL_checkinteger(L,2));return 0;}
        if(!strcmp(op,"SetTextColor")||!strcmp(op,"SetDrawColor")){
            Color c(luaL_checkinteger(L,2),luaL_checkinteger(L,3),luaL_checkinteger(L,4),luaL_checkinteger(L,5));
            if(!strcmp(op,"SetTextColor"))s->DrawSetTextColor(c);else s->DrawSetColor(c);return 0;
        }
        if(!strcmp(op,"DrawTexturedRect")){int x=luaL_checknumber(L,2),y=luaL_checknumber(L,3);s->DrawTexturedRect(x,y,x+luaL_checknumber(L,4),y+luaL_checknumber(L,5));return 0;}
        return luaL_error(L,"Unknown tool screen operation: %s",op);
    }
    vgui::HFont m_Font;int m_Background;bool m_Ready;std::unique_ptr<glua::Runtime> m_VM;
};
class CGMPToolScreenSystem : public CAutoGameSystemPerFrame
{
public:
    CGMPToolScreenSystem():CAutoGameSystemPerFrame("GModToolgunScreen"),m_Panel(NULL),m_Attempted(false),m_IconsAttempted(false),m_Gradient(0){for(int i=0;i<3;++i)m_Icons[i]=0;}
    void LevelInitPostEntity() override {
        s_GMPToolLanguage.clear();GMP_LoadToolLanguage("en");ConVarRef lang("gmod_language");
        if(lang.IsValid()&&Q_strcmp(lang.GetString(),"en")&&!strstr(lang.GetString(),"..")&&!strchr(lang.GetString(),'/'))GMP_LoadToolLanguage(lang.GetString());
    }
    int Gradient(){
        if(!m_Gradient){m_Gradient=vgui::surface()->CreateNewTextureID();vgui::surface()->DrawSetTextureFile(m_Gradient,"gui/gradient",1,false);}
        return m_Gradient;
    }
    int ButtonIcon(int index){
        if(!m_IconsAttempted){
            m_IconsAttempted=true;
            const char *paths[]={"materials/gui/lmb.png","materials/gui/rmb.png","materials/gui/r.png"};
            for(int i=0;i<3;++i)m_Icons[i]=GMP_LoadPNGTexture(paths[i]);
        }
        return index>=0&&index<3?m_Icons[index]:0;
    }
    void Shutdown() override {Release();}
    void LevelShutdownPostEntity() override {Release();}
    void PreRender() override {
        C_BasePlayer *p=C_BasePlayer::GetLocalPlayer();C_BaseCombatWeapon *w=p?p->GetActiveWeapon():NULL;
        if(!w||Q_strcmp(w->GetClassname(),"gmod_tool"))return;
        if(!m_Attempted){m_Attempted=true;Start();}
        if(!m_Panel||!m_Target.IsValid())return;
        CMatRenderContextPtr ctx(materials);
        ctx->PushRenderTargetAndViewport(m_Target,0,0,256,256);
        ctx->MatrixMode(MATERIAL_PROJECTION);ctx->PushMatrix();ctx->LoadIdentity();ctx->Ortho(0,0,256,256,-1,1);
        ctx->MatrixMode(MATERIAL_VIEW);ctx->PushMatrix();ctx->LoadIdentity();
        VMatrix panelToWorld;panelToWorld.Identity();panelToWorld[1][1]=-1;
        static_cast<IMatSystemSurface *>(vgui::surface())->DrawPanelIn3DSpace(m_Panel->GetVPanel(),panelToWorld,256,256,256,256);
        ctx->MatrixMode(MATERIAL_VIEW);ctx->PopMatrix();ctx->MatrixMode(MATERIAL_PROJECTION);ctx->PopMatrix();ctx->MatrixMode(MATERIAL_MODEL);
        ctx->PopRenderTargetAndViewport();
    }
private:
    void Start(){
        m_Material.Init("models/weapons/v_toolgun/screen",TEXTURE_GROUP_MODEL,false);
        if(IsErrorMaterial(m_Material)){Warning("Tool screen: original screen material missing\n");ReleaseResources();return;}
        bool found=false;IMaterialVar *base=m_Material->FindVar("$basetexture",&found,false);
        if(!found||!base->IsTexture()){Warning("Tool screen: screen material has no base texture\n");ReleaseResources();return;}
        m_Original.Init(base->GetTextureValue());
        ITexture *rt=materials->CreateNamedRenderTargetTextureEx("GModToolgunScreen",256,256,RT_SIZE_NO_CHANGE,IMAGE_FORMAT_RGBA8888,MATERIAL_RT_DEPTH_NONE);
        if(!rt||rt->IsError()){Warning("Tool screen: 256x256 RT allocation failed\n");ReleaseResources();return;}
        m_Target.Init(rt);rt->DecrementReferenceCount();
        m_Panel=new CGMPToolScreenPanel;
        if(!m_Panel->Start()){ReleaseResources();return;}
        base->SetTextureValue(m_Target);
        Msg("Tool screen: original cl_viewscreen.lua, GModToolgunScreen 256x256 active\n");
    }
    void ReleaseResources(){
        if(m_Material.IsValid()&&m_Original.IsValid()){bool found=false;IMaterialVar *base=m_Material->FindVar("$basetexture",&found,false);if(found)base->SetTextureValue(m_Original);}
        delete m_Panel;m_Panel=NULL;m_Target.Shutdown();m_Original.Shutdown();m_Material.Shutdown();
    }
    void Release(){
        ReleaseResources();m_Attempted=false;s_GMPToolLanguage.clear();
        for(int i=0;i<3;++i){if(m_Icons[i])vgui::surface()->DestroyTextureID(m_Icons[i]);m_Icons[i]=0;}m_IconsAttempted=false;
        if(m_Gradient)vgui::surface()->DestroyTextureID(m_Gradient);m_Gradient=0;
    }
    CGMPToolScreenPanel *m_Panel;bool m_Attempted,m_IconsAttempted;int m_Icons[3],m_Gradient;CMaterialReference m_Material;CTextureReference m_Target,m_Original;
};
static CGMPToolScreenSystem s_GMPToolScreenSystem;
