// The original GLua skin executes against one real decoded GWEN atlas.
#include "gmp_image.h"
#include "../gmod/glua_runtime.h"
class CGMPQSkin {
public:
    ~CGMPQSkin(){Shutdown();}
    void Shutdown(){m_VM.reset();if(m_Texture&&vgui::surface())vgui::surface()->DestroyTextureID(m_Texture);m_Texture=0;free(m_Pixels);m_Pixels=NULL;m_Attempted=m_Ready=false;}
    bool Draw(const char *slot,int x,int y,int w,int h){
        if(w<=0||h<=0)return false;
        if(!m_Attempted)Init();if(!m_Ready){
            int shade=!strcmp(slot,"Tree")?255:!strcmp(slot,"TabT_Inactive")?140:170;
            vgui::surface()->DrawSetColor(shade,shade,shade,255);vgui::surface()->DrawFilledRect(x,y,x+w,y+h);return false;
        }
        lua_State *L=m_VM->State();const int top=lua_gettop(L);
        lua_getglobal(L,"qskin");lua_getfield(L,-1,"Draw");lua_pushstring(L,slot);
        lua_pushinteger(L,x);lua_pushinteger(L,y);lua_pushinteger(L,w);lua_pushinteger(L,h);
        if(lua_pcall(L,5,0,0)){Warning("Q skin: %s\n",lua_tostring(L,-1));m_Ready=false;}
        lua_settop(L,top);return m_Ready;
    }
    const char *Status()const{return m_Ready?"original default.lua/GWEN active":m_Attempted?"original GWEN skin unavailable; native fallback":"not loaded";}
private:
    void Init(){
        m_Attempted=true;CUtlBuffer image;
        if(!filesystem->ReadFile("materials/gwenskin/GModDefault.png","GAME",image)||!GMPImage_DecodeRGBA(image.Base(),image.TellPut(),&m_Pixels,&m_Width,&m_Height)){
            Warning("Q skin: missing or invalid materials/gwenskin/GModDefault.png\n");return;
        }
        m_Texture=vgui::surface()->CreateNewTextureID(true);
        vgui::surface()->DrawSetTextureRGBA(m_Texture,static_cast<unsigned char *>(m_Pixels),m_Width,m_Height,1,true);
        glua::RuntimeConfig config{glua::REALM_CLIENT,[](const std::string &path,std::string &out){CUtlBuffer file;std::string name="lua/"+path;if(!filesystem->ReadFile(name.c_str(),"GAME",file))return false;out.assign(static_cast<const char *>(file.Base()),file.TellPut());return true;},[](const std::string &s){Warning("Q skin: %s\n",s.c_str());},[this](lua_State *L){lua_newtable(L);lua_pushlightuserdata(L,this);lua_pushcclosure(L,Invoke,1);lua_setfield(L,-2,"Invoke");lua_setglobal(L,"qskin");}};
        m_VM.reset(new glua::Runtime(config));m_Ready=m_VM->File("port/qskin.lua");
    }
    static int Invoke(lua_State *L){
        CGMPQSkin *self=static_cast<CGMPQSkin *>(lua_touserdata(L,lua_upvalueindex(1)));const char *op=luaL_checkstring(L,1);
        if(!strcmp(op,"Width")||!strcmp(op,"Height")){lua_pushinteger(L,!strcmp(op,"Width")?self->m_Width:self->m_Height);return 1;}
        if(!strcmp(op,"Pixel")){int x=luaL_checkinteger(L,2),y=luaL_checkinteger(L,3);if(x<0||x>=self->m_Width||y<0||y>=self->m_Height)return luaL_error(L,"GWEN sample outside atlas");unsigned char *p=static_cast<unsigned char *>(self->m_Pixels)+(y*self->m_Width+x)*4;for(int i=0;i<4;i++)lua_pushinteger(L,p[i]);return 4;}
        if(!strcmp(op,"Color")){vgui::surface()->DrawSetColor(luaL_checkinteger(L,2),luaL_checkinteger(L,3),luaL_checkinteger(L,4),luaL_checkinteger(L,5));return 0;}
        if(!strcmp(op,"Quad")){
            int x=static_cast<int>(luaL_checknumber(L,2)),y=static_cast<int>(luaL_checknumber(L,3));int w=static_cast<int>(luaL_checknumber(L,4)),h=static_cast<int>(luaL_checknumber(L,5));if(w<1||h<1)return 0;
            vgui::surface()->DrawSetTexture(self->m_Texture);vgui::surface()->DrawTexturedSubRect(x,y,x+w,y+h,luaL_checknumber(L,6),luaL_checknumber(L,7),luaL_checknumber(L,8),luaL_checknumber(L,9));return 0;
        }
        return luaL_error(L,"Unknown Q skin native: %s",op);
    }
    bool m_Attempted=false,m_Ready=false;void *m_Pixels=NULL;int m_Width=0,m_Height=0,m_Texture=0;std::unique_ptr<glua::Runtime> m_VM;
};
static CGMPQSkin s_GMPQSkin;
CON_COMMAND(gmp_qmenu_status,"Print Q-menu renderer status"){Msg("Q menu: %s\n",s_GMPQSkin.Status());}
