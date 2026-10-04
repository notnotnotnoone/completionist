// Release atlas page: task/release selection, scope regions, preserved history and itinerary.
(() => {
  const R=window.RM,$=s=>document.querySelector(s),data=R?.data;
  if(!data){$('#guide').textContent="Couldn't load roadmap.js.";return;}
  const {esc,rich}=R,releases=data.releases,shipped=releases.filter(r=>r.status==='released'),latest=shipped.at(-1),active=R.groupByVersion[data.activeRelease];
  const title=r=>data.releasePlans[r.version]?.title||r.title;
  const pill=(cls,text)=>`<span class="pill ${cls}">${esc(text)}</span>`;
  const taskLink=t=>`<li class="g-task"><span class="pill s-${t.status}">${esc(R.STATUS[t.status].label)}</span><a href="#t${encodeURIComponent(t.id)}" data-go="${esc(t.id)}">${rich(t.label||t.title)}</a><span class="meta">${t.stage?'Stage '+t.stage+' · ':''}${t.shippedIn?'Shipped '+t.shippedIn:'Target '+t.targetRelease}</span></li>`;
  let selected=data.activeRelease||latest?.version||'0.0.0';
  const layout=()=>window.CompletionistMapLayout.layoutMap(data);
  let L=layout();
  const map=window.CompletionistMap.mount($('#map-frame'),L,{onSelect:id=>select(id,false)});
  function releaseGuide(r){
    const group=R.groupByVersion[r.version],own=r.status==='released'?R.tasks.filter(t=>t.shippedIn===r.version):group?.tasks||[];
    const content=r.essay?`<div class="essay">${r.essay.map(p=>`<p>${rich(p)}</p>`).join('')}</div>`:`<p class="prose big">${rich(r.text)}</p>`;
    const plans=group?`<aside class="g-box"><p class="lbl">${r.status==='released'?'Original destination':'Release gate'} · ${r.version}</p><h4 class="g-goal">${esc(group.title)}</h4><p>${rich(group.goal)}</p><p><strong>Ready when:</strong> ${rich(group.done_when)}</p>${r.status!=='released'?'<p class="meta">Completing all tasks makes the release ready. Shipping requires a separate validated release action.</p>':''}${R.highways.map(h=>{const list=own.filter(t=>t.hw?.id===h.id);return list.length?`<div class="g-group" style="--hw:${R.hwColor(h.id)}"><h4>${esc(h.title)}</h4><ul>${list.map(taskLink).join('')}</ul></div>`:'';}).join('')}</aside>`:'';
    $('#guide').innerHTML=`<article class="gd"><header class="gd-head"><span class="plate">${r.version}</span><div><h3>${esc(title(r))}</h3><div class="gd-meta">${pill('v-'+r.status,R.RELEASE_STATUS[r.status])}${r.date?`<time>${esc(R.fmtDate(r.date))}</time>`:''}${R.highwaysOf(r).map(id=>`<span class="hw-tag" style="--hw:${R.hwColor(id)}"><i></i>${esc(R.hwById[id].title)}</span>`).join('')}</div></div></header><div class="gd-body${group?' has-box':''}"><div class="gd-text">${content}${r.validation?`<p class="meta">Validation: ${rich(r.validation)}</p>`:''}${!group&&own.length?`<ul class="g-list">${own.map(taskLink).join('')}</ul>`:''}</div>${plans}</div></article>`;
  }
  function guide(id){
    const task=R.tasks.find(t=>t.id===id);
    if(task){const total=R.tasks.filter(t=>t.targetRelease===task.targetRelease&&t.stage).reduce((n,t)=>Math.max(n,t.stage),0);$('#guide').innerHTML=`<article class="gd"><header class="gd-head"><div><p class="lbl">Roadwork · ${esc(task.hw?.title||task.area)}${task.stage?' · stage '+task.stage+'/'+total:''}</p><h3>${rich(task.title)}</h3><div class="gd-meta">${pill('s-'+task.status,R.STATUS[task.status].label)}<button class="linkish" data-go="${task.targetRelease}">Target ${task.targetRelease}</button>${task.shippedIn?`<button class="linkish" data-go="${task.shippedIn}">Shipped in ${task.shippedIn}</button>`:task.status==='done'?'<span class="meta">Complete · not yet shipped</span>':''}</div></div></header><div class="gd-body"><div class="gd-text"><p class="prose">${rich(task.notes||'No notes recorded.')}</p><p class="meta">${(task.refs||[]).map(r=>/^https?:\/\//.test(r)?`<a href="${esc(r)}" target="_blank" rel="noopener">${esc(r)}</a>`:rich(r)).join(' · ')}</p><a href="tasks.html#${encodeURIComponent(task.id)}">Open on the task board →</a></div></div></article>`;return;}
    if(id==='0.0.0'){const own=shipped.filter(r=>R.parseVer(r.version)[0]===0);$('#guide').innerHTML=`<article class="gd"><header class="gd-head"><h3>Groundwork and preview</h3></header><p class="prose">Every shipped version is preserved. Select a release for its original writing and shipping details.</p><ul class="g-list">${own.map(r=>`<li><button class="linkish" data-go="${r.version}">${r.version}</button> ${esc(r.title)} <span class="meta">${esc(r.date)}</span></li>`).join('')}</ul></article>`;return;}
    const release=R.releaseByVersion[id];if(release)releaseGuide(release);
  }
  function select(id,scroll=true){selected=id;map.select(id);guide(id);const prefix=R.tasks.some(t=>t.id===id)?'t':'v';history.replaceState(null,'','#'+prefix+encodeURIComponent(id));if(scroll)map.scrollToX(map.xOf(id));}
  function hash(){const raw=decodeURIComponent(location.hash.slice(1));if(raw==='v0.0.0')return '0.0.0';if(raw.startsWith('v')&&R.releaseByVersion[raw.slice(1)])return raw.slice(1);if(raw.startsWith('t')&&R.tasks.some(t=>t.id===raw.slice(1)))return raw.slice(1);return null;}
  $('#hero-tag').textContent='Versions are destinations. Tasks are the roadwork between them.';$('#hero-ghost').textContent=latest?.version||'';
  $('#hero-arrows').innerHTML=releases.filter(r=>r.status!=='released').map((r,i)=>`<li>${i===0?'↑ Building toward':'→ Then'} ${r.version} · ${esc(title(r))}</li>`).join('');
  $('#vms-text').textContent=`${active?'Building '+active.version+' · '+active.title:'No active release'} · latest shipped ${latest?.version||'none'}`;
  $('#updated').textContent=data.updated;$('#updated').setAttribute('datetime',data.updated);
  const work=active?R.progress(active.tasks):{done:0,total:0};
  const post=(n,label)=>`<li class="post"><b class="num">${n}</b><span>${esc(label)}</span></li>`;
  $('#posts').innerHTML=post(latest?.version||'—','Latest shipped')+post(active?.version||'—','Current destination')+post(active?active.tasks.filter(t=>!['done','dropped'].includes(t.status)).length:0,'Tasks remaining')+post(shipped.length,'Real releases shipped');
  $('#hw-chips').innerHTML=R.highways.map(h=>`<button class="hw-chip" data-hw="${h.id}" style="--hw:${R.hwColor(h.id)}" aria-pressed="false"><i></i>${esc(h.title)}</button>`).join('');
  $('#hw-chips').addEventListener('click',e=>{const b=e.target.closest('[data-hw]');if(!b)return;const on=b.getAttribute('aria-pressed')!=='true';$('#hw-chips').querySelectorAll('button').forEach(x=>x.setAttribute('aria-pressed',String(x===b&&on)));map.spotlight(on?b.dataset.hw:null);});
  $('#legend').innerHTML=`<div><p class="lbl">How to read the map · time runs east</p><ul class="key"><li><span><b>Coloured lanes</b> · the parts of the app, one highway each</span></li><li><span><b>Signs</b> · major and minor releases, where lanes meet</span></li><li><span><b>✓ Dots</b> · one per task; the small number under it is the release that shipped it</span></li><li><span><b>Release stops</b> · releases with no recorded tasks</span></li><li><span><b>Magenta ring</b> · doing, next or blocked</span></li><li><span><b>Dashed ring</b> · still to do</span></li><li><span><b>× Faded dot</b> · dropped</span></li><li><span><b>Hatched road</b> · under construction</span></li></ul><p class="key-note">A run of dots between two signs shows how much work went into that release. Unshipped regions are as wide as their scope (contained, medium or large), never their task count.</p></div>`;
  $('#work-cols').innerHTML=R.highways.map(h=>{const list=R.tasks.filter(t=>t.hw?.id===h.id&&['doing','next','blocked'].includes(t.status));return `<section class="work-col" style="--hw:${R.hwColor(h.id)}"><h3>${esc(h.title)}</h3>${list.length?`<ul>${list.map(taskLink).join('')}</ul>`:'<p class="clear">Clear road</p>'}</section>`;}).join('');
  function itinerary(){
    const future=releases.filter(r=>r.status!=='released');const item=r=>`<article class="itin-rel itin-${R.tierOf(r.version)}" id="i-${r.version}"><header><span class="mono ver">${r.version}</span><h4>${esc(title(r))}</h4>${pill('v-'+r.status,R.RELEASE_STATUS[r.status])}${r.date?`<time>${esc(r.date)}</time>`:''}</header><div class="itin-text">${(r.essay||[r.text]).map(p=>`<p>${rich(p)}</p>`).join('')}</div><button class="ctl" data-go="${r.version}">Show on map</button></article>`;
    const families=new Map();for(const r of shipped){const [a,b]=R.parseVer(r.version),key=a===0?'0.x':`${a}.${b}.x`;if(!families.has(key))families.set(key,[]);families.get(key).push(r);}
    $('#itin').innerHTML=`<div class="itin-major">${future.map(item).join('')}</div>${[...families].reverse().map(([name,rs])=>`<details class="itin-history"><summary>${name} · ${rs.length} shipped releases</summary><div class="itin-major">${rs.map(item).join('')}</div></details>`).join('')}`;
  }
  itinerary();
  document.addEventListener('click',e=>{const go=e.target.closest('[data-go]');if(go){e.preventDefault();select(go.dataset.go);}});
  $('#go-start').onclick=()=>map.scrollToX(0);$('#go-now').onclick=()=>{map.fit(false);$('#fit').setAttribute('aria-pressed','false');map.scrollToX(L.now.x);select(data.activeRelease||latest.version,false);};$('#go-end').onclick=()=>map.scrollToX(L.width);
  $('#fit').onclick=()=>{const on=$('#fit').getAttribute('aria-pressed')!=='true';$('#fit').setAttribute('aria-pressed',String(on));map.fit(on);};
    addEventListener('hashchange',()=>{const id=hash();if(id)select(id);});
  const initial=hash();R.bindThemeButton($('#theme'));select(initial||selected,false);document.fonts.ready.then(()=>{L=layout();map.redraw(L);});map.scrollToX(initial?map.xOf(initial):L.now.x,false);
})();
