-- Menu-only binding layer. Original Facepunch files below are loaded unchanged.
-- This does not claim general game-realm GLua/VGUI compatibility.
local native = port.Invoke
local JSON = include('port/json.lua')
local function bind(op) return function(...) return native(op, ...) end end
util = {TableToJSON=JSON.encode, JSONToTable=function(s) local ok,v=pcall(JSON.decode,s);if ok then return v end end, KeyValuesToTable=bind('KeyValues')}
for _,name in ipairs({'string','number','table','function','bool'}) do
    local expected=name=='bool' and 'boolean' or name
    _G['is'..name]=function(v) return type(v)==expected end
end
function tobool(v) return v~=nil and v~=false and v~=0 and v~='0' and v~='false' end
function IsValid(v) return type(v)=='table' and type(v.IsValid)=='function' and v:IsValid() end
ErrorNoHaltWithStack=print
Msg=print
MsgN=print
ScrW=bind('Width');ScrH=bind('Height');CurTime=bind('Time');RealTime=CurTime
IsInGame=bind('InGame')
function RunConsoleCommand(name,...)
    native('Command',name,...)
    if name=='gmod_language' and LanguageChanged then LanguageChanged((...)) end
end
RunGameUICommand=bind('UICommand')
function GetConVarString(name) return native('ConVar',name) or '' end
function GetConVar(name)
    if native('ConVar',name)==nil then return nil end
    return {GetString=function() return native('ConVar',name) end}
end
function RunString(code)
    local chunk,err=loadstring(code,'HTML menu');if not chunk then error(err) end
    return chunk()
end
VERSIONSTR='GModPort'
-- Actual engine build/protocol strings supplied by the host, never a PC GMod version.
NETVERSIONSTR=tostring(native('Protocol'));BRANCH='iOS port'
file={Read=bind('Read'),Find=bind('Find'),Exists=bind('Exists')}
local function unsupported(name) return function() error(name..' is not implemented by this iOS engine build',2) end end
file.Delete=unsupported('Workshop file deletion')
local cookies
local function loadCookies()
    if cookies then return end
    local text=native('DataRead','menu_cookies.json')
    cookies=text and util.JSONToTable(text) or {};if type(cookies)~='table' then cookies={} end
end
cookie={}
function cookie.GetString(key,default) loadCookies();return cookies[key] or default end
function cookie.Set(key,value) loadCookies();cookies[key]=tostring(value);native('DataWrite','menu_cookies.json',JSON.encode(cookies)) end
language={GetPhrase=bind('Phrase')}
motionsensor={IsAvailable=function() return false end}
surface={PlaySound=bind('Sound')}
gui={HideGameUI=bind('Hide'),IsConsoleVisible=bind('ConsoleVisible'),ShowConsole=bind('Console'),ActivateGameUI=bind('Activate'),OpenURL=bind('OpenURL')}
engine={GetGamemodes=bind('Gamemodes'),ActiveGamemode=function() return 'sandbox' end,GetAddons=bind('Addons'),GetUserContent=bind('UserContent'),GetGames=bind('Games'),SetMounted=unsupported('Steam game mounting')}
serverlist={IsCurrentServerFavorite=function() return false end,Query=unsupported('Steam server queries'),PlayerList=unsupported('Steam player queries'),PingServer=unsupported('Steam server ping'),AddCurrentServerToFavorites=unsupported('Steam favourites')}
function CanAddServerToFavorites() return false end
function GetAddonStatus() return true,true end
function GetAPIManifest(callback) native('Log','Online news manifest is unavailable in this port') end
function LoadAddonPresets() return native('DataRead','addon_presets.json') end
function SaveAddonPresets(s) native('DataWrite','addon_presets.json',s) end
function OpenFolder(name) native('OpenFolder',name) end
function OpenProblemsPanel() native('Notify','Menu uses original Facepunch HTML and Lua. Steam Workshop, saves, dupes, demos and other gamemodes still require engine bindings.') end
steamworks=setmetatable({}, {__index=function(_,name) return unsupported('steamworks.'..name) end})
for _,name in ipairs({'ws_dupe','ws_save','addon','demo'}) do
    _G[name]=setmetatable({}, {__index=function(_,method) return unsupported(name..'.'..method) end})
end
-- The original background Lua draws into the HTML background layer.
local drawList={}
local material,alpha
function Material(path) return {path=path} end
function DynamicMaterial(path)
    local m=Material(path)
    function m:SetDynamicImage(value)
        self.path=value
        local clean=value:gsub('^%.%./','')
        self.width,self.height=native('ImageSize',clean)
    end
    function m:IsError() return not self.width or self.width<1 or not self.height or self.height<1 end
    function m:GetInt(key) return key=='$realwidth' and self.width or self.height end
    m:SetDynamicImage(path)
    return m
end
function surface.SetMaterial(value) material=value end
function surface.SetDrawColor(r,g,b,a) alpha=a or 255 end
function surface.DrawTexturedRectRotated(x,y,w,h,angle)
    drawList[#drawList+1]={path=material.path:gsub('^%.%./',''),x=x-w/2,y=y-h/2,w=w,h=h,angle=-angle,alpha=alpha}
end
function surface.DrawTexturedRect(x,y,w,h)
    drawList[#drawList+1]={path=material.path,x=x,y=y,w=w,h=h,angle=0,alpha=alpha}
end
local frameDelta=0
function FrameTime() return frameDelta end
function IsInLoading() return native('Loading') end
concommand={commands={}}
function concommand.Add(name,fn) concommand.commands[name]=fn end

include('includes/extensions/string.lua')
include('includes/extensions/table.lua')
include('includes/modules/hook.lua')
local timers={}
timer={}
function timer.Simple(delay,fn) timers[#timers+1]={due=CurTime()+math.max(delay,0),fn=fn} end
function timer.Create(name,delay,reps,fn)
    for i=#timers,1,-1 do if timers[i].name==name then table.remove(timers,i) end end
    timers[#timers+1]={name=name,due=CurTime()+math.max(delay,0),delay=delay,reps=reps,fn=fn}
end

FILL=5;TOP=1
local classes={}
local panels={}
local methods={}
vgui={}
function methods:IsValid() return not self.removed end
function methods:GetParent() return self.parent end
function methods:GetWide() return self.width or ScrW() end
function methods:GetTall() return self.height or ScrH() end
function methods:Dock(mode) self.dock=mode end
function methods:SetSize(w,h) self.width=w;self.height=h end
function methods:SetKeyboardInputEnabled(v) self.keyboard=v end
function methods:SetMouseInputEnabled(v) self.mouse=v end
function methods:RequestFocus() self.focus=true end
function methods:MakePopup() self.popup=true end
function methods:SetPopupStayAtBack(v) self.stayBack=v end
function methods:Remove()
    if self.removed then return end
    self.removed=true
    for _,child in ipairs(panels) do if child.parent==self then child:Remove() end end
    if self.handle then native('HTMLDestroy',self.handle);self.handle=nil end
    self.callbacks={};self.javascript={}
end
local function panel(class,parent)
    local p={parent=parent,callbacks={},javascript={},width=ScrW(),height=ScrH()}
    setmetatable(p,{__index=function(_,key) return class[key] or methods[key] end})
    panels[#panels+1]=p
    if p.Init then p:Init() end
    return p
end
function vgui.Register(name,class,base) classes[name]=class end
function vgui.RegisterTable(class,base) return class end
function vgui.CreateFromTable(class,parent) return panel(class,parent) end
function vgui.Create(name,parent)
    if name=='EditablePanel' or name=='Panel' then return panel({},parent) end
    if name=='DHTML' then return panel(classes.DHTML,parent) end
    if not classes[name] then error('Unimplemented menu VGUI class: '..name) end
    return panel(classes[name],parent)
end
local HTML={}
classes.DHTML=HTML
function HTML:AddFunction(object,name,fn)
    self.callbacks[object..'.'..name]=fn
    if self.handle then native('HTMLFunction',self.handle,object,name) end
end
function HTML:OpenURL(url)
    if self.handle then native('HTMLDestroy',self.handle) end
    self.ready=false;self.handle=native('HTMLCreate',url)
    if self.handle==0 then error('Cannot create original HTML menu') end
    for name in pairs(self.callbacks) do
        local object,fn=name:match('^([^.]+)%.(.+)$');native('HTMLFunction',self.handle,object,fn)
    end
    native('HTMLViewport',self.handle,ScrW(),ScrH())
    native('HTMLShow',self.handle,true)
end
function HTML:QueueJavascript(js)
    if self.ready then native('HTMLCall',self.handle,js) else self.javascript[#self.javascript+1]=js end
end
HTML.RunJavascript=HTML.QueueJavascript
function HTML:Call(js) self:QueueJavascript(js) end
function HTML:SetSize(w,h) methods.SetSize(self,w,h);if self.handle then native('HTMLViewport',self.handle,w,h) end end

include('includes/util/javascript_util.lua')
include('menu/getmaps.lua')
include('menu/mainmenu.lua')
local reported={}
local function report(err)
    err=tostring(err);if reported[err] then return end;reported[err]=true
    native('Log',err);native('Notify',err)
end
local width,height=ScrW(),ScrH()
local previousTime=CurTime()
function port.MenuTick()
    local now=CurTime();frameDelta=math.max(0,math.min(now-previousTime,0.1));previousTime=now
    drawList={}
    local due={}
    for i=#timers,1,-1 do
        local task=timers[i]
        if task.due<=CurTime() then
            table.remove(timers,i);due[#due+1]=task
            if task.reps and (task.reps==0 or task.reps>1) then
                if task.reps>1 then task.reps=task.reps-1 end
                task.due=CurTime()+math.max(task.delay,0.001);timers[#timers+1]=task
            end
        end
    end
    for i=#due,1,-1 do local ok,err=pcall(due[i].fn);if not ok then report(err) end end
    for _,p in ipairs(panels) do
        if p.handle and not p.removed then
            for count=1,128 do
                local name,json=native('HTMLPoll',p.handle);if not name then break end
                if name=='_gmp.Ready' then
                    p.ready=true
                    if p.OnBeginLoadingDocument then p.OnBeginLoadingDocument() end
                    for _,js in ipairs(p.javascript) do native('HTMLCall',p.handle,js) end
                    p.javascript={}
                    if p.OnDocumentReady then p:OnDocumentReady() end
                elseif name=='_gmp.Error' or name=='_gmp.ResourceError' then report(json)
                else
                    local fn=p.callbacks[name]
                    if fn then
                        local ok,packet=pcall(JSON.decode,json)
                        if ok and type(packet)=='table' and type(packet.args)=='table' then
                            local args=packet.args
                            local argc=#args
                            for i=1,#args do if args[i]==JSON.null then args[i]=nil end end
                            local result={pcall(fn,unpack(args,1,argc))};local success=table.remove(result,1)
                            if success then
                                if packet.request and packet.request~=JSON.null then native('HTMLReply',p.handle,packet.request,JSON.encode(result)) end
                            else report(result[1]) end
                        else report('Malformed HTML callback packet') end
                    end
                end
            end
        end
    end
    if ScrW()~=width or ScrH()~=height then
        width,height=ScrW(),ScrH()
        if IsValid(pnlMainMenu) then pnlMainMenu:SetSize(width,height);pnlMainMenu.HTML:SetSize(width,height) end
    end
    if IsValid(pnlMainMenu) and pnlMainMenu.HTML.ready then
        local ok,err=pcall(pnlMainMenu.Paint,pnlMainMenu);if not ok then report(err) end
        native('BackgroundFrame',pnlMainMenu.HTML.handle,JSON.encode(drawList),width,height)
    end
end
function port.MenuShutdown()
    for _,p in ipairs(panels) do p:Remove() end
    timers={};panels={}
end
