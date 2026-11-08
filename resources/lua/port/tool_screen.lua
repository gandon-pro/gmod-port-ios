-- Bounded surface adapter for the unmodified Sandbox tool screen.
local call = toolscreen.Invoke
local function bind(name) return function(...) return call(name, ...) end end
SWEP = {}
function SWEP:GetToolObject() return nil end -- Native stools have no Lua DrawToolScreen override.
function Material(name)
    assert(name == 'models/weapons/v_toolgun/screen')
    return { SetTexture = function(_, key, rt) assert(key == '$basetexture' and rt == 'GModToolgunScreen') end }
end
function GetRenderTarget(name, w, h)
    assert(name == 'GModToolgunScreen' and w == 256 and h == 256)
    return name
end
function GetConVar(name)
    assert(name == 'gmod_toolmode')
    return { GetString = bind('Mode') }
end
RealTime = bind('Time')
surface = {}
for _, name in ipairs({'GetTextureID', 'CreateFont', 'SetFont', 'GetTextSize', 'SetTextColor', 'SetTextPos', 'DrawText', 'SetDrawColor', 'SetTexture', 'DrawTexturedRect'}) do
    surface[name] = bind(name)
end
local depth, camera = 0, false
render = {
    PushRenderTarget = function(rt) assert(rt == 'GModToolgunScreen' and depth == 0); depth = 1 end,
    PopRenderTarget = function() assert(depth == 1 and not camera); depth = 0 end
}
cam = {
    Start2D = function() assert(depth == 1 and not camera); camera = true end,
    End2D = function() assert(camera); camera = false end
}
include('gamemodes/sandbox/entities/weapons/gmod_tool/cl_viewscreen.lua')
function toolscreen.Paint()
    assert(depth == 0 and not camera)
    SWEP:RenderScreen()
    assert(depth == 0 and not camera)
end
