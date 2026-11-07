"""Compile production read-only diagnostics in a small Source interface fixture."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
fixture = r'''
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <map>
#include <string>
#ifndef NULL
#define NULL nullptr
#endif
#define CLIENT_DLL 1
#define MATERIALSYSTEM_HARDWARECONFIG_INTERFACE_VERSION "fixture"
#define CON_COMMAND(name,description) void name()
std::string logText;
void Msg(const char* format,...) { char b[2048]; va_list a;va_start(a,format);vsnprintf(b,sizeof(b),format,a);va_end(a);logText+=b; }
enum HDRType_t { HDR_TYPE_NONE,HDR_TYPE_INTEGER,HDR_TYPE_FLOAT };
struct IMaterialSystemHardwareConfig {
 HDRType_t active=HDR_TYPE_NONE,hardware=HDR_TYPE_FLOAT;
 HDRType_t GetHDRType(){return active;} HDRType_t GetHardwareHDRType(){return hardware;}
 int GetDXSupportLevel(){return 95;}
 bool SupportsSRGB(){return true;} bool FakeSRGBWrite(){return true;}
 bool CanDoSRGBReadFromRTs(){return true;} bool UsesSRGBCorrectBlending(){return false;}
 bool HasSetDeviceGammaRamp(){return true;}
};
struct MaterialSystem_Config_t {
 bool hdr=false; int dxSupportLevel=95; float m_fMonitorGamma=2.2f; unsigned char nFullbright=0;
 bool HDREnabled()const{return hdr;}
};
struct Materials {
 IMaterialSystemHardwareConfig hw; MaterialSystem_Config_t config;bool hasHardware=true;
 IMaterialSystemHardwareConfig* GetHardwareConfig(const char*,int*){return hasHardware?&hw:nullptr;}
 const MaterialSystem_Config_t& GetCurrentConfigForVideoCard(){return config;}
} materialFixture;
Materials* materials=&materialFixture;
struct Engine {
 bool inGame=false,loading=false,mapHDR=false;
 const char* GetLevelName(){return inGame?"fixture_map":"";}
 bool IsInGame(){return inGame;} bool IsDrawingLoadingImage(){return loading;}
 bool MapHasHDRLighting(){assert(inGame);return mapHDR;}
} engineFixture;
Engine* engine=&engineFixture;
std::map<std::string,std::string> values={{"mat_hdr_level","2"},{"mat_monitorgamma","2.2"}};
struct ConVarRef {
 std::string name; ConVarRef(const char* n,bool):name(n){}
 bool IsValid(){return values.count(name)!=0;}
 const char* GetString(){assert(IsValid());return values.at(name).c_str();}
};
struct CAutoGameSystemPerFrame {
 explicit CAutoGameSystemPerFrame(const char*){}
 virtual ~CAutoGameSystemPerFrame()=default;
 virtual void LevelInitPostEntity(){} virtual void LevelShutdownPreEntity(){} virtual void Update(float){}
};
struct Globals {float realtime=0;} globals;
Globals* gpGlobals=&globals;
#include "gmp_render_diagnostics.inl"
void Contains(const char* text){assert(logText.find(text)!=std::string::npos);}
int main(){
 // Requested HDR=2 is not reported as active when hardware returns NONE.
 gmp_render_status();
 Contains("hdr_type=none hardware_hdr_type=float");Contains("cvar mat_hdr_level=2");
 Contains("map_hdr_lighting=unavailable");Contains("mat_fullbright=<unavailable>");
 auto before=values;
 engineFixture.inGame=true;materialFixture.hw.active=HDR_TYPE_INTEGER;
 materialFixture.config.hdr=true;engineFixture.mapHDR=false;logText.clear();
 gmp_render_status();Contains("hdr_type=integer");Contains("config_hdr_enabled=1");Contains("map_hdr_lighting=no");
 assert(before==values);
 // Snapshot waits for a loaded map and does not spam every frame.
 logText.clear();s_GMPRenderDiagnostics.LevelInitPostEntity();
 globals.realtime=1;s_GMPRenderDiagnostics.Update(0);assert(logText.empty());
 globals.realtime=3;engineFixture.loading=true;s_GMPRenderDiagnostics.Update(0);assert(logText.empty());
 engineFixture.loading=false;s_GMPRenderDiagnostics.Update(0);Contains("reason=map loaded +2s");
 logText.clear();s_GMPRenderDiagnostics.Update(0);assert(logText.empty());
 s_GMPRenderDiagnostics.LevelInitPostEntity();s_GMPRenderDiagnostics.LevelShutdownPreEntity();
 globals.realtime=99;s_GMPRenderDiagnostics.Update(0);assert(logText.empty());
 materialFixture.hasHardware=false;gmp_render_status();Contains("hardware config interface unavailable");
 logText.clear();materials=nullptr;gmp_render_status();Contains("material system or engine unavailable");
}
'''
with tempfile.TemporaryDirectory() as directory:
    tmp = Path(directory)
    for name in ['materialsystem/imaterialsystem.h','materialsystem/imaterialsystemhardwareconfig.h',
                 'materialsystem/materialsystem_config.h','igamesystem.h']:
        path = tmp / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text('// Interfaces supplied by the fixture.\n')
    source=tmp/'test.cpp'
    source.write_text(fixture)
    exe=tmp/'test'
    subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-I',str(tmp),'-I',
                    str(root/'engine_src/game/shared/hl2mp'),str(source),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print('Renderer diagnostic fixture passed: requested/active HDR, missing values, read-only capture, load delay and cleanup')
