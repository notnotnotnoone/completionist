// Pure atlas geometry. Scope determines width; task count only determines stack height.
(function(root,factory){if(typeof module==='object'&&module.exports)module.exports=factory();else root.CompletionistMapLayout=factory();})(typeof self!=='undefined'?self:this,function(){
  const G={left:180,top:225,taskPitch:120,laneGap:70,scope:{contained:330,medium:500,large:700},history:210,destination:205};
  const parse=v=>String(v).split('.').map(Number);
  const cmp=(a,b)=>{const x=parse(a),y=parse(b);return x[0]-y[0]||x[1]-y[1]||x[2]-y[2];};
  const tier=v=>{const [,m,p]=parse(v);return p?'patch':m?'minor':'major';};
  const family=v=>parse(v)[0]===0?'0.x':`${parse(v)[0]}.${parse(v)[1]}.x`;
  function layoutMap(data,{expandedHistory=false}={}){
    const highways=data.highways,owners={};for(const h of highways)for(const a of h.areas)owners[a]=h.id;
    const releases=data.releases.slice().sort((a,b)=>cmp(a.version,b.version));
    const released=releases.filter(r=>r.status==='released'),future=releases.filter(r=>r.status!=='released');
    const lanesOf=r=>{
      if(r.highways?.length)return highways.filter(h=>r.highways.includes(h.id)).map(h=>h.id);
      let own=data.tasks.filter(t=>(r.status==='released'?t.shippedIn===r.version:t.targetRelease===r.version)&&t.status!=='dropped');
      if(!own.length&&tier(r.version)!=='patch')own=data.tasks.filter(t=>t.targetRelease===r.version&&t.status!=='dropped');
      const used=new Set(own.map(t=>owners[t.area]));
      if(!used.size&&r.status==='released'&&tier(r.version)==='major')return highways.filter(h=>!h.opens||cmp(r.version,h.opens)>=0).map(h=>h.id);
      return highways.filter(h=>used.has(h.id)).map(h=>h.id);
    };
    const groups=[];for(const r of released){const f=family(r.version);let group=groups.at(-1);if(!group||group.family!==f){group={id:`history:${f}`,family:f,releases:[]};groups.push(group);}group.releases.push(r);}
    const events=[],historyGroups=[],regions=[],taskStops=[];let x=G.left;
    if(expandedHistory){for(const r of released){const width=tier(r.version)==='patch'?115:210;events.push({id:r.version,kind:tier(r.version),release:r,version:r.version,status:r.status,x:x+width/2,width,lanes:lanesOf(r)});x+=width;}}
    else for(const g of groups){g.x0=x;g.x=x+G.history/2;g.width=G.history;g.x1=x+G.history;g.lanes=[...new Set(g.releases.flatMap(lanesOf))];historyGroups.push(g);events.push({...g,kind:'history'});x+=G.history;}
    const historyEnd=x;x+=30;
    for(const r of future){
      const plan=data.releasePlans[r.version]||{},width=G.scope[plan.scope]||G.scope.medium;
      const own=data.tasks.filter(t=>t.targetRelease===r.version&&!t.shippedIn);
      const live=own.filter(t=>t.status!=='dropped');
      const region={id:r.version,version:r.version,release:r,plan,x0:x,x1:x+width,width,tasks:own,progress:{done:live.filter(t=>t.status==='done').length,total:live.length}};
      regions.push(region);const destination=x+width-65;
      events.push({id:r.version,kind:tier(r.version),release:r,version:r.version,status:r.status,x:destination,width:G.destination,lanes:lanesOf(r)});
      const columns=Math.max(1,Math.floor((width-G.destination-40)/145));
      for(const h of highways){const list=own.filter(t=>owners[t.area]===h.id);
        list.forEach((t,i)=>{const col=i%columns,row=Math.floor(i/columns);taskStops.push({id:t.id,kind:'task',task:t,target:r.version,lane:h.id,x:x+45+col*145,row,branch:t.status==='dropped',lanes:[h.id]});});
      }
      x+=width;
    }
    const lanes=[];let y=G.top;
    for(const h of highways){const rows=Math.max(1,...taskStops.filter(t=>t.lane===h.id).map(t=>t.row+1));const launch=events.find(e=>e.id===h.opens),construction=regions.find(r=>r.version===h.opens);const lane={id:h.id,y,rows,opensX:h.opens?(construction?.x0??launch?.x??x):G.left,launchX:launch?.x,opensId:h.opens};lanes.push(lane);y+=rows*G.taskPitch+G.laneGap;}
    for(const t of taskStops){const lane=lanes.find(l=>l.id===t.lane);t.y=lane.y+t.row*G.taskPitch+(t.branch?22:0);}
    const active=regions.find(r=>r.version===data.activeRelease);
    const now={id:'now',kind:'now',x:active?active.x0+20:historyEnd+12,target:data.activeRelease,version:released.at(-1)?.version||null};
    const width=x+80,height=y+35;events.push(now,{id:'end',kind:'end',x:width-30});
    return {G,width,height,roadEnd:width-30,events,historyGroups,historyEnd,regions,taskStops,lanes,now,expandedHistory};
  }
  return {G,layoutMap};
});
