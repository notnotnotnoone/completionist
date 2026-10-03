// SVG release atlas and interaction; data and pure geometry live in separate files.
(() => {
  const R=window.RM,esc=R.esc,color=id=>R.hwColor(id);
  const T=(x,y,text,extra='')=>`<text x="${x}" y="${y}" ${extra}>${esc(text)}</text>`;
  function lines(text,max=22){const words=String(text).split(/\s+/),out=[''];for(const word of words){let i=out.length-1;if(out[i]&&out[i].length+word.length+1>max){out.push(word);}else out[i]+=(out[i]?' ':'')+word;}return out.slice(0,3).map((s,i)=>i===2&&out.length>3?s+'…':s);}
  const action=(id,hws,label,body,hit)=>`<g class="marker" data-hws="${hws.join(' ')}"><g aria-hidden="true" style="pointer-events:none">${body}</g><g class="stn" tabindex="0" role="button" data-id="${esc(id)}" aria-label="${esc(label)}"><title>${esc(label)}</title><rect class="hit" ${hit} rx="8" style="fill:transparent;stroke:transparent"/></g></g>`;
  function draw(svg,L){
    let out=`<rect width="${L.width}" height="${L.height}" style="fill:var(--surface)"/>`;
    for(const [i,region] of L.regions.entries()){
      out+=`<rect x="${region.x0}" y="0" width="${region.width}" height="${L.height}" style="fill:${i%2?'var(--surface)':'var(--paper-2)'};opacity:.65"/>`;
      out+=T(region.x0+25,30,region.version===R.data.activeRelease?'BUILDING TOWARD '+region.version:'THEN · '+region.version,'style="font:800 12px var(--display);letter-spacing:.12em"');
      out+=T(region.x0+25,54,`${region.plan.scope||'medium'} scope · ${region.progress.done}/${region.progress.total} roadwork complete`,'style="font:13px var(--display);fill:var(--muted)"');
    }
    for(const lane of L.lanes){
      const h=R.hwById[lane.id];out+=`<path data-hw="${lane.id}" d="M${lane.opensX} ${lane.y} H${L.roadEnd}" style="fill:none;stroke:${color(lane.id)};stroke-width:9;opacity:.24;stroke-dasharray:9 5"/>`;
      if(lane.opensX<L.historyEnd)out+=`<path data-hw="${lane.id}" d="M${lane.opensX} ${lane.y} H${L.historyEnd}" style="fill:none;stroke:${color(lane.id)};stroke-width:9"/>`;
      out+=T(20,lane.y+5,h.title,'style="font:800 14px var(--display)"');
      if(h.opens&&lane.launchX)out+=T(lane.opensX+25,lane.y+55,`Construction · opens at ${h.opens}`,'style="font:11px var(--mono);fill:var(--muted)"');
    }
    for(const e of L.events){
      if(e.kind==='history'){
        let body=`<rect x="${e.x-91}" y="90" width="182" height="94" rx="8" style="fill:var(--sign-deep)"/><rect x="${e.x-85}" y="96" width="170" height="82" rx="5" style="fill:none;stroke:var(--sign-dim)"/>`;
        body+=T(e.x,119,e.family,'text-anchor="middle" style="font:600 24px var(--mono);fill:var(--sign-ink)"');
        body+=T(e.x,145,`${e.releases.length} shipped releases`,'text-anchor="middle" style="font:13px var(--display);fill:var(--sign-ink)"');
        body+=T(e.x,166,'Open history →','text-anchor="middle" style="font:800 11px var(--display);fill:var(--sign-dim)"');
        for(const id of e.lanes){const y=L.lanes.find(l=>l.id===id).y;body+=`<circle cx="${e.x}" cy="${y}" r="7" style="fill:${color(id)};stroke:var(--surface);stroke-width:2"/>`;}
        out+=action(e.id,e.lanes,`${e.family}: ${e.releases.length} shipped releases`,body,`x="${e.x-95}" y="84" width="190" height="106"`);continue;
      }
      if(!e.release)continue;
      const r=e.release,ys=e.lanes.map(id=>L.lanes.find(l=>l.id===id).y);
      let body='';
      if(ys.length){const y0=Math.min(...ys),y1=Math.max(...ys);body+=`<path d="M${e.x} 178 V${y0}" style="stroke:var(--line-strong);stroke-width:2"/>`;
        if(ys.length>1)body+=`<path d="M${e.x} ${y0} V${y1}" style="stroke:var(--sign);stroke-width:${e.kind==='major'?22:13};stroke-linecap:round"/>`;
        for(const id of e.lanes){const y=L.lanes.find(l=>l.id===id).y;body+=`<circle cx="${e.x}" cy="${y}" r="${e.kind==='patch'?8:10}" style="fill:${r.status==='released'?color(id):'var(--surface)'};stroke:${color(id)};stroke-width:3"/>`;if(r.status==='released')body+=T(e.x,y+4,'✓','text-anchor="middle" style="font:900 12px var(--display);fill:var(--sign-ink)"');}
      }
      if(e.kind==='patch'&&r.status==='released'){
        const y=(ys[0]||L.G.top)+25;const title=lines(r.title,15);
        body+=T(e.x,y,r.version,'text-anchor="middle" style="font:600 12px var(--mono)"');title.forEach((s,i)=>body+=T(e.x,y+18+i*14,s,'text-anchor="middle" style="font:11px var(--display);fill:var(--muted)"'));
        if(r.kind==='fix')body+=`<path d="M${e.x} ${y-35} v18 h14" style="fill:none;stroke:${color(e.lanes[0]||'engine')};stroke-width:3"/>`;
        out+=action(e.id,e.lanes,`${r.version} released · ${r.title}`,body,`x="${e.x-54}" y="${y-42}" width="108" height="105"`);
      }else{
        const title=R.data.releasePlans[r.version]?.title||r.title;
        body+=`<rect x="${e.x-127}" y="80" width="190" height="98" rx="9" style="fill:var(--sign);stroke:${r.status==='next'?'var(--accent)':'var(--surface)'};stroke-width:3"/><rect x="${e.x-121}" y="86" width="178" height="86" rx="5" style="fill:none;stroke:var(--sign-dim)"/>`;
        body+=T(e.x-112,115,r.version,'style="font:600 26px var(--mono);fill:var(--sign-ink)"');lines(title,23).slice(0,2).forEach((s,i)=>body+=T(e.x-112,139+i*17,s,'style="font:800 14px var(--display);fill:var(--sign-ink)"'));
        out+=action(e.id,e.lanes,`${r.version} · ${title} · ${r.status}`,body,`x="${e.x-131}" y="76" width="198" height="106"`);
      }
    }
    for(const stop of L.taskStops){
      const t=stop.task,lane=L.lanes.find(l=>l.id===stop.lane),active=['doing','next','blocked'].includes(t.status),dropped=stop.branch;
      let body=`<path d="M${stop.x} ${lane.y} V${stop.y}" style="stroke:${color(stop.lane)};stroke-width:2;opacity:${dropped?.3:.55}${dropped?';stroke-dasharray:3 3':''}"/><circle cx="${stop.x}" cy="${stop.y}" r="9" style="fill:${t.status==='done'?color(stop.lane):'var(--surface)'};stroke:${active?'var(--accent)':color(stop.lane)};stroke-width:3${t.status==='todo'||dropped?';stroke-dasharray:3 2':''};opacity:${dropped?.45:1}"/>`;
      if(t.status==='done')body+=T(stop.x,stop.y+4,'✓','text-anchor="middle" style="font:900 12px var(--display);fill:var(--sign-ink)"');
      if(dropped)body+=T(stop.x,stop.y+4,'×','text-anchor="middle" style="font:900 12px var(--display);fill:var(--muted)"');
      if(t.stage)body+=T(stop.x,stop.y-19,`STAGE ${t.stage}`,'text-anchor="middle" style="font:600 10px var(--mono);fill:var(--muted)"');
      lines(t.label||t.title,20).slice(0,2).forEach((s,i)=>body+=T(stop.x,stop.y+27+i*14,s,`text-anchor="middle" style="font:700 11px var(--display);fill:${dropped?'var(--muted)':'var(--ink)'}"`));
      body+=T(stop.x,stop.y+58,R.STATUS[t.status].label,'text-anchor="middle" style="font:10px var(--display);fill:var(--muted)"');
      out+=action(stop.id,[stop.lane],`${t.label||t.title} · ${R.STATUS[t.status].label} · target ${t.targetRelease}`,body,`x="${stop.x-64}" y="${stop.y-31}" width="128" height="95"`);
    }
    out+=`<path d="M${L.now.x} 190 V${L.height-60}" style="stroke:var(--accent);stroke-width:2;stroke-dasharray:5 5"/>`;
    out+=T(L.now.x+8,L.height-32,`YOU ARE HERE${L.now.target?' · building '+L.now.target:''}`,'style="font:800 12px var(--display);fill:var(--accent-ink)"');
    out+=T(20,L.height-10,`Last shipped: ${L.now.version||'none'} · Width represents broad scope, not time or percent complete.`,'style="font:11px var(--mono);fill:var(--muted)"');
    svg.setAttribute('viewBox',`0 0 ${L.width} ${L.height}`);svg.setAttribute('width',L.width);svg.setAttribute('height',L.height);svg.innerHTML=out;
  }
  function mount(frame,layout,{onSelect}={}){
    let L=layout,selected=null,spot=null;
    const scroller=frame.querySelector('.map-scroll'),inner=frame.querySelector('.map-inner'),svg=frame.querySelector('svg'),tags=frame.querySelector('.lane-tags');
    const reduced=matchMedia('(prefers-reduced-motion: reduce)').matches;
    function render(){inner.style.width=L.width+'px';inner.style.height=L.height+'px';draw(svg,L);tags.style.height=L.height+'px';tags.innerHTML=L.lanes.map(l=>`<span class="lane-tag" style="top:${l.y-17}px;--hw:${color(l.id)}">${esc(R.hwById[l.id].title)}</span>`).join('');api.select(selected);api.spotlight(spot);}
    const api={
      select(id){selected=id;const history=L.historyGroups.find(g=>g.releases.some(r=>r.version===id));const marker=history?.id||id;svg.querySelectorAll('.stn').forEach(g=>g.classList.toggle('on',g.dataset.id===marker));},
      spotlight(hw){spot=hw;svg.querySelectorAll('[data-hw]').forEach(e=>e.classList.toggle('dim',!!hw&&e.dataset.hw!==hw));svg.querySelectorAll('.marker').forEach(e=>e.classList.toggle('dim',!!hw&&!e.dataset.hws.split(' ').includes(hw)));},
      xOf(id){const e=L.events.find(e=>e.id===id)||L.taskStops.find(t=>t.id===id);if(e)return e.x;const group=L.historyGroups.find(g=>g.releases.some(r=>r.version===id));return group?.x||0;},
      scrollToX(x,smooth=true){scroller.scrollTo({left:Math.max(0,x-scroller.clientWidth*.15),behavior:smooth&&!reduced?'smooth':'auto'});},
      fit(on){frame.classList.toggle('fit',on);},
      redraw(next){L=next;render();}
    };
    svg.addEventListener('click',e=>{const g=e.target.closest('.stn');if(g)onSelect?.(g.dataset.id);});
    svg.addEventListener('keydown',e=>{const g=e.target.closest('.stn');if(g&&['Enter',' '].includes(e.key)){e.preventDefault();onSelect?.(g.dataset.id);}});
    scroller.addEventListener('scroll',()=>tags.classList.toggle('show',scroller.scrollLeft>40),{passive:true});
    scroller.addEventListener('keydown',e=>{if(e.target===scroller&&['ArrowLeft','ArrowRight'].includes(e.key)){e.preventDefault();scroller.scrollBy({left:e.key==='ArrowRight'?240:-240,behavior:reduced?'auto':'smooth'});}});
    render();return api;
  }
  window.CompletionistMap={mount};
})();
