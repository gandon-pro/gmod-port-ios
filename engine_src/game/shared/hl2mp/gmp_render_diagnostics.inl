// Read-only renderer snapshots, independent of the unfinished client GLua bootstrap.
#ifdef CLIENT_DLL
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "materialsystem/materialsystem_config.h"
#include "igamesystem.h"

static const char *GMP_HDRName( HDRType_t type )
{
    switch ( type )
    {
        case HDR_TYPE_NONE: return "none";
        case HDR_TYPE_INTEGER: return "integer";
        case HDR_TYPE_FLOAT: return "float";
        default: return "unknown";
    }
}

static void GMP_PrintRenderStatus( const char *reason )
{
    Msg( "GMP render: begin reason=%s level=%s in_game=%d\n", reason,
         engine ? engine->GetLevelName() : "<no engine>", engine && engine->IsInGame() );
    if ( !materials || !engine )
    {
        Msg( "GMP render: material system or engine unavailable; active HDR unverified\n" );
        return;
    }
    IMaterialSystemHardwareConfig *hardware = materials->GetHardwareConfig(
        MATERIALSYSTEM_HARDWARECONFIG_INTERFACE_VERSION, NULL );
    const MaterialSystem_Config_t &config = materials->GetCurrentConfigForVideoCard();
    Msg( "GMP render: config_hdr_enabled=%d config_dxlevel=%d monitor_gamma=%g fullbright=%u\n",
         config.HDREnabled(), config.dxSupportLevel, config.m_fMonitorGamma, unsigned(config.nFullbright) );
    Msg( "GMP render: map_hdr_lighting=%s\n",
         engine->IsInGame() ? ( engine->MapHasHDRLighting() ? "yes" : "no" ) : "unavailable (no loaded map)" );
    if ( hardware )
    {
        Msg( "GMP render: hdr_type=%s hardware_hdr_type=%s dxlevel=%d\n",
             GMP_HDRName(hardware->GetHDRType()), GMP_HDRName(hardware->GetHardwareHDRType()), hardware->GetDXSupportLevel() );
        Msg( "GMP render: srgb_supported=%d fake_srgb_write=%d srgb_read_from_rt=%d srgb_correct_blending=%d gamma_ramp_caps=%d\n",
             hardware->SupportsSRGB(), hardware->FakeSRGBWrite(), hardware->CanDoSRGBReadFromRTs(),
             hardware->UsesSRGBCorrectBlending(), hardware->HasSetDeviceGammaRamp() );
    }
    else Msg( "GMP render: hardware config interface unavailable; active HDR unverified\n" );
    // Missing variables stay explicit. Never substitute defaults for runtime data.
    const char *names[] = { "mat_hdr_level", "mat_monitorgamma", "mat_monitorgamma_tv_enabled",
        "mat_fullbright", "mat_postprocess_enable", "mat_colorcorrection", "mat_disable_bloom",
        "mat_force_tonemap_scale", "mat_dynamic_tonemapping", "mat_specular", "mat_bumpmap" };
    for ( const char *name : names )
    {
        ConVarRef value( name, true );
        Msg( "GMP render: cvar %s=%s\n", name, value.IsValid() ? value.GetString() : "<unavailable>" );
    }
    Msg( "GMP render: end (Source HDR is scene rendering, not a display HDR claim)\n" );
}

CON_COMMAND( gmp_render_status, "Print actual gamma, sRGB and HDR state without changing settings" )
{
    GMP_PrintRenderStatus( "console" );
}

class CGMPRenderDiagnostics : public CAutoGameSystemPerFrame
{
public:
    CGMPRenderDiagnostics() : CAutoGameSystemPerFrame("GMPRenderDiagnostics"), m_PendingAt(-1) {}
    void LevelInitPostEntity() override { m_PendingAt = gpGlobals->realtime + 2.0; }
    void LevelShutdownPreEntity() override { m_PendingAt = -1; }
    void Update(float) override
    {
        if ( m_PendingAt >= 0 && gpGlobals->realtime >= m_PendingAt && engine->IsInGame() && !engine->IsDrawingLoadingImage() )
        {
            m_PendingAt = -1;
            GMP_PrintRenderStatus( "map loaded +2s" );
        }
    }
private:
    double m_PendingAt;
};
static CGMPRenderDiagnostics s_GMPRenderDiagnostics;
#endif
