import test from 'node:test';
import assert from 'node:assert/strict';
import {createRequire} from 'node:module';
const require = createRequire(import.meta.url);
const {layoutMap} = require('../docs/roadmap/map-layout.js');
const fixture = () => ({activeRelease:'2.0.0', highways:[{id:'engine',title:'Engine',areas:['engine']},{id:'tsf',title:'Text service',areas:['dll']},{id:'browser',title:'Browser',areas:['extension'],opens:'3.0.0'}],
  releases:[{version:'1.0.0',status:'released',title:'Desktop'},{version:'1.0.1',status:'released',title:'Fix',highways:['engine']},{version:'1.0.2',status:'released',title:'Two things'},{version:'1.0.3',status:'released',title:'No recorded tasks'},{version:'2.0.0',status:'next',title:'Overhaul'},{version:'2.1.0',status:'planned',title:'Tense'},{version:'3.0.0',status:'planned',title:'Browser'}],
  releasePlans:{'2.0.0':{scope:'large'},'2.1.0':{scope:'contained'},'3.0.0':{scope:'large'}},
  tasks:[{id:'shipped-fix',targetRelease:'2.0.0',area:'engine',status:'done',shippedIn:'1.0.1',title:'Shipped fix'},
    {id:'two-a',targetRelease:'1.0.0',area:'engine',status:'done',shippedIn:'1.0.2',title:'First of two'},{id:'two-b',targetRelease:'1.0.0',area:'dll',status:'done',shippedIn:'1.0.2',title:'Second of two'},
    {id:'glass',targetRelease:'2.0.0',area:'dll',status:'next',title:'Glass',stage:1},{id:'lifecycle',targetRelease:'2.0.0',area:'engine',status:'todo',title:'AI lifecycle',stage:2},{id:'abandoned',targetRelease:'2.0.0',area:'dll',status:'dropped',title:'Old popup'},{id:'tense',targetRelease:'2.1.0',area:'engine',status:'todo',title:'Tense'},{id:'browser-task',targetRelease:'3.0.0',area:'extension',status:'todo',title:'Extension'}]});

test('task insertion keeps all future destinations at the same x',()=>{
  const d=fixture(),before=layoutMap(d);for(let i=0;i<30;i++)d.tasks.push({id:`extra-${i}`,targetRelease:'2.0.0',area:'dll',status:'todo',title:'Extra work'});
  const after=layoutMap(d);
  assert.deepEqual(after.regions.map(r=>[r.version,r.x0,r.x1]),before.regions.map(r=>[r.version,r.x0,r.x1]));
  assert.ok(after.height>before.height);
  assert.equal(new Set(after.taskStops.map(t=>`${t.x}:${t.y}`)).size,after.taskStops.length);
});
test('scope width distinguishes large and contained initiatives',()=>{
  const L=layoutMap(fixture());assert.ok(L.regions[0].width>L.regions[1].width);assert.equal(L.regions[0].width,L.regions[2].width);
});
test('dropped work stays on a branch and is excluded from completed progress',()=>{
  const L=layoutMap(fixture());const t=L.taskStops.find(t=>t.id==='abandoned');assert.equal(t.branch,true);assert.equal(L.regions[0].progress.total,2);
});
test('release landmarks span only participating highways',()=>{
  const L=layoutMap(fixture());assert.deepEqual(L.events.find(e=>e.id==='2.0.0').lanes,['engine','tsf']);assert.deepEqual(L.events.find(e=>e.id==='3.0.0').lanes,['browser']);
});
test('new highway has construction space before its launch',()=>{
  const L=layoutMap(fixture()),lane=L.lanes.find(l=>l.id==='browser'),release=L.events.find(e=>e.id==='3.0.0');assert.ok(lane.opensX<release.x);assert.ok(L.taskStops.find(t=>t.id==='browser-task').x<release.x);
});
test('shipped history shows every task as its own stop, in release order, left of the unshipped work',()=>{
  const d=fixture(),before=structuredClone(d),L=layoutMap(d);
  const shipped=L.taskStops.filter(t=>t.shipped);
  assert.deepEqual(shipped.map(t=>t.id),['shipped-fix','two-a','two-b']);
  assert.ok(shipped.every(t=>t.x<L.historyEnd)&&L.taskStops.filter(t=>!t.shipped).every(t=>t.x>=L.historyEnd));
  assert.deepEqual(shipped.map(t=>t.x),[...shipped.map(t=>t.x)].sort((a,b)=>a-b));
  assert.equal(new Set(shipped.map(t=>t.x)).size,shipped.length);
  assert.deepEqual(d,before);
});
test('a release is reachable by its first task, and releases without tasks stay as stops',()=>{
  const L=layoutMap(fixture());
  assert.equal(L.events.find(e=>e.id==='1.0.2').x,L.taskStops.find(t=>t.id==='two-a').x);
  assert.equal(L.taskStops.find(t=>t.id==='two-a').first,true);assert.equal(L.taskStops.find(t=>t.id==='two-b').first,false);
  const bare=L.events.find(e=>e.id==='1.0.3');assert.equal(bare.kind,'patch');assert.ok(bare.x<L.historyEnd);
});
test('a shipped task sits on its own highway',()=>{
  const L=layoutMap(fixture());assert.deepEqual(['two-a','two-b'].map(id=>L.taskStops.find(t=>t.id===id).lane),['engine','tsf']);
});
test('you are here marks the active region, separately from latest shipped',()=>{
  const L=layoutMap(fixture());assert.equal(L.now.target,'2.0.0');assert.equal(L.now.version,'1.0.3');assert.ok(L.now.x>L.regions[0].x0);assert.ok(L.now.x<L.regions[0].x1);
});
test('previously shipped contributions appear once, in history, never as duplicate roadwork stops',()=>{
  const L=layoutMap(fixture());const hits=L.taskStops.filter(t=>t.id==='shipped-fix');assert.equal(hits.length,1);assert.equal(hits[0].shipped,true);
});
