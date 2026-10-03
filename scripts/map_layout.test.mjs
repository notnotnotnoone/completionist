import test from 'node:test';
import assert from 'node:assert/strict';
import {createRequire} from 'node:module';
const require = createRequire(import.meta.url);
const {layoutMap} = require('../docs/roadmap/map-layout.js');
const fixture = () => ({activeRelease:'2.0.0', highways:[{id:'engine',title:'Engine',areas:['engine']},{id:'tsf',title:'Text service',areas:['dll']},{id:'browser',title:'Browser',areas:['extension'],opens:'3.0.0'}],
  releases:[{version:'1.0.0',status:'released',title:'Desktop'},{version:'1.0.1',status:'released',title:'Fix',highways:['engine']},{version:'2.0.0',status:'next',title:'Overhaul'},{version:'2.1.0',status:'planned',title:'Tense'},{version:'3.0.0',status:'planned',title:'Browser'}],
  releasePlans:{'2.0.0':{scope:'large'},'2.1.0':{scope:'contained'},'3.0.0':{scope:'large'}},
  tasks:[{id:'glass',targetRelease:'2.0.0',area:'dll',status:'next',title:'Glass',stage:1},{id:'lifecycle',targetRelease:'2.0.0',area:'engine',status:'todo',title:'AI lifecycle',stage:2},{id:'abandoned',targetRelease:'2.0.0',area:'dll',status:'dropped',title:'Old popup'},{id:'tense',targetRelease:'2.1.0',area:'engine',status:'todo',title:'Tense'},{id:'browser-task',targetRelease:'3.0.0',area:'extension',status:'todo',title:'Extension'}]});

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
test('history groups are view-only and expand to every real released version',()=>{
  const d=fixture(),before=structuredClone(d),compact=layoutMap(d),expanded=layoutMap(d,{expandedHistory:true});
  assert.equal(compact.historyGroups.length,1);assert.equal(expanded.events.filter(e=>e.release?.status==='released').length,2);assert.deepEqual(d,before);
});
test('you are here marks the active region, separately from latest shipped',()=>{
  const L=layoutMap(fixture());assert.equal(L.now.target,'2.0.0');assert.equal(L.now.version,'1.0.1');assert.ok(L.now.x>L.regions[0].x0);assert.ok(L.now.x<L.regions[0].x1);
});
test('previously shipped contributions do not become duplicate roadwork stops',()=>{
  const d=fixture();d.tasks.push({id:'old-fix',targetRelease:'2.0.0',area:'engine',status:'done',shippedIn:'1.0.1',title:'Shipped fix'});
  assert.ok(!layoutMap(d).taskStops.some(t=>t.id==='old-fix'));
});
