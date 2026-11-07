// Exercise unchanged Facepunch HTML/JS against the real menu Lua and a test engine.
import { chromium } from 'playwright';
import { spawn } from 'node:child_process';
import { createInterface } from 'node:readline';
import { createServer } from 'node:http';
import { readFile, mkdir } from 'node:fs/promises';
import path from 'node:path';
import assert from 'node:assert/strict';

const root=path.resolve(process.argv[2]||'deps/garrysmod/garrysmod');
const binary=path.resolve(process.argv[3]||'build-native-host/glua_menu_test');
const portLua=path.resolve('resources/lua/port');
const artifact=path.resolve('artifacts/menu');
await mkdir(artifact,{recursive:true});
const host=spawn(binary,[root,portLua,'--serve'],{stdio:['pipe','pipe','inherit']});
const lines=createInterface({input:host.stdout});
const waiting=[];const pending=[];
lines.on('line',line=>{const data=JSON.parse(line);if(waiting.length)waiting.shift()(data);else pending.push(data);});
host.on('exit',code=>{if(code)throw Error(`Lua host exited: ${code}`);});
const next=()=>pending.length?Promise.resolve(pending.shift()):new Promise(r=>waiting.push(r));
const initial=await next();
const server=createServer(async(req,res)=>{
  try{
    const target=path.resolve(root,'.'+decodeURIComponent(new URL(req.url,'http://localhost').pathname));
    if(!target.startsWith(root+path.sep)){res.writeHead(403).end();return;}
    const bytes=await readFile(target);
    const mime={'.js':'application/javascript','.css':'text/css','.html':'text/html','.png':'image/png','.jpg':'image/jpeg'}[path.extname(target)]||'application/octet-stream';
    res.writeHead(200,{'Content-Type':mime});res.end(bytes);
  }catch{res.writeHead(404).end();}
});
await new Promise(r=>server.listen(0,'127.0.0.1',r));
const browser=await chromium.launch({headless:true,executablePath:process.env.PLAYWRIGHT_CHROMIUM_EXECUTABLE_PATH||undefined});
console.log('Browser:',browser.version());
let page,chain=Promise.resolve();const errors=[],commands=[];
try{
  const context=await browser.newContext({viewport:{width:1280,height:720},userAgent:'Valve Source Client GModPort browser smoke'});
  page=await context.newPage();
  page.on('pageerror',e=>errors.push(e.message));
  await page.exposeBinding('__gmpTestSend',(_source,name,packet)=>{
    chain=chain.then(async()=>{
      host.stdin.write(name+'\t'+JSON.stringify(packet)+'\n');
      const response=await next();
      errors.push(...response.errors);commands.push(...response.commands);
      for(const js of response.js)await page.evaluate(code=>(0,eval)(code),js);
    });
    return chain;
  });
  const bridgeHeader=await readFile('platform/html/bridge_script.h','utf8');
  const bridge=bridgeHeader.split('R"GMPJS(')[1].split(')GMPJS"')[0];
  await page.addInitScript(({bridge,functions})=>{
    window.webkit={messageHandlers:{gmp:{postMessage:packet=>window.__gmpTestSend(packet.name,packet)}}};
    (0,eval)(bridge);
    for(const name of functions){const [object,fn]=name.split('.');window.__gmpRegister(object,fn);}
  },{bridge,functions:initial.functions});
  // Test engine has two map fixtures; unavailable PC thumbnail content is explicit.
  await page.route('asset://**',async route=>{
    const url=new URL(route.request().url());
    const relative=url.hostname==='mapimage'?'html/img/downloading.png':decodeURIComponent(url.pathname.slice(1));
    const file=path.resolve(root,relative);
    if(!file.startsWith(root+path.sep)){await route.abort();return;}
    try{await route.fulfill({path:file});}catch{await route.abort();}
  });

  await page.goto(`http://127.0.0.1:${server.address().port}/html/menu.html`);
  await page.getByText('Start New Game',{exact:true}).waitFor();
  assert.equal(await page.evaluate(()=>IN_ENGINE),true);
  assert.equal(await page.evaluate(()=>gScope.Gamemode),'sandbox');
  await page.screenshot({path:path.join(artifact,'main-menu.png')});
  await page.getByText('Start New Game',{exact:true}).click();
  await page.locator('.mapicon').filter({hasText:'gm_construct'}).waitFor();
  await page.locator('.mapicon').filter({hasText:'gm_construct'}).click();
  await page.screenshot({path:path.join(artifact,'new-game.png')});
  await page.getByRole('button',{name:'Start Game',exact:true}).click();
  await page.waitForTimeout(600);
  await chain;
  assert(commands.includes('map gm_construct'),JSON.stringify(commands));
  assert.equal(errors.length,0,JSON.stringify(errors));
  console.log('PASS: original HTML, localization callbacks, installed-map selection and real Lua map command');
}finally{
  host.stdin.end();await browser.close();server.close();
}
