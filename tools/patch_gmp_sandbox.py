"""GMod sandbox hooks in stock engine files: the numpad and the material tool.

The features themselves live in engine_src/game/shared/hl2mp/gmp_*.inl. This also runs on the
Windows test tree (no git pin): py -3.13 patch_gmp_sandbox.py <engine dir>
"""


def apply(root):
    changed = []

    def change(path, before, after):
        text = (root / path).read_text(encoding='utf-8')
        if after in text:
            return
        if text.count(before) != 1:
            raise RuntimeError('Unexpected sandbox context in %s: %s' % (path, before[:75]))
        (root / path).write_text(text.replace(before, after), encoding='utf-8', newline='\n')
        if path not in changed:
            changed.append(path)

    # GMod's numpad library: any key, alongside its bind (thrusters, wheels, winches...).
    change('game/client/cdll_client_int.cpp',
           'int CHLClient::IN_KeyEvent( int eventcode, ButtonCode_t keynum, const char *pszCurrentBinding )\n'
           '{\n',
           '// GModPort: GMod numpad keys, set by gmp_sents.inl.\n'
           'void ( *g_pfnGMPKeyEvent )( int eventcode, ButtonCode_t keynum ) = NULL;\n'
           '\n'
           'int CHLClient::IN_KeyEvent( int eventcode, ButtonCode_t keynum, const char *pszCurrentBinding )\n'
           '{\n'
           '\tif ( g_pfnGMPKeyEvent )\n'
           '\t\tg_pfnGMPKeyEvent( eventcode, keynum );\n')
    # GMod's material tool: a per-entity material override (gmp_entmods.inl).
    change('game/client/c_baseanimating.cpp',
           'int C_BaseAnimating::DrawModel( int flags )\n'
           '{\n',
           '// GModPort: the material tool, set by gmp_entmods.inl.\n'
           'IMaterial *( *g_pfnGMPMaterialOverride )( C_BaseAnimating *pEnt ) = NULL;\n'
           '\n'
           'int C_BaseAnimating::DrawModel( int flags )\n'
           '{\n')
    change('game/client/c_baseanimating.cpp',
           '\t\tif ( !IsFollowingEntity() )\n'
           '\t\t{\n'
           '\t\t\tdrawn = InternalDrawModel( flags|extraFlags );\n'
           '\t\t}\n',
           '\t\tIMaterial *pGMPMaterial = ( g_pfnGMPMaterialOverride && !( flags & ( STUDIO_SHADOWDEPTHTEXTURE | STUDIO_SSAODEPTHTEXTURE ) ) ) ? g_pfnGMPMaterialOverride( this ) : NULL;\n'
           '\t\tif ( pGMPMaterial )\n'
           '\t\t\tmodelrender->ForcedMaterialOverride( pGMPMaterial );\n'
           '\t\tif ( !IsFollowingEntity() )\n'
           '\t\t{\n'
           '\t\t\tdrawn = InternalDrawModel( flags|extraFlags );\n'
           '\t\t}\n')
    change('game/client/c_baseanimating.cpp',
           '\t\t\t\t\tdrawn = InternalDrawModel( STUDIO_RENDER|extraFlags );\n'
           '\t\t\t\t}\n'
           '\t\t\t}\n'
           '\t\t}\n'
           '\t}\n',
           '\t\t\t\t\tdrawn = InternalDrawModel( STUDIO_RENDER|extraFlags );\n'
           '\t\t\t\t}\n'
           '\t\t\t}\n'
           '\t\t}\n'
           '\t\tif ( pGMPMaterial )\n'
           '\t\t\tmodelrender->ForcedMaterialOverride( NULL );\n'
           '\t}\n')
    return changed


if __name__ == '__main__':
    import sys
    from pathlib import Path
    print(apply(Path(sys.argv[1]).resolve()))
