// Lindell Live: receiver only. Never requests microphone access.
async function liveAPI(action,body=null,params={}){
 const u=new URL(location.href);u.search='';u.searchParams.set('action',action);if(share)u.searchParams.set('share',share);for(const[k,v]of Object.entries(params))u.searchParams.set(k,v);
 const controller=new AbortController(),timer=setTimeout(()=>controller.abort(),8000);
 try{const options={signal:controller.signal};if(body){body.set('csrf',state.csrf);options.method='POST';options.body=body;}const r=await fetch(u,options);const value=await r.json();if(!r.ok)throw Error(value.error||'Live request failed.');return value;}
 catch(e){if(e.name==='AbortError')throw Error('Live connection timed out. Please reconnect.');throw e;}finally{clearTimeout(timer);}
}
const controls=new Map();let active=null,statusBusy=false;
const node=(tag,text,cls)=>{const e=document.createElement(tag);if(text!==undefined)e.textContent=text;if(cls)e.className=cls;return e;};
const btn=(text,fn)=>{const b=node('button',text);b.type='button';b.onclick=()=>Promise.resolve(fn()).catch(error);return b;};
window.stopLivePlayback=()=>active?.stop();
window.addLiveControl=(container,p)=>{if(state.songOnly)return;const b=btn('● Live',()=>openLive(p));b.className='live-open';b.title='Open the live listening room';container.append(b);controls.set(b,p);};
if(typeof state!=='undefined'&&(state?.playlist||state?.admin))render();
async function refreshBadges(){
 if(statusBusy||document.hidden)return;statusBusy=true;
 try{for(const [b,p]of controls){if(!b.isConnected){controls.delete(b);continue;}try{const s=await liveAPI('liveStatus',null,{playlist:p.id});b.classList.toggle('is-live',s.online);b.textContent=s.online?'● Live now':'● Live';}catch{b.classList.remove('is-live');b.textContent='● Live';}}}finally{statusBusy=false;}
}
setInterval(refreshBadges,10000);setTimeout(refreshBadges,300);
window.addEventListener('pagehide',()=>active?.stop());
function gather(pc){return new Promise((resolve,reject)=>{let t;const done=()=>{clearTimeout(t);pc.removeEventListener('icegatheringstatechange',change);pc.removeEventListener('connectionstatechange',change);};const change=()=>{if(pc.connectionState==='closed'){done();reject(Error('Connection cancelled.'));}else if(pc.iceGatheringState==='complete'){done();resolve();}};pc.addEventListener('icegatheringstatechange',change);pc.addEventListener('connectionstatechange',change);t=setTimeout(()=>{done();reject(Error('Could not establish a network route. Try again.'));},12000);change();});}
async function openLive(p){
 if(active){active.dialog.focus();return;}if(document.querySelector('.ab-dialog')){notice('Close A/B Compare before opening Live.');return;}
 const owner=!!state.admin,dialog=node('dialog',undefined,'live-dialog');dialog.setAttribute('aria-label','Live listening room');
 const top=node('div',undefined,'live-heading'),title=node('div');title.append(node('p','LINDELL STREAMS · LIVE','live-eyebrow'),node('h2',p.title));const close=btn('Close',()=>dialog.close());top.append(title,close);
 const description=node('p','Listen to your producer’s mix as it happens. This is the same playlist link you already use.','live-help');
 const message=node('p','Checking producer status…','live-status');message.setAttribute('role','status');
 const listen=btn('Listen Live',start),stopButton=btn('Stop listening',()=>stop());listen.disabled=true;stopButton.disabled=true;
 const actions=node('div',undefined,'live-actions');actions.append(listen,stopButton);
 const media=node('audio');media.autoplay=true;media.controls=true;media.setAttribute('aria-label','Live listening volume');media.hidden=true;
 const help=node('p','Live audio plays directly, without the playlist EQ or limiter.','live-help');
 const diagnosticBox=node('textarea');diagnosticBox.readOnly=true;diagnosticBox.hidden=true;diagnosticBox.rows=5;diagnosticBox.style.width='100%';diagnosticBox.setAttribute('aria-label','Connection diagnostics');
 const diagnosticsButton=btn('Show connection diagnostics',()=>{if(pc)lastDiagnostic=diagnostic(message.textContent);diagnosticBox.value=lastDiagnostic||'No connection attempt yet.';diagnosticBox.hidden=false;diagnosticBox.focus();diagnosticBox.select();});
 dialog.append(top,description,message,actions,media,help,diagnosticsButton,diagnosticBox);
 let closed=false,generation=0,pc=null,peer=null,online=false,statusKnown=false,started=false,refreshTimer=null,pollTimer=null,connectTimer=null,busy=false,refreshBusy=false;
 let lastDiagnostic='',routeCount='not checked';
 function diagnostic(reason){const sdp=pc?.localDescription?.sdp||'';const counts={host:0,srflx:0,relay:0};for(const m of sdp.matchAll(/a=candidate:[^\r\n]* typ (host|srflx|relay)/g))counts[m[1]]++;return 'Listener diagnostics 0.4.0\n'+reason+'\nServers: '+routeCount+'\nICE: '+(pc?.iceConnectionState||'none')+' / gathering: '+(pc?.iceGatheringState||'none')+'\nCandidates: '+JSON.stringify(counts)+'\nOffer registered: '+!!peer+' / answer received: '+!!pc?.remoteDescription+' / audio track: '+!!media.srcObject;}
 let keyBox=null,keyMessage=null;let pairingBusy=false;
 if(owner){
  const setup=node('section',undefined,'live-setup');setup.append(node('h3','Connect your AAX plugin'),node('p','Paste this same shared playlist link into the plugin. Authorize publishing with a private connection key. Never send the key to listeners.','live-help'));
  const link=node('input');link.readOnly=true;link.value=p.url;link.setAttribute('aria-label','Shared playlist link');setup.append(link);
  const copy=btn('Copy playlist link',()=>copyValue(link));setup.append(copy);
  keyBox=node('input');keyBox.readOnly=true;keyBox.hidden=true;keyBox.setAttribute('aria-label','Private plugin connection key');keyBox.autocomplete='off';
  keyMessage=node('p','The AAX sender must implement the Live connection API before it can broadcast.','live-help');
  const pairing=btn('Create connection key',async()=>{
   if(pairingBusy)return;if(!confirm('Create a new private key? This disconnects any existing broadcaster for this playlist.'))return;
   pairingBusy=true;pairing.disabled=true;
   try{const result=await liveAPI('livePair',data({playlist:p.id}));if(closed)return;stop();keyBox.value=result.key;keyBox.hidden=false;copyKey.hidden=false;keyMessage.textContent='Shown once. Save in the plugin, not in your playlist link. Expires '+new Date(result.expires*1000).toLocaleDateString()+'.';await refresh();}catch(e){if(!closed)keyMessage.textContent=e.message;}finally{pairingBusy=false;pairing.disabled=false;}
  });
  const copyKey=btn('Copy private key',()=>copyValue(keyBox));copyKey.hidden=true;
  const revoke=btn('Disconnect plugin',async()=>{if(!confirm('Revoke the plugin key and stop this playlist’s live broadcast?'))return;await liveAPI('liveUnpair',data({playlist:p.id}));keyBox.value='';keyBox.hidden=true;copyKey.hidden=true;stop();keyMessage.textContent='Plugin disconnected. Its connection key is no longer valid.';await refresh();});
  const end=btn('End broadcast',async()=>{await liveAPI('liveStop',data({playlist:p.id}));stop();await refresh();});
  setup.append(pairing,keyBox,copyKey,keyMessage,end,revoke);dialog.append(setup);
 }
 async function copyValue(field){try{await navigator.clipboard.writeText(field.value);notice('Copied.');}catch{field.focus();field.select();notice('Press Copy to copy the selected text.');}}
 dialog.addEventListener('close',()=>{closed=true;clearTimeout(refreshTimer);stop();if(keyBox)keyBox.value='';active=null;dialog.remove();refreshBadges();},{once:true});
 dialog.addEventListener('cancel',()=>dialog.close());
 active={dialog,stop};document.body.append(dialog);dialog.showModal();await refresh();
 async function refresh(){
  if(closed||refreshBusy)return;refreshBusy=true;
  try{const result=await liveAPI('liveStatus',null,{playlist:p.id});if(closed)return;const changed=!statusKnown||online!==result.online;statusKnown=true;online=result.online;listen.disabled=started||busy||!online;listen.textContent='Listen Live';
   if(!online){if(started||busy)stop('The broadcast has ended.');else if(changed)message.textContent='Producer offline. This room will update when they go live.';}
   else if(peer&&peer.session!==result.session)stop('The producer started a new broadcast. Click Listen Live to reconnect.');
   else if(!started&&!busy&&changed)message.textContent='Producer connected. Click Listen Live to join.';
  }catch(e){if(!closed){online=false;if(started||busy)stop('Connection lost. '+e.message);else message.textContent=e.message;listen.disabled=true;}}
  finally{refreshBusy=false;if(!closed){clearTimeout(refreshTimer);refreshTimer=setTimeout(refresh,5000);}}
 }
 function claimAudio(){audio.pause();try{videoPlayer?.pauseVideo();}catch{}audio.addEventListener('play',mainPlay);window.liveMediaSync=()=>{if('mediaSession'in navigator){navigator.mediaSession.metadata=new MediaMetadata({title:p.title+' · Live',artist:p.producer||'Lindell Streams'});navigator.mediaSession.playbackState=started?'playing':'paused';try{navigator.mediaSession.setPositionState();}catch{}for(const action of ['play','pause','stop','previoustrack','nexttrack','seekbackward','seekforward','seekto'])try{navigator.mediaSession.setActionHandler(action,['pause','stop'].includes(action)?()=>stop():null);}catch{}}};syncMediaSession();}
 function mainPlay(){stop();}
 function stop(text='Live listening stopped.'){
  if(pc){lastDiagnostic=diagnostic(text);diagnosticBox.value=lastDiagnostic;}
  ++generation;busy=false;started=false;clearTimeout(pollTimer);clearTimeout(connectTimer);const old=peer;peer=null;
  if(pc){pc.ontrack=pc.onconnectionstatechange=null;pc.close();pc=null;}media.pause();media.srcObject=null;media.hidden=true;audio.removeEventListener('play',mainPlay);
  if(window.liveMediaSync){window.liveMediaSync=null;mediaKey=null;mediaActionsKey=null;syncMediaSession();}
  if(old)liveAPI('liveLeave',data({playlist:p.id,...old})).catch(()=>{});
  if(!closed){message.textContent=text;listen.disabled=!online;stopButton.disabled=true;}
 }
 async function start(){
  if(closed||busy||started)return;stop();const gen=++generation;busy=true;listen.disabled=true;stopButton.disabled=false;message.textContent='Connecting to the live mix…';
  try{
   if(!window.RTCPeerConnection)throw Error('This browser does not support live audio.');
   const status=await liveAPI('liveStatus',null,{playlist:p.id});if(gen!==generation||closed)return;if(!status.online)throw Error('The producer is offline.');
   const urls=(status.iceServers||[]).flatMap(s=>Array.isArray(s.urls)?s.urls:[s.urls]);routeCount=urls.filter(u=>typeof u==='string'&&u.startsWith('turn')).length+' TURN, '+urls.filter(u=>typeof u==='string'&&u.startsWith('stun')).length+' STUN';
   const connection=new RTCPeerConnection({iceServers:status.iceServers});pc=connection;const transceiver=connection.addTransceiver('audio',{direction:'recvonly'});
   const opus=window.RTCRtpReceiver?.getCapabilities('audio')?.codecs.filter(c=>c.mimeType.toLowerCase()==='audio/opus');if(opus?.length&&transceiver.setCodecPreferences)transceiver.setCodecPreferences(opus);
   connection.ontrack=e=>{if(gen!==generation||closed)return;media.srcObject=e.streams[0]||new MediaStream([e.track]);media.hidden=false;media.play().catch(()=>{message.textContent='Connected. Press Play on the live audio control.';});};
   connection.onconnectionstatechange=()=>{if(gen!==generation||closed)return;if(connection.connectionState==='connected'){clearTimeout(connectTimer);started=true;busy=false;message.textContent='● Listening live';window.liveMediaSync?.();}else if(connection.connectionState==='failed')stop('Connection failed. Click Listen Live to reconnect.');else if(connection.connectionState==='disconnected'){message.textContent='Connection interrupted. Reconnecting…';clearTimeout(connectTimer);connectTimer=setTimeout(()=>{if(gen===generation)stop('Connection lost. Click Listen Live to reconnect.');},12000);}};
   const offer=await connection.createOffer();
   // Ask the publisher for stereo Opus rather than its default voice/mono mode.
   const payload=offer.sdp.match(/a=rtpmap:(\d+) opus\/48000\/2/i)?.[1];
   if(payload)offer.sdp=offer.sdp.replace(new RegExp('(a=fmtp:'+payload+' [^\\r\\n]*)'), '$1;stereo=1');
   await connection.setLocalDescription(offer);await gather(connection);if(gen!==generation||closed)return;
   const joined=await liveAPI('liveJoin',data({playlist:p.id,offer:connection.localDescription.sdp}));
   if(gen!==generation||closed){liveAPI('liveLeave',data({playlist:p.id,...joined})).catch(()=>{});return;}peer=joined;claimAudio();
   connectTimer=setTimeout(()=>{if(gen===generation)stop('The producer could not connect. Please retry; the live relay may need configuration.');},25000);
   async function poll(){if(gen!==generation||closed)return;try{const reply=await liveAPI('livePoll',data({playlist:p.id,...joined}));if(gen!==generation||closed)return;if(reply.answer&&!connection.remoteDescription)await connection.setRemoteDescription({type:'answer',sdp:reply.answer});}catch(e){if(gen===generation)stop(e.message);return;}if(gen===generation&&!closed)pollTimer=setTimeout(poll,2000);}
   await poll();
  }catch(e){if(gen===generation&&!closed){lastDiagnostic=diagnostic(e.message);stop(e.message);}}
 }
}
