const {chromium}=require('playwright');
const fs=require('node:fs');const http=require('node:http');const {spawn}=require('node:child_process');const os=require('node:os');const path=require('node:path');
(async()=>{
 const temp=fs.mkdtempSync(path.join(os.tmpdir(),'lindell-live-'));let sender,browser;const server=http.createServer((_,res)=>{res.end('<!doctype html><title>Lindell stereo receiver test</title>');});
 try{
 await new Promise(r=>server.listen(0,'127.0.0.1',r));browser=await chromium.launch({headless:true,args:['--autoplay-policy=no-user-gesture-required','--use-fake-device-for-media-stream']});const page=await browser.newPage();await page.goto(`http://127.0.0.1:${server.address().port}`);
 const offer=await page.evaluate(async()=>{
  const pc=window.pc=new RTCPeerConnection({iceServers:[]});pc.addTransceiver('audio',{direction:'recvonly'});
  window.audio=new AudioContext({sampleRate:48000});await audio.resume();window.analysers=[];
  pc.ontrack=e=>{const stream=new MediaStream([e.track]);const player=window.player=document.createElement('audio');player.autoplay=true;player.volume=.00001;player.srcObject=stream;document.body.appendChild(player);player.play().catch(error=>window.playError=String(error));const source=audio.createMediaStreamSource(stream);const splitter=audio.createChannelSplitter(2);const merger=audio.createChannelMerger(2);source.connect(splitter);for(let c=0;c<2;c++){const a=audio.createAnalyser();a.fftSize=4096;splitter.connect(a,c);a.connect(merger,0,c);window.analysers.push(a);}const mute=audio.createGain();mute.gain.value=.00001;merger.connect(mute).connect(audio.destination);window.remoteTrack=e.track;};
  const description=await pc.createOffer();description.sdp=description.sdp.replace(/a=fmtp:(\d+) ([^\r\n]*)/g,(m,pt,params)=>description.sdp.includes(`a=rtpmap:${pt} opus/48000/2`)?`a=fmtp:${pt} ${params};stereo=1;sprop-stereo=1;maxaveragebitrate=192000`:m);
  await pc.setLocalDescription(description);await new Promise((resolve,reject)=>{if(pc.iceGatheringState==='complete')return resolve();const timeout=setTimeout(()=>reject(Error('Browser ICE gathering timeout')),10000);pc.onicegatheringstatechange=()=>{if(pc.iceGatheringState==='complete'){clearTimeout(timeout);resolve();}};});return pc.localDescription.sdp;
 });
 const offerFile=path.join(temp,'offer.sdp'),answerFile=path.join(temp,'answer.sdp');fs.writeFileSync(offerFile,offer);
 sender=spawn(process.argv[2],['--browser',offerFile,answerFile],{stdio:['ignore','pipe','pipe']});let senderError='';sender.stderr.on('data',b=>{senderError+=b.toString();process.stderr.write(b);});sender.stdout.on('data',b=>process.stdout.write(b));
 const completion=new Promise((resolve,reject)=>{sender.once('error',reject);sender.once('exit',code=>resolve(code));});
 for(let n=0;!fs.existsSync(answerFile)&&n<300;n++)await new Promise(r=>setTimeout(r,50));if(!fs.existsSync(answerFile))throw Error('Native answer missing: '+senderError);
 const answerText=fs.readFileSync(answerFile,'utf8');console.log('Native answer candidate types:',answerText.match(/typ (host|srflx|relay)/g));await page.evaluate(answer=>pc.setRemoteDescription({type:'answer',sdp:answer}),answerText);
 try{await page.waitForFunction(()=>pc.connectionState==='connected'&&analysers.length===2,null,{timeout:15000});}catch(e){console.error('Browser route state',await page.evaluate(()=>({ice:pc.iceConnectionState,connection:pc.connectionState,gathering:pc.iceGatheringState})));throw e;}await page.waitForTimeout(2000);
 const results=await page.evaluate(async()=>{
  function measure(analyser){const d=new Float32Array(analyser.fftSize);analyser.getFloatTimeDomainData(d);let power=0;for(const v of d)power+=v*v;function magnitude(f){let re=0,im=0;for(let n=0;n<d.length;n++){const phase=2*Math.PI*f*n/audio.sampleRate;re+=d[n]*Math.cos(phase);im+=d[n]*Math.sin(phase);}return Math.hypot(re,im);}return {rms:Math.sqrt(power/d.length),at440:magnitude(440),at880:magnitude(880)};}
  const channels=analysers.map(measure);let bytes=0;const inbound=[];for(const stat of (await pc.getStats()).values())if(stat.type==='inbound-rtp'&&stat.kind==='audio'){bytes+=stat.bytesReceived;inbound.push(stat);}return {channels,bytes,inbound,audioState:audio.state,audioTime:audio.currentTime,trackMuted:remoteTrack.muted,playerPaused:player.paused,playError:window.playError};
 });
 if(results.bytes<1000||results.channels.some(x=>x.rms<.05)||results.channels[0].at440<5*results.channels[0].at880||results.channels[1].at880<5*results.channels[1].at440)throw Error('Stereo delivery check failed: '+JSON.stringify(results));
 const code=await completion;if(code!==0)throw Error('Native sender failed: '+senderError);
 console.log('PASS: native WebRTC -> Chromium, decoded stereo 440Hz left / 880Hz right',JSON.stringify(results));
 }finally{sender?.kill();await browser?.close();server.close();fs.rmSync(temp,{recursive:true,force:true});}
})().catch(e=>{console.error(e);process.exitCode=1;});
