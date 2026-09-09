#pragma once
// Web UI (served from flash). Plain HTML + JS, no external resources.
static const char WEB_PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="ru"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>E-ink табло</title>
<style>
 body{font-family:system-ui,Segoe UI,Arial,sans-serif;margin:0;background:#f3f3f0;color:#222}
 header{background:#1d2b3a;color:#fff;padding:12px 20px;font-size:18px;display:flex;justify-content:space-between;align-items:center}
 header small{opacity:.7;font-size:13px}
 main{max-width:960px;margin:0 auto;padding:16px;display:grid;gap:16px;grid-template-columns:1fr 1fr}
 @media(max-width:720px){main{grid-template-columns:1fr}}
 .card{background:#fff;border-radius:10px;padding:16px;box-shadow:0 1px 3px rgba(0,0,0,.12)}
 h2{margin:0 0 10px;font-size:16px}
 textarea{width:100%;box-sizing:border-box;height:110px;font-size:16px;padding:8px;border:1px solid #ccc;border-radius:6px;resize:vertical}
 .row{display:flex;gap:12px;align-items:center;margin:10px 0;flex-wrap:wrap;font-size:14px}
 input[type=number]{width:80px;padding:6px;border:1px solid #ccc;border-radius:6px}
 button{padding:10px 16px;border:0;border-radius:6px;background:#1d2b3a;color:#fff;font-size:15px;cursor:pointer}
 button.secondary{background:#888}
 button:disabled{opacity:.5;cursor:default}
 .preview{margin-top:10px;background:#fff;border:1px solid #bbb;aspect-ratio:1360/480;display:flex;align-items:center;justify-content:center;text-align:center;white-space:pre-wrap;overflow:hidden;font-family:"PT Sans",Arial,sans-serif;line-height:1.15;padding:2%;box-sizing:border-box}
 .status{font-size:13px;color:#666;min-height:18px;margin-top:6px}
 .busy{color:#b35c00}
 #led{background:#555;padding:8px 14px;font-size:14px;margin-left:16px}
 #led.on{background:#e0a800;color:#222}
</style></head><body>
<header><span>E-ink табло <button id="led" title="GPIO47">💡 Лампа: …</button>
 <input type="color" id="ledc" value="#ffffff" title="цвет" style="vertical-align:middle;height:30px;width:40px;border:0;background:none">
 <input type="range" id="ledb" min="1" max="255" value="128" title="яркость" style="vertical-align:middle;width:110px">
 <select id="ledfx" title="эффект" style="vertical-align:middle;padding:5px;border-radius:6px;border:0">
  <option value="solid">Сплошной</option><option value="wave">Волна</option><option value="load">Загрузка</option>
  <option value="converge">Схождение в точку</option><option value="comet">Комета</option><option value="breathe">Дыхание</option><option value="rainbow">Радуга</option>
 </select>
 <input type="range" id="ledsp" min="1" max="10" value="5" title="скорость" style="vertical-align:middle;width:80px"></span><small id="net"></small></header>
<main>
 <section class="card" data-screen="1">
  <h2>Экран 1</h2>
  <textarea placeholder="Текст для экрана 1. Enter — новая строка."></textarea>
  <div class="row">
   <label>Размер, px <input type="number" min="0" max="440" value="0" title="0 = подобрать автоматически"></label>
   <label><input type="checkbox"> жирный</label>
  </div>
  <div class="preview"></div>
  <div class="row"><button class="show">Показать на экране 1</button><button class="secondary clear">Очистить</button></div>
  <div class="status"></div>
 </section>
 <section class="card" data-screen="2">
  <h2>Экран 2</h2>
  <textarea placeholder="Текст для экрана 2. Enter — новая строка."></textarea>
  <div class="row">
   <label>Размер, px <input type="number" min="0" max="440" value="0" title="0 = подобрать автоматически"></label>
   <label><input type="checkbox"> жирный</label>
  </div>
  <div class="preview"></div>
  <div class="row"><button class="show">Показать на экране 2</button><button class="secondary clear">Очистить</button></div>
  <div class="status"></div>
 </section>
 <section class="card" id="imgcard" style="grid-column:1/-1">
  <h2>Картинка на экран</h2>
  <div class="row">
   <input type="file" id="imgfile" accept="image/*">
   <label><input type="checkbox" id="imginv"> инвертировать (белое → чёрное)</label>
   <label><input type="checkbox" id="imgdith"> дизеринг</label>
   <label>порог <input type="range" id="imgthr" min="1" max="254" value="128" style="width:110px"></label>
   <label>поля, px <input type="number" id="imgmargin" min="0" max="200" value="40"></label>
  </div>
  <canvas id="imgcanvas" width="1360" height="480" style="width:100%;border:1px solid #bbb;background:#fff;aspect-ratio:1360/480"></canvas>
  <div class="row"><button id="imgsend1">На экран 1</button><button id="imgsend2">На экран 2</button></div>
  <div class="status" id="imgstatus">Выбери файл: PNG, JPG, SVG. Прозрачный фон станет белым.</div>
 </section>
</main>
<script>
// ---- image upload: fit into 1360x480, threshold/dither in the browser, POST 81600 bytes to /image
const imgc=document.getElementById('imgcanvas'),ictx=imgc.getContext('2d');let imgSrc=null;
const imgOpts=['imginv','imgdith','imgthr','imgmargin'].map(id=>document.getElementById(id));
document.getElementById('imgfile').addEventListener('change',e=>{const f=e.target.files[0];if(!f)return;const im=new Image();
 im.onload=()=>{imgSrc=im;renderImg()};im.src=URL.createObjectURL(f)});
imgOpts.forEach(el=>el.addEventListener('input',renderImg));
function renderImg(){if(!imgSrc)return;const W=1360,H=480,m=+imgOpts[3].value,inv=imgOpts[0].checked,dith=imgOpts[1].checked,thr=+imgOpts[2].value;
 ictx.fillStyle='#fff';ictx.fillRect(0,0,W,H);
 const sc=Math.min((W-2*m)/imgSrc.width,(H-2*m)/imgSrc.height),w=Math.round(imgSrc.width*sc),h=Math.round(imgSrc.height*sc);
 const off=document.createElement('canvas');off.width=w;off.height=h;const octx=off.getContext('2d');
 octx.drawImage(imgSrc,0,0,w,h);const d=octx.getImageData(0,0,w,h),p=d.data;
 for(let i=0;i<p.length;i+=4){const a=p[i+3]/255;let g=(0.299*p[i]+0.587*p[i+1]+0.114*p[i+2]);if(inv)g=255-g;g=g*a+255*(1-a);p[i]=p[i+1]=p[i+2]=g;p[i+3]=255}
 octx.putImageData(d,0,0);ictx.drawImage(off,(W-w)/2,(H-h)/2);
 const fd=ictx.getImageData(0,0,W,H),q=fd.data,g=new Float32Array(W*H);
 for(let i=0;i<W*H;i++)g[i]=q[i*4];
 for(let y=0;y<H;y++)for(let x=0;x<W;x++){const i=y*W+x,o=g[i],v=o<thr?0:255;g[i]=v;if(dith){const e=o-v;
  if(x+1<W)g[i+1]+=e*7/16;if(y+1<H){if(x>0)g[i+W-1]+=e*3/16;g[i+W]+=e*5/16;if(x+1<W)g[i+W+1]+=e*1/16}}}
 for(let i=0;i<W*H;i++){q[i*4]=q[i*4+1]=q[i*4+2]=g[i]}ictx.putImageData(fd,0,0)}
async function sendImg(n){if(!imgSrc){document.getElementById('imgstatus').textContent='Сначала выбери файл';return}
 const W=1360,H=480,q=ictx.getImageData(0,0,W,H).data,bytes=new Uint8Array(W/8*H);
 for(let y=0;y<H;y++)for(let x=0;x<W;x++)if(q[(y*W+x)*4]<128)bytes[y*(W/8)+(x>>3)]|=0x80>>(x&7);
 const fdata=new FormData();fdata.append('image',new Blob([bytes]),'frame.bin');const st=document.getElementById('imgstatus');
 st.textContent='Отправляю…';try{const r=await fetch('/image?screen='+n,{method:'POST',body:fdata});st.textContent=r.status==202?'Принято, экран обновляется ~20 с':'Ошибка '+r.status+': '+await r.text()}catch(e){st.textContent='Нет связи с платой'}}
document.getElementById('imgsend1').onclick=()=>sendImg(1);document.getElementById('imgsend2').onclick=()=>sendImg(2);
const cards=[...document.querySelectorAll('.card')];
function ui(c){return{ta:c.querySelector('textarea'),size:c.querySelector('input[type=number]'),bold:c.querySelector('input[type=checkbox]'),
 prev:c.querySelector('.preview'),st:c.querySelector('.status'),btns:c.querySelectorAll('button'),n:c.dataset.screen}}
function preview(c){const u=ui(c);const w=u.prev.clientWidth;const scale=w/1360;
 u.prev.textContent=u.ta.value||' ';u.prev.style.fontWeight=u.bold.checked?'bold':'normal';
 let px=+u.size.value;if(!px){const lines=(u.ta.value||' ').split('\n');const n=lines.length;const longest=Math.max(...lines.map(l=>l.length),1);
  px=Math.min(440,(480-32)/(n*1.15),(1300/longest)*1.9)}
 u.prev.style.fontSize=(px*scale)+'px'}
cards.forEach(c=>{const u=ui(c);['input','change'].forEach(e=>{u.ta.addEventListener(e,()=>preview(c));u.size.addEventListener(e,()=>preview(c));u.bold.addEventListener(e,()=>preview(c))});
 c.querySelector('.show').onclick=()=>send('/show',c);c.querySelector('.clear').onclick=()=>send('/clear',c)});
async function send(path,c){const u=ui(c);const p=new URLSearchParams({screen:u.n,text:u.ta.value,size:u.size.value,bold:u.bold.checked?1:0});
 u.st.textContent='Отправляю…';try{const r=await fetch(path,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:p});
 u.st.textContent=r.status==202?'Принято, экран обновляется ~20 с':'Ошибка '+r.status}catch(e){u.st.textContent='Нет связи с платой'}}
const ledBtn=document.getElementById('led');
function showLed(on){ledBtn.textContent='💡 Лампа: '+(on?'ВКЛ':'выкл');ledBtn.classList.toggle('on',on)}
ledBtn.onclick=async()=>{try{const s=await(await fetch('/led?state=toggle',{method:'POST'})).json();showLed(s.led)}catch(e){}};
const ledC=document.getElementById('ledc'),ledB=document.getElementById('ledb');let ledInit=false;
async function sendLedColor(){try{await fetch('/led?color='+ledC.value.slice(1)+'&bright='+ledB.value,{method:'POST'})}catch(e){}}
ledC.addEventListener('change',sendLedColor);ledB.addEventListener('change',sendLedColor);
const ledFx=document.getElementById('ledfx'),ledSp=document.getElementById('ledsp');
async function sendLedFx(){try{await fetch('/led?state=on&effect='+ledFx.value+'&speed='+ledSp.value,{method:'POST'})}catch(e){}}
ledFx.addEventListener('change',sendLedFx);ledSp.addEventListener('change',sendLedFx);
async function poll(){try{const s=await(await fetch('/status')).json();
 document.getElementById('net').textContent=(s.ip?'IP '+s.ip+' · ':'')+s.board;showLed(s.led);
 if(!ledInit){ledC.value='#'+s.ledColor.toLowerCase();ledB.value=s.ledBright;ledFx.value=s.ledEffect;ledSp.value=s.ledSpeed;ledInit=true}
 cards.forEach((c,i)=>{const u=ui(c);const b=s.busy&&s.busyScreen==i+1;u.btns.forEach(x=>x.disabled=s.busy);
  if(b){u.st.textContent='Обновляется…';u.st.classList.add('busy')}else{u.st.classList.remove('busy');if(u.st.textContent=='Обновляется…')u.st.textContent='Готово'}
  if(!u.ta.dataset.init){u.ta.value=s.screens[i].text;u.size.value=s.screens[i].size;u.bold.checked=s.screens[i].bold;u.ta.dataset.init=1;preview(c)}})}catch(e){}
 setTimeout(poll,1500)}
poll();window.addEventListener('resize',()=>cards.forEach(preview));
</script></body></html>)HTML";
