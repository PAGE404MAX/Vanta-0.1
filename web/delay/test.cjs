const {JSDOM}=require('jsdom');
const {RenderingAudioContext}=require('web-audio-engine');
const fs=require('fs'),assert=require('assert');
const contexts=[];let errors=[];
function boot(){return new JSDOM(fs.readFileSync(require('path').join(__dirname,'index.html'),'utf8'),{runScripts:'dangerously',pretendToBeVisual:true,beforeParse(w){w.AudioContext=function(){const ctx=new RenderingAudioContext({sampleRate:44100,numberOfChannels:2});contexts.push(ctx);return ctx};w.requestAnimationFrame=()=>0;w.HTMLCanvasElement.prototype.getContext=()=>({setTransform(){},clearRect(){},fillRect(){},fillText(){},beginPath(){},moveTo(){},lineTo(){},stroke(){}});w.addEventListener('error',e=>errors.push(e.error));}})}
(async()=>{
 const dom=boot(),w=dom.window,d=w.document;
 assert.equal(d.querySelectorAll('.control').length,8);
 await w.eval('audio()');
 // Render an impulse through the exact UI graph, not a separate delay implementation.
 w.eval(`vals.mix=100; vals.output=0; vals.feedback=50; vals.low=20; vals.high=20000; update(); const test=ctx.createBuffer(2,44100,44100);test.getChannelData(0)[0]=1;test.getChannelData(1)[0]=1;install(test,'impulse.wav');start();`);
 const ctx=contexts[0];ctx.processTo(2);const out=ctx.exportAsAudioData().channelData[0];
 const energy=(from,to)=>{let sum=0;for(let i=Math.floor(from*44100);i<Math.floor(to*44100);i++)sum+=out[i]*out[i];return sum};
 assert(energy(.29,.34)>1e-4,'first 300ms echo exists');
 assert(energy(.59,.64)>1e-6,'feedback creates second echo');
 assert(energy(.59,.64)<energy(.29,.34),'repeats decay');
 assert(out.every(Number.isFinite),'audio stays finite');
 d.getElementById('mode').value='ping';d.getElementById('mode').onchange();
 d.getElementById('bypass').click();assert.equal(d.getElementById('bypass').getAttribute('aria-pressed'),'true');
 d.getElementById('sync').value='.75';d.getElementById('sync').onchange();assert.equal(d.getElementById('delay').disabled,true);
 d.getElementById('bpm').value=0;d.getElementById('bpm').onchange();assert(+d.getElementById('bpm').value>=30);
 d.getElementById('reset').click();assert.equal(d.getElementById('bypass').getAttribute('aria-pressed'),'false');assert.equal(d.getElementById('delay').disabled,false);
 d.getElementById('stop').click();assert.equal(d.getElementById('play').textContent,'▶ PLAY');
 assert.equal(errors.length,0,errors.map(String).join('\n'));dom.window.close();
 console.log('PASS: UI initialization, exact graph impulse rendering, delay timing, feedback decay, finite samples, routing, bypass, sync, BPM clamping, reset and stop.');
})().catch(e=>{console.error(e);process.exit(1)});
