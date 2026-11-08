// Run the production extension with Angular's DOM debug metadata unavailable.
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import vm from 'node:vm';
class Element {
 constructor(tag){this.tagName=tag;this.children=[];this.attributes={};this.style={};this.textContent='';}
 setAttribute(k,v){this.attributes[k]=v;}
 appendChild(c){this.children.push(c);return c;}
 querySelector(selector){
  const attr=selector.match(/^\[([^\]]+)\]$/)?.[1];
  for(const c of this.children){if(attr&&attr in c.attributes)return c;const child=c.querySelector(selector);if(child)return child;}
  return null;
 }
}
const calls=[],listeners={},timers=[];let observer,cards=[new Element('workshopicon'),new Element('workshopicon')];
const ul=new Element('ul');
const dupesPage={querySelectorAll:()=>cards};
const context={
 console,location:{hash:'#/dupes/'},
 angular:{element(){throw Error('Angular DOM debug metadata must not be used');}},
 dupe:{Scope:{Files:[{local:true,info:{file:'dupes/first build.dupe'}},{local:false,info:{file:'online'}}]}},
 demo:{Scope:{RefreshCurrentView(){calls.push('refresh');}}},
 lua:{Run(code){calls.push(code);}},gmod:{ArmDupe(file){calls.push(file);}},
 document:{body:{},createElement:tag=>new Element(tag),querySelectorAll(selector){assert.equal(selector,'workshopicon');return cards;},querySelector(selector){
  if(selector==='.page[ng-controller="ControllerDupes"]')return context.location.hash.includes('dupes')?dupesPage:null;
  if(selector==='.page .options ul')return ul;
  throw Error('Unexpected selector: '+selector);
 }},
 MutationObserver:class{constructor(fn){observer=fn;}observe(){}},
 addEventListener(name,fn){listeners[name]=fn;},setTimeout(fn){timers.push(fn);},
};
context.window=context;
const source=readFileSync(process.argv[2]||new URL('../../resources/lua/port/local_content.lua',import.meta.url),'utf8').split('local tools = [=[')[1].split(']=]')[0];
vm.runInNewContext(source,context);
const arm=cards[0].querySelector('[data-gmp-arm]');assert(arm);assert.equal(arm.tagName,'button');assert(arm.style.cssText.includes('bottom:36px'));
assert.equal(cards[1].querySelector('[data-gmp-arm]'),null);arm.onclick();assert.equal(calls.pop(),'dupes/first build.dupe');
observer();assert.equal(cards[0].children.length,1);assert.equal(ul.children.length,1);
cards=[new Element('workshopicon')];context.dupe.Scope.Files=[{local:true,info:{file:'dupes/second.dupe'}}];observer();cards[0].querySelector('[data-gmp-arm]').onclick();assert.equal(calls.pop(),'dupes/second.dupe');
ul.children=[];context.location.hash='#/demos/';listeners.hashchange();
const buttons=ul.children[0].children;assert.equal(buttons.length,2);buttons[0].onclick();buttons[1].onclick();assert.deepEqual(calls,['port.RecordDemo()','port.StopDemo()']);timers[0]();assert.equal(calls.pop(),'refresh');
console.log('PASS: production local controls without Angular DOM scopes, public model binding, refresh, online exclusion, duplicate prevention and recording buttons');
