local nativeLocal = nativeLocal
-- Original local-library controllers; file operations remain bounded in the native host.
file.Delete = function(path, realm)
    assert(realm == nil or realm == 'MOD', 'Only local MOD files can be deleted')
    return nativeLocal('LocalDelete', path)
end
include('includes/util/workshop_files.lua')
include('menu/menu_dupe.lua')
include('menu/menu_demo.lua')
include('menu/menu_save.lua')
for namespace, object in pairs({dupe=ws_dupe, demo=demo, save=ws_save}) do
    local fetch = object.Fetch
    function object:Fetch(kind, offset, perpage, ...)
        offset=math.floor(tonumber(offset) or 0);perpage=math.floor(tonumber(perpage) or 20)
        assert(offset>=0 and perpage>0 and perpage<=256, 'Invalid local library page')
        if kind=='local' then return fetch(self,kind,offset,perpage,...) end
        self.HTML:Call(namespace..'.ReceiveLocal({totalresults:0,results:[]})')
        nativeLocal('Notify','Steam Workshop is unavailable; use the Local category.')
    end
end
function ws_dupe:Arm(path) nativeLocal('LocalAction','DupeArm',path) end
function ws_save:Load(path) nativeLocal('LocalAction','SaveLoad',path) end
function demo:Play(path) nativeLocal('LocalAction','DemoPlay',path) end
function demo:ToVideo() nativeLocal('Notify','Video export is unavailable. Demo recording and playback are supported.') end
function port.RecordDemo() nativeLocal('LocalAction','DemoRecord',os.date('recording_%Y%m%d_%H%M%S')) end
function port.StopDemo() nativeLocal('LocalAction','DemoStop','') end
function port.SaveDupe() nativeLocal('LocalAction','DupeSave','') end
local tools = [=[
(function(){
 if(window.__gmpLocalTools)return;window.__gmpLocalTools=true;
 function button(text,code,refresh){var b=document.createElement('button');b.textContent=text;b.style.cssText='display:block;margin:8px 0;padding:8px;cursor:pointer';b.onclick=function(){lua.Run(code);if(refresh)setTimeout(function(){if(window.demo&&demo.Scope)demo.Scope.RefreshCurrentView()},300)};return b}
 function update(){
  var route=location.hash.toLowerCase();
  var dupesPage=document.querySelector('.page[ng-controller="ControllerDupes"]');
  if(route.indexOf('dupes')!==-1&&dupesPage)dupesPage.querySelectorAll('workshopicon').forEach(function(card,index){
   if(card.querySelector('[data-gmp-arm]'))return;
   // Original common.js disables Angular DOM scope metadata. Use its public controller model.
   var files=window.dupe&&dupe.Scope&&dupe.Scope.Files,file=files&&files[index];
   if(!file||!file.local)return;
   var arm=document.createElement('button');arm.textContent='Load into Duplicator';arm.setAttribute('data-gmp-arm','1');
   arm.style.cssText='position:absolute;left:6px;right:6px;bottom:36px;z-index:11;padding:6px;font-size:12px;cursor:pointer';
   arm.onclick=function(){gmod.ArmDupe(file.info.file)};card.appendChild(arm);
  });
  var page=document.querySelector('.page .options ul');if(!page||page.querySelector('[data-gmp-local]'))return;
  var li=document.createElement('li');li.setAttribute('data-gmp-local','1');
  if(route.indexOf('demos')!==-1){li.appendChild(button('Record demo','port.RecordDemo()'));li.appendChild(button('Stop recording','port.StopDemo()',true))}
  else if(route.indexOf('dupes')!==-1)li.appendChild(button('Save copied dupe','port.SaveDupe()'));
  else return;
  page.appendChild(li);
 }
 new MutationObserver(update).observe(document.body,{childList:true,subtree:true});window.addEventListener('hashchange',update);update();
})();
]=]
local installed
function port.LocalContentTick()
    if IsValid(pnlMainMenu) and pnlMainMenu.HTML.ready and installed~=pnlMainMenu.HTML then
        installed=pnlMainMenu.HTML;pnlMainMenu.HTML:Call(tools)
    end
end
