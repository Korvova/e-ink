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
</style></head><body>
<header><span>E-ink табло</span><small id="net"></small></header>
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
</main>
<script>
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
async function poll(){try{const s=await(await fetch('/status')).json();
 document.getElementById('net').textContent=(s.ip?'IP '+s.ip+' · ':'')+s.board;
 cards.forEach((c,i)=>{const u=ui(c);const b=s.busy&&s.busyScreen==i+1;u.btns.forEach(x=>x.disabled=s.busy);
  if(b){u.st.textContent='Обновляется…';u.st.classList.add('busy')}else{u.st.classList.remove('busy');if(u.st.textContent=='Обновляется…')u.st.textContent='Готово'}
  if(!u.ta.dataset.init){u.ta.value=s.screens[i].text;u.size.value=s.screens[i].size;u.bold.checked=s.screens[i].bold;u.ta.dataset.init=1;preview(c)}})}catch(e){}
 setTimeout(poll,1500)}
poll();window.addEventListener('resize',()=>cards.forEach(preview));
</script></body></html>)HTML";
