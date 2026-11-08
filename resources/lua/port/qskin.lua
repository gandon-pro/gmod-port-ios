-- Execute Facepunch's real default skin and GWEN nine-slice functions unchanged.
local native=qskin.Invoke
local panelMeta={}
function FindMetaTable(name) if name=='Panel' then return panelMeta end error('Unsupported skin metatable: '..name) end
function Color(r,g,b,a) return {r=r,g=g,b=b,a=a or 255} end
function ColorAlpha(c,a) return Color(c.r,c.g,c.b,a) end
function Material(path)
    local mat={path=path}
    function mat:IsError() return self.path~='gwenskin/GModDefault.png' end
    function mat:GetTexture()
        return {Width=function() return native('Width') end,Height=function() return native('Height') end}
    end
    function mat:GetColor(x,y) return Color(native('Pixel',x,y)) end
    return mat
end
surface={}
function surface.SetMaterial(mat) assert(mat and not mat:IsError()) end
function surface.SetDrawColor(r,g,b,a) native('Color',r,g,b,a or 255) end
function surface.DrawTexturedRectUV(x,y,w,h,u0,v0,u1,v1) native('Quad',x,y,w,h,u0,v0,u1,v1) end
local skin

derma={DefineSkin=function(name,description,value) assert(name=='Default');skin=value end}
include('derma/derma_gwen.lua')
include('skins/default.lua')
function qskin.Draw(path,x,y,w,h)
    local fn=skin.tex
    for part in path:gmatch('[^.]+') do fn=assert(fn[part],'Unknown GWEN texture: '..path) end
    fn(x,y,w,h)
end
