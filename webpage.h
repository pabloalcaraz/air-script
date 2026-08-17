#pragma once
#include <Arduino.h>

// Dashboard completo en PROGMEM: HTML + CSS + JS, sin CDN ni frameworks.
// Todo debe funcionar con el router sin internet, asi que no puede haber
// una sola referencia externa.

static const char PAGINA_HTML[] PROGMEM = R"HTMLPAGE(<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Air Script</title>
<!-- Icono vacio: sin esto el navegador pide /favicon.ico en cada carga y
     el ESP32 se come una peticion extra para devolver un 404. -->
<link rel="icon" href="data:,">
<style>
:root{
 --bg:#0e1116;--panel:#171c24;--linea:#252c38;--txt:#e8ecf2;--soft:#95a0b3;
 --ok:#3fb27f;--aviso:#e0a33e;--malo:#e2564d;--azul:#5aa9e6;
}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--txt);
 font:16px/1.45 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif}
header{padding:16px 18px 10px;border-bottom:1px solid var(--linea)}
h1{margin:0;font-size:15px;letter-spacing:.12em;text-transform:uppercase;color:var(--soft)}
#veredicto{margin-top:8px;font-size:26px;font-weight:650}
#veredicto.n1{color:var(--aviso)}
#veredicto.n2{color:var(--malo)}
#sub{font-size:13px;color:var(--soft);margin-top:4px}
nav{display:flex;gap:4px;padding:10px 14px;border-bottom:1px solid var(--linea);
 position:sticky;top:0;background:var(--bg);z-index:5}
nav button{flex:1;padding:10px;background:none;border:1px solid transparent;
 border-radius:8px;color:var(--soft);font:inherit;font-size:15px;cursor:pointer}
nav button.on{background:var(--panel);border-color:var(--linea);color:var(--txt)}
main{padding:16px;max-width:960px;margin:0 auto}
/* 6 tarjetas: 3 columnas dan dos filas exactas y valores grandes, legibles
   a un metro. Es un panel de pared, no una tabla. */
.grid{display:grid;gap:12px;grid-template-columns:repeat(2,1fr)}
@media(min-width:680px){.grid{grid-template-columns:repeat(3,1fr)}}
.card{background:var(--panel);border:1px solid var(--linea);border-left:4px solid var(--linea);
 border-radius:10px;padding:14px}
.card.n1{border-left-color:var(--aviso)}
.card.n2{border-left-color:var(--malo)}
.card .n{font-size:13px;color:var(--soft);text-transform:uppercase;letter-spacing:.06em}
.card .v{font-size:34px;font-weight:650;line-height:1.1;margin:6px 0 2px}
.card .u{font-size:13px;color:var(--soft)}
.card .e{font-size:12px;font-weight:700;margin-top:8px;letter-spacing:.05em}
.card.n0 .e{color:var(--ok)}
.card.n1 .e{color:var(--aviso)}
.card.n2 .e{color:var(--malo)}
.card.off{opacity:.72}
.card.off .v{font-size:19px;color:var(--malo)}
.bar{display:flex;flex-wrap:wrap;gap:8px;align-items:center;margin-bottom:14px}
.bar button,select{background:var(--panel);color:var(--txt);border:1px solid var(--linea);
 border-radius:7px;padding:7px 12px;font:inherit;font-size:14px;cursor:pointer}
.bar button.on{border-color:var(--azul);color:var(--azul)}
canvas{width:100%;height:320px;background:var(--panel);
 border:1px solid var(--linea);border-radius:10px}
.leg{display:flex;flex-wrap:wrap;gap:10px;margin-top:12px;font-size:13px}
.leg label{display:flex;align-items:center;gap:6px;background:var(--panel);
 border:1px solid var(--linea);border-radius:20px;padding:5px 12px;cursor:pointer}
.leg i{width:14px;height:3px;border-radius:2px;display:inline-block}
table{width:100%;border-collapse:collapse;font-size:14px}
td{padding:9px 6px;border-bottom:1px solid var(--linea);vertical-align:top}
td.k{color:var(--soft);width:38%}
.pill{font-size:12px;font-weight:700;padding:2px 8px;border-radius:20px;
 border:1px solid currentColor}
.s-ok{color:var(--ok)}.s-warn{color:var(--aviso)}.s-bad{color:var(--malo)}
h2{font-size:13px;text-transform:uppercase;letter-spacing:.1em;color:var(--soft);
 margin:22px 0 8px}
.panel{background:var(--panel);border:1px solid var(--linea);border-radius:10px;padding:6px 14px}
#ventana{margin-bottom:14px;border-left:4px solid var(--linea)}
#ventana.v1{border-left-color:var(--aviso)}
#ventana.v2{border-left-color:var(--malo)}
#log{list-style:none;margin:0;padding:0;font-size:14px}
#log li{padding:9px 4px;border-bottom:1px solid var(--linea);display:flex;gap:10px}
#log li:last-child{border:0}
#log time{color:var(--soft);font-variant-numeric:tabular-nums;flex:none;font-size:13px}
#log .m{flex:1}
#log .lv{flex:none;font-size:11px;font-weight:700;letter-spacing:.05em}
.l0 .lv{color:var(--soft)}.l1 .lv{color:var(--aviso)}.l2 .lv{color:var(--malo)}
.rep{font-size:11px;color:var(--soft);border:1px solid var(--linea);
 border-radius:10px;padding:0 6px;margin-left:6px}
.err{color:var(--malo);font-size:14px}
.ayuda{color:var(--soft);font-size:13px;margin:10px 0}
input[type=number],input[type=text]{background:var(--bg);color:var(--txt);
 border:1px solid var(--linea);border-radius:7px;padding:7px 10px;
 font:inherit;font-size:14px;width:90px}
input[type=text]{flex:1;min-width:150px;width:auto}
#bcal{border-color:var(--aviso);color:var(--aviso)}
.okmsg{color:var(--ok);font-size:14px;margin:10px 0}
/* Barra apilada de reparto de tiempo. Responde a "cuánto rato he respirado
   aire malo", que es la pregunta honesta; el pico máximo no la responde. */
.stack{display:flex;height:14px;border-radius:7px;overflow:hidden;
 background:var(--bg);margin:6px 0}
.stack i{display:block;min-width:0}
.b0{background:var(--ok)}.b1{background:var(--aviso)}.b2{background:var(--malo)}
.pctleg{display:flex;gap:14px;font-size:12px;color:var(--soft);margin-bottom:16px}
.pctleg b{font-weight:600}
.mag{font-size:14px;margin-top:14px}
.mag:first-child{margin-top:2px}
.tend{font-size:15px;color:var(--soft);font-weight:400;vertical-align:middle}
table.num td{text-align:right;font-variant-numeric:tabular-nums}
table.num td:first-child{text-align:left;color:var(--soft)}
table.num th{font-size:11px;color:var(--soft);text-transform:uppercase;
 letter-spacing:.06em;font-weight:600;text-align:right;padding:4px 6px;
 border-bottom:1px solid var(--linea)}
table.num th:first-child{text-align:left}
footer{padding:8px 16px 30px;color:var(--soft);font-size:12px;text-align:center}
@media(max-width:520px){.card .v{font-size:27px}.card .u{font-size:12px}}
</style>
</head>
<body>
<header>
 <h1>Air Script</h1>
 <div id="veredicto">Conectando...</div>
 <div id="sub">&nbsp;</div>
</header>
<nav>
 <button data-t="ahora" class="on">Ahora</button>
 <button data-t="hist">Histórico</button>
 <button data-t="est">Datos</button>
 <button data-t="sis">Sistema</button>
</nav>
<main>
 <section id="ahora">
  <div class="panel" id="ventana"></div>
  <div class="grid" id="cards"></div>
  <h2>Calculado a partir de lo anterior</h2>
  <div class="panel"><table id="deriv"></table></div>
  <h2>Aire de fuera</h2>
  <div class="panel">
   <table id="ext" class="num"></table>
   <p class="ayuda" id="extnota"></p>
  </div>
 </section>

 <section id="hist" hidden>
  <div class="bar">
   <button data-r="1h">1 h</button>
   <button data-r="6h">6 h</button>
   <button data-r="24h" class="on">24 h</button>
   <span id="hinfo" style="color:var(--soft);font-size:13px"></span>
  </div>
  <canvas id="g"></canvas>
  <div class="leg" id="leg"></div>
 </section>

 <section id="est" hidden>
  <div class="bar">
   <button data-e="1h">1 h</button>
   <button data-e="6h">6 h</button>
   <button data-e="24h" class="on">24 h</button>
   <span id="einfo" style="color:var(--soft);font-size:13px"></span>
  </div>

  <h2>Exposición — media móvil de 24 h</h2>
  <div class="panel">
   <table id="m24"></table>
   <p class="ayuda">Los límites de la OMS 2021 (PM2.5 15 µg/m³, PM10 45 µg/m³)
    son <b>medias de 24 horas</b>, no valores instantáneos. Por eso el titular
    de la cabecera usa esta media y las tarjetas usan la lectura del momento:
    freír dos minutos dispara el instantáneo sin que tu exposición diaria se
    mueva apenas.</p>
  </div>

  <h2>Ventilación</h2>
  <div class="panel">
   <table id="vent"></table>
   <p class="ayuda">Las renovaciones por hora salen de lo rápido que cae el CO₂
    cuando abres, comparado con el fondo atmosférico. Solo se calcula si hay una
    bajada limpia de al menos 10 minutos; si no la hay, no se enseña un número
    inventado.</p>
  </div>

  <h2>Resumen de la ventana</h2>
  <div class="panel"><table id="tstats" class="num"></table></div>

  <h2>Reparto del tiempo por nivel</h2>
  <div class="panel">
   <div class="pctleg"><span><b class="s-ok">■</b> correcto</span>
    <span><b class="s-warn">■</b> vigilar</span>
    <span><b class="s-bad">■</b> malo</span></div>
   <div id="pcts"></div>
  </div>
 </section>

 <section id="sis" hidden>
  <h2>Estado de los subsistemas</h2>
  <div class="panel"><table id="salud"></table></div>
  <h2>Recursos</h2>
  <div class="panel"><table id="recursos"></table></div>
  <h2>Flash</h2>
  <div class="panel">
   <p>Uso: <span id="flashUso">-</span></p>
   <p>Escrituras NVS/EEPROM desde el arranque: <span id="flashEscrituras">-</span></p>
  </div>
  <h2>Backup en la nube</h2>
  <div class="panel">
   <p>Estado: <span id="cloudEstado">-</span></p>
   <p>Token: <span id="cloudTokenEstado">-</span></p>
   <p>En cola: <span id="cloudCola">-</span></p>
   <div class="bar">
    <input id="cloudUrl" type="url" placeholder="https://…grafana.net/…">
    <input id="cloudUser" type="text" placeholder="Usuario/Instance ID">
   </div>
   <div class="bar">
    <input id="cloudToken" type="password"
     placeholder="Token nuevo (deja en blanco para conservar el guardado)">
    <label><input id="cloudOn" type="checkbox"> Activo</label>
    <button id="bcloud">Guardar</button>
   </div>
   <div id="cloudMsg"></div>
  </div>
  <h2>Diagnóstico del SCD41</h2>
  <div class="panel">
   <table id="scddiag"></table>
   <p class="ayuda">Si el sensor deja de entregar datos, el aparato lo reinicia
    solo cada minuto. Estos botones son para forzarlo a mano o para preguntarle
    al chip si está averiado.</p>
   <div class="bar">
    <button id="breset">Reiniciar sensor</button>
    <button id="btest">Autotest (tarda 10 s)</button>
    <button id="basc">Autocalibración</button>
   </div>
   <div id="scdmsg"></div>
  </div>

  <h2>Localización del aire exterior</h2>
  <div class="panel">
   <table id="extinfo"></table>
   <p class="ayuda" id="extdisc">El dato de fuera <b>no sale de una estación de medida</b>:
    es el modelo CAMS de Copernicus sobre una rejilla de 11 km, con valores
    horarios que se recalculan dos veces al día. Sirve para saber si fuera está
    peor que dentro; no para saber cuánto contamina tu calle.</p>
   <div class="bar">
    <input id="qtoken" type="text" placeholder="Token AQICN (opcional)">
    <button id="btoken">Guardar token</button>
   </div>
   <p class="ayuda">Gratis en aqicn.org/data-platform/register. Sin token, el
    aparato usa directamente el modelo CAMS de Open-Meteo.</p>
   <div id="tokenmsg"></div>
   <div class="bar">
    <input id="qlugar" type="text" placeholder="Buscar ciudad, barrio...">
    <button id="bbuscar">Buscar</button>
   </div>
   <p class="ayuda">La búsqueda la hace tu navegador, no el aparato. Los atajos
    de abajo funcionan aunque no tengas internet.</p>
   <div class="bar" id="presets"></div>
   <div id="extres"></div>
   <div id="extmsg"></div>
  </div>

  <h2>Calibración del CO₂</h2>
  <div class="panel">
   <p class="ayuda">Solo si el sensor marca de más. Antes de pulsar:
    <b>deja el aparato al aire libre (ventana, balcón, calle) y aléjate
    3 minutos</b> — tu respiración falsea la referencia. El aire exterior
    son unos 420 ppm.</p>
   <div class="bar">
    <input id="ppm" type="number" value="420" min="350" max="2000" step="10">
    <span class="ayuda">ppm de referencia</span>
    <button id="bcal">Calibrar ahora</button>
   </div>
   <div id="calmsg"></div>
  </div>

  <h2>Log de eventos</h2>
  <div class="bar">
   <button data-f="0" class="on">Todo</button>
   <button data-f="1">Avisos+</button>
   <button data-f="2">Solo errores</button>
  </div>
  <div class="panel"><ul id="log"></ul></div>
 </section>
</main>
<footer id="pie"></footer>

<script>
"use strict";
const $=s=>document.querySelector(s);
const CARDS=[
 {k:"pm1", g:"pms",t:"PM1.0",u:"µg/m³"},
 {k:"pm25",g:"pms",t:"PM2.5",u:"µg/m³",n:"n_pm25"},
 {k:"pm10",g:"pms",t:"PM10", u:"µg/m³",n:"n_pm10"},
 {k:"co2", g:"scd",t:"CO₂",  u:"ppm",  n:"n_co2"},
 {k:"temp",g:"scd",t:"Temp", u:"°C",   d:1},
 {k:"hum", g:"scd",t:"Humedad",u:"%",  n:"n_hum", d:1}
];
const SERIES=[
 {k:"pm25",t:"PM2.5",c:"#5aa9e6",eje:"L",on:1},
 {k:"co2", t:"CO₂",  c:"#e0a33e",eje:"R",on:1},
 {k:"pm1", t:"PM1.0",c:"#7c8cf8",eje:"L",on:0},
 {k:"pm10",t:"PM10", c:"#57c8b8",eje:"L",on:0},
 {k:"temp",t:"Temp", c:"#e2564d",eje:"L",on:0},
 {k:"hum", t:"Hum",  c:"#a97ce0",eje:"L",on:0}
];
const MAG=[
 {k:"pm1", t:"PM1.0",  u:"µg/m³",d:1},
 {k:"pm25",t:"PM2.5",  u:"µg/m³",d:1},
 {k:"pm10",t:"PM10",   u:"µg/m³",d:1},
 {k:"co2", t:"CO₂",    u:"ppm",  d:0},
 {k:"temp",t:"Temp",   u:"°C",   d:1},
 {k:"hum", t:"Humedad",u:"%",    d:1}
];
// Suelo de ruido de cada magnitud, por hora. Por debajo de esto la pendiente
// la marca el propio sensor, no el aire: pintar una flecha sería mentir.
const RUIDO={pm1:2,pm25:2,pm10:3,co2:20,temp:0.3,hum:1};
// Atajos de localización. Van con coordenadas fijas a propósito: el buscador
// necesita internet en el navegador y estos tienen que funcionar sin él.
const LUGARES=[
 {n:"Madrid",   lat:40.4168,lon:-3.7038},
 {n:"Barcelona",lat:41.3874,lon: 2.1686},
 {n:"Valencia", lat:39.4699,lon:-0.3763},
 {n:"Sevilla",  lat:37.3891,lon:-5.9845},
 {n:"Alicante", lat:38.3452,lon:-0.4810}
];
const ETIQ=["OK","! VIGILAR","!! MALO"];
let horaOk=false, rango="24h", datos=null, filtro=0, ascOn=0;
let rangoEst="24h", tend={};
let ext=null, ultNow=null;

// ---- utilidades ----
// Sin NTP los ts son segundos desde arranque, no epoch: se muestran distinto
// para no mentir con una hora inventada.
function hora(ts){
 if(!horaOk) return "+"+Math.floor(ts/60)+"m";
 const d=new Date(ts*1000);
 return String(d.getHours()).padStart(2,"0")+":"+String(d.getMinutes()).padStart(2,"0");
}
function fallo(el,txt){el.innerHTML='<div class="err">'+txt+'</div>';}
// Los nombres de ciudad vienen de un servicio externo: van escapados antes de
// tocar innerHTML, igual que los mensajes del log.
const esc=s=>String(s).replace(/[<>&"]/g,
 c=>({"<":"&lt;",">":"&gt;","&":"&amp;",'"':"&quot;"}[c]));
// Compartidas por pintarExt() y pintarVentana(): "hay dato" y "última
// lectura interior de esta magnitud, o null si el sensor está caído".
const hay=v=>v!==null&&v!==undefined;
const dentro=(g,k)=>(ultNow&&ultNow[g]&&ultNow[g].ok)?ultNow[g][k]:null;
async function api(r){
 const res=await fetch(r,{cache:"no-store"});
 if(!res.ok) throw new Error("HTTP "+res.status);
 return res.json();
}

// ---- pestañas ----
document.querySelectorAll("nav button").forEach(b=>b.onclick=()=>{
 document.querySelectorAll("nav button").forEach(x=>x.classList.toggle("on",x===b));
 ["ahora","hist","est","sis"].forEach(id=>$("#"+id).hidden=(id!==b.dataset.t));
 if(b.dataset.t==="hist") cargarHist();
 if(b.dataset.t==="est")  cargarEst();
 if(b.dataset.t==="sis")  cargarSis();
});

// ---- Ahora ----
// Los umbrales los publica el firmware en /api/now: son los mismos de config.h
// y no pueden discrepar. Estos valores son solo el arranque en frio, hasta que
// llega la primera respuesta.
let UMBRAL={pm25_aviso:15,pm25_malo:35,co2_aviso:800,co2_malo:1200};

function pintarVentana(){
 const el=$("#ventana");
 const pi=dentro("pms","pm25"), pe=ext&&ext.valido&&!ext.rancio?ext.pm25:null;
 const co2=dentro("scd","co2");
 if(!hay(pi)||!hay(pe)){
  el.innerHTML="<b>¿Abrir la ventana?</b> Falta dato "
   +(!hay(pi)?"interior (PMS5003)":"exterior")+" para decidir.";
  el.className="panel"; return;
 }
 const ratio = pe>=1 ? pi/pe : null;
 const fueraMalo = pe>=UMBRAL.pm25_malo;
 const fueraOk   = pe<UMBRAL.pm25_aviso;
 const viciado   = hay(co2)&&co2>=UMBRAL.co2_aviso;
 const muyViciado= hay(co2)&&co2>=UMBRAL.co2_malo;

 let msg, nivel;
 // "fuera malo" es una puerta única: sea cual sea el CO₂, nunca puede caer
 // en una rama que diga "da igual" o "abre" con el aire de fuera por
 // encima del umbral OMS.
 if(fueraMalo){
  if(muyViciado){ msg="Ventila poco y rápido: CO₂ alto, pero fuera también está mal."; nivel=1; }
  else{ msg="No abras: fuera está más contaminado."; nivel=2; }
 }
 // Con pe casi cero el ratio pi/pe dispararía un número absurdo; en ese
 // caso se compara pi contra el umbral absoluto en vez del ratio.
 else if((ratio!==null && ratio>1.25) || (ratio===null && pi>=UMBRAL.pm25_malo)){
  msg="Abre: la fuente de partículas está dentro."; nivel=0;
 }
 // Fuera en aviso (no llega a "malo") no debe silenciar un CO₂ muy alto:
 // se avisa igual, aunque con menos urgencia que el gate de fueraMalo.
 else if(!fueraOk && muyViciado){
  msg="Ventila poco y rápido: CO₂ muy alto, aunque fuera no esté tan mal."; nivel=1;
 }
 else if(fueraOk && viciado){ msg="Abre: para bajar el CO₂, fuera está limpio."; nivel=0; }
 else if(ratio!==null && ratio<0.8 && !viciado){ msg="Mantén cerrada: dentro está mejor que fuera."; nivel=0; }
 else { msg="Da igual: dentro y fuera van parejos, no urge."; nivel=0; }

 el.innerHTML="<b>¿Abrir la ventana?</b> "+msg;
 el.className="panel v"+nivel;
}

function pintarAhora(d){
 horaOk=d.hora_ok; ultNow=d;
 if(d.umbrales) UMBRAL=d.umbrales;
 const v=$("#veredicto");
 v.className="n"+d.peor.nivel;
 v.textContent=d.peor.nivel===0?"Calidad del Aire correcta"
   :(d.peor.nivel===1?"Vigilar: "+d.peor.que:"MALO: "+d.peor.que);
 $("#sub").textContent=(d.hora_ok?"Actualizado "+hora(d.ts):"Sin hora NTP")
   +" · encendido "+dur(d.uptime_s);

 $("#cards").innerHTML=CARDS.map(c=>{
  const g=d[c.g];
  if(!g.ok){
   // El caso "rancio" va primero: es el único en el que el aparato tiene un
  // valor guardado y podría tentar de pintarlo. No se pinta a propósito.
  const m=c.g==="scd"
    ?(g.rancio?"Sin datos desde hace "+g.hace_s+" s"
      :(g.estado===0?"Esperando primer dato":(g.error?"Error "+g.error+" en "+g.etapa
       :(g.i2c?"Responde pero no mide":"No responde en I2C"))))
    :(g.estado===0?"Calentando":"Sin frames");
   return '<div class="card off"><div class="n">'+c.t+'</div><div class="v">—</div>'
        +'<div class="u">'+m+'</div></div>';
  }
  const n=c.n?g[c.n]:0;
  const val=c.d?g[c.k].toFixed(1):g[c.k];
  // PM1.0 y temperatura no tienen umbral: pintarles "OK" fingiría un
  // veredicto que nadie ha emitido. Se muestra el número y ya.
  return '<div class="card n'+n+'"><div class="n">'+c.t+'</div>'
       +'<div class="v">'+val+flecha(c.k)+'</div><div class="u">'+c.u+'</div>'
       +(c.n?'<div class="e">'+ETIQ[n]+'</div>':'')+'</div>';
 }).join("");
 derivados(d);
 // La columna "dentro" del panel exterior sale de estos mismos datos: se
 // repinta aquí para que las dos columnas nunca sean de momentos distintos.
 pintarExt();
 pintarVentana();
}

// Flecha de tendencia de los últimos 30 min. Neutra de color a propósito:
// que el CO₂ suba es malo, que la temperatura suba en invierno no.
function flecha(k){
 const v=tend[k];
 if(v===null||v===undefined) return "";
 const t=' title="'+(v>0?"+":"")+v.toFixed(1)+" por hora (últimos 30 min)"+'"';
 if(Math.abs(v)<RUIDO[k]) return ' <span class="tend"'+t+'>→</span>';
 return ' <span class="tend"'+t+'>'+(v>0?"↑":"↓")+'</span>';
}

// Punto de rocío y humedad absoluta con la fórmula de Magnus. Se calculan en
// el navegador: el firmware no gasta ni un byte y el resultado es el mismo.
function derivados(d){
 const el=$("#deriv");
 let h="";
 if(d.scd.ok){
  const T=d.scd.temp, H=d.scd.hum, a=17.625, b=243.04;
  const g=Math.log(H/100)+a*T/(b+T), td=b*g/(a-g);
  const ha=6.112*Math.exp(17.67*T/(T+243.5))*H*2.1674/(273.15+T);
  h+=fila("Punto de rocío", td.toFixed(1)+" °C"
   +(T-td<3?" <span class='err'>— cualquier superficie fría condensa</span>":""));
  h+=fila("Humedad absoluta", ha.toFixed(1)+" g/m³"
   +" <span class='ayuda'>agua real en el aire, no depende de la temperatura</span>");
 }else{
  h+=fila("Punto de rocío","— sin datos del SCD41");
 }
 // La media de 24 h se enseña aquí porque es la que decide el titular de
 // arriba: si no, el usuario ve "MALO" sin poder comprobar de dónde sale.
 const m=d.m24;
 h+=fila("Exposición PM2.5 (24 h)", m.pm25===null
   ?"aún sin datos suficientes — hace falta 1 h de historial"
   :m.pm25.toFixed(1)+" µg/m³ · límite OMS 15 <span class='pill "
    +["s-ok","s-warn","s-bad"][m.n_pm25]+'">'+ETIQ[m.n_pm25]+"</span>");
 h+=fila("Exposición PM10 (24 h)", m.pm10===null
   ?"aún sin datos suficientes"
   :m.pm10.toFixed(1)+" µg/m³ · límite OMS 45 <span class='pill "
    +["s-ok","s-warn","s-bad"][m.n_pm10]+'">'+ETIQ[m.n_pm10]+"</span>");
 el.innerHTML=h;
}
function dur(s){
 const d=Math.floor(s/86400),h=Math.floor(s%86400/3600),m=Math.floor(s%3600/60);
 return d?d+"d "+h+"h":(h?h+"h "+m+"m":m+"m");
}
async function cargarAhora(){
 try{pintarAhora(await api("/api/now"));}
 catch(e){$("#veredicto").textContent="Sin conexión con el aparato";}
}

// ---- Histórico ----
document.querySelectorAll("[data-r]").forEach(b=>b.onclick=()=>{
 document.querySelectorAll("[data-r]").forEach(x=>x.classList.toggle("on",x===b));
 rango=b.dataset.r; cargarHist();
});
$("#leg").innerHTML=SERIES.map((s,i)=>
 '<label><input type="checkbox" data-s="'+i+'"'+(s.on?" checked":"")+'>'
 +'<i style="background:'+s.c+'"></i>'+s.t+'</label>').join("");
$("#leg").onchange=e=>{SERIES[e.target.dataset.s].on=e.target.checked?1:0;dibujar();};

async function cargarHist(){
 $("#hinfo").textContent="cargando...";
 try{
  datos=await api("/api/history?range="+rango);
  horaOk=datos.hora_ok;
  $("#hinfo").textContent=datos.n+" puntos"+(datos.n?"":" — aún sin datos, "
   +"se guarda una muestra por minuto");
  dibujar();
 }catch(e){$("#hinfo").textContent="error al cargar";}
}

function dibujar(){
 const cv=$("#g"),ctx=cv.getContext("2d");
 // Sin devicePixelRatio el canvas se ve borroso en cualquier móvil.
 const dpr=window.devicePixelRatio||1, W=cv.clientWidth, H=cv.clientHeight;
 cv.width=W*dpr; cv.height=H*dpr; ctx.setTransform(dpr,0,0,dpr,0,0);
 ctx.clearRect(0,0,W,H);
 if(!datos||!datos.n){ctx.fillStyle="#95a0b3";ctx.font="14px system-ui";
  ctx.fillText("Sin datos todavía",14,26);return;}

 const ml=42,mr=46,mt=14,mb=26, w=W-ml-mr, h=H-mt-mb;
 const act=SERIES.filter(s=>s.on);
 // Dos ejes: el CO₂ (400-2000 ppm) aplastaria las partículas (0-50) contra
 // el suelo si compartieran escala.
 const usaL=act.some(s=>s.eje==="L"), usaR=act.some(s=>s.eje==="R");
 const max=e=>{let m=0;act.filter(s=>s.eje===e).forEach(s=>
   (datos[s.k]||[]).forEach(v=>{if(v!==null&&v>m)m=v;}));return m;};
 let maxL=Math.max(max("L")*1.15,10), maxR=Math.max(max("R")*1.15,1000);

 ctx.strokeStyle="#252c38"; ctx.fillStyle="#95a0b3";
 ctx.font="11px system-ui"; ctx.lineWidth=1;
 for(let i=0;i<=4;i++){
  const y=mt+h-h*i/4;
  ctx.beginPath();ctx.moveTo(ml,y);ctx.lineTo(ml+w,y);ctx.stroke();
  // Con escalas pequeñas (0-10 µg/m³) redondear a entero repite etiquetas
  // y miente sobre la posición de la línea.
  if(usaL){ctx.textAlign="right";
   ctx.fillText((maxL*i/4).toFixed(maxL<20?1:0),ml-6,y+4);}
  if(usaR){ctx.textAlign="left";ctx.fillText((maxR*i/4).toFixed(0),ml+w+6,y+4);}
 }
 const t=datos.t||[];
 ctx.textAlign="center";
 for(let i=0;i<=4;i++){
  const idx=Math.min(t.length-1,Math.round((t.length-1)*i/4));
  ctx.fillText(hora(t[idx]),ml+w*i/4,H-8);
 }

 // Umbrales de referencia, punteados y solo si su serie está visible.
 const umbral=(v,e,c)=>{
  const m=e==="L"?maxL:maxR, y=mt+h-h*Math.min(v/m,1);
  ctx.save();ctx.setLineDash([4,4]);ctx.strokeStyle=c;ctx.globalAlpha=.5;
  ctx.beginPath();ctx.moveTo(ml,y);ctx.lineTo(ml+w,y);ctx.stroke();ctx.restore();
 };
 if(act.some(s=>s.k==="pm25")) umbral(15,"L","#e0a33e");
 if(act.some(s=>s.k==="co2"))  umbral(800,"R","#e0a33e");

 const n=datos.n, dx=n>1?w/(n-1):0;
 act.forEach(s=>{
  const arr=datos[s.k]||[], m=s.eje==="L"?maxL:maxR;
  ctx.strokeStyle=s.c; ctx.lineWidth=2; ctx.beginPath();
  let dentro=false;
  for(let i=0;i<n;i++){
   const v=arr[i];
   // null = sensor caído en ese tramo. Se corta la línea en vez de
   // interpolar una medida que nunca se tomó.
   if(v===null||v===undefined){dentro=false;continue;}
   const x=ml+dx*i, y=mt+h-h*Math.min(v/m,1);
   if(dentro)ctx.lineTo(x,y);else{ctx.moveTo(x,y);dentro=true;}
  }
  ctx.stroke();
 });
}
window.addEventListener("resize",()=>{if(!$("#hist").hidden)dibujar();});

// ---- Estadísticas ----
document.querySelectorAll("[data-e]").forEach(b=>b.onclick=()=>{
 document.querySelectorAll("[data-e]").forEach(x=>x.classList.toggle("on",x===b));
 rangoEst=b.dataset.e; cargarEst();
});
const num=(v,d)=>v===null||v===undefined?"—":v.toFixed(d);

// Las flechas de las tarjetas siempre miran la última hora aunque en esta
// pestaña estés viendo 24 h: "hacia dónde va" es una pregunta del presente.
async function refrescarTend(){
 try{
  const s=await api("/api/stats?range=1h");
  MAG.forEach(m=>tend[m.k]=s.series[m.k].tend_h);
 }catch(e){}
}

async function cargarEst(){
 $("#einfo").textContent="cargando...";
 try{
  const s=await api("/api/stats?range="+rangoEst);
  $("#einfo").textContent=s.muestras+" muestras"+(s.muestras?"":
   " — aún sin datos, se guarda una por minuto");
  pintarEst(s);
 }catch(e){$("#einfo").textContent="error al cargar";}
}

function pintarEst(s){
 const CO2V=s.co2_ventilado;
 const p24=(v,n,lim)=>v===null
  ?"aún sin datos suficientes — hacen falta "+s.umbral_media24+" minutos de historial"
  :num(v,1)+" µg/m³ &nbsp;<span class='pill "+["s-ok","s-warn","s-bad"][n]+"'>"
   +ETIQ[n]+"</span> <span class='ayuda'>límite OMS "+lim+"</span>";
 $("#m24").innerHTML=
  fila("PM2.5", p24(s.pm25_24h, s.n_pm25_24h, 15))
 +fila("PM10",  p24(s.pm10_24h, s.n_pm10_24h, 45));

 const a=s.ach;
 const cal=a===null?null:(a<0.5?["Mala","s-bad"]:(a<=1?["Aceptable","s-warn"]:["Buena","s-ok"]));
 const sv=s.sin_ventilar_min;
 $("#vent").innerHTML=
  fila("Renovaciones de aire", a===null
    ?"— no hay ninguna bajada de CO₂ lo bastante limpia para medirlo"
    :a.toFixed(2)+" ren/h &nbsp;<span class='pill "+cal[1]+"'>"+cal[0]+"</span>")
 +fila("Sin renovar desde hace", sv===null
    ?"el CO₂ no ha bajado de "+CO2V+" ppm en todo el historial"
    :(sv<60?sv+" min":Math.floor(sv/60)+" h "+(sv%60)+" min")
     +" <span class='ayuda'>(última vez por debajo de "+CO2V+" ppm)</span>")
 +fila("CO₂ exterior de referencia", s.co2_ext+" ppm"
    +" <span class='ayuda'>fondo atmosférico, constante — no lo mide ningún"
    +" sensor ni servicio</span>");

 let h="<tr><th>Magnitud</th><th>Mín</th><th>Media</th><th>P95</th><th>Máx</th>"
      +"<th>Tend/h</th></tr>";
 MAG.forEach(m=>{
  const e=s.series[m.k];
  const t=e.tend_h===null?"—":(e.tend_h>0?"+":"")+e.tend_h.toFixed(m.d?1:0);
  h+="<tr><td>"+m.t+" <span class='ayuda'>"+m.u+"</span></td>"
    +"<td>"+num(e.min,m.d)+"</td><td>"+num(e.media,m.d)+"</td>"
    +"<td>"+num(e.p95,m.d)+"</td><td>"+num(e.max,m.d)+"</td><td>"+t+"</td></tr>";
 });
 $("#tstats").innerHTML=h;

 // El P95 va antes que el máximo a propósito: el máximo de 1440 muestras es
 // casi siempre un artefacto de un segundo, el P95 sí describe el ambiente.
 let g="";
 MAG.forEach(m=>{
  const e=s.series[m.k];
  if(!e.pct) return;   // magnitud sin umbrales definidos
  g+="<div class='mag'>"+m.t+"</div>"
   +"<div class='stack'>"+[0,1,2].map(i=>e.pct[i]>0
     ?"<i class='b"+i+"' style='width:"+e.pct[i]+"%'></i>":"").join("")+"</div>"
   +"<div class='pctleg'>"+[0,1,2].map(i=>
     "<span>"+e.pct[i].toFixed(1)+"% "+["correcto","vigilar","malo"][i]+"</span>"
    ).join("")+"</div>";
 });
 $("#pcts").innerHTML=g||"<p class='ayuda'>Sin datos todavía.</p>";
}

// ---- Aire exterior ----
async function cargarExt(){
 try{ext=await api("/api/exterior");}catch(e){ext=null;}
 pintarExt(); pintarExtInfo();
}

function pintarExt(){
 const el=$("#ext"), no=$("#extnota");
 if(!ext||!ext.configurado){
  el.innerHTML=fila("Aire de fuera","sin localización elegida");
  no.innerHTML="Elígela en <b>Sistema → Localización del aire exterior</b>. "
   +"Sin ella el aparato no pregunta a ningún servicio.";
  return;
 }
 if(!ext.valido){
  el.innerHTML=fila("Localización",esc(ext.lugar))
   +fila("Estado",ext.intentos===0
     ?"buscando el primer dato..."
     :"no se pudo traer el dato (HTTP "+ext.http+")");
  // innerHTML y no textContent: el resto de ramas escriben HTML aquí y mezclar
  // las dos deja restos del estado anterior debajo del nuevo.
  no.innerHTML=ext.intentos===0?"":"Se reintenta solo cada pocos minutos.";
  return;
 }

 // Un sensor caído no aporta columna "dentro": comparar contra su última
 // lectura buena de hace horas daría un delta que no significa nada.
 const R=(t,u,a,b,dec)=>{
  const d=hay(a)&&hay(b)?((a-b>0?"+":"")+(a-b).toFixed(dec)):"—";
  return "<tr><td>"+t+" <span class='ayuda'>"+u+"</span></td>"
   +"<td>"+(hay(a)?a.toFixed(dec):"—")+"</td>"
   +"<td>"+(hay(b)?b.toFixed(dec):"—")+"</td><td>"+d+"</td></tr>";
 };
 el.innerHTML="<tr><th>Magnitud</th><th>Dentro</th><th>Fuera</th><th>Δ</th></tr>"
  +R("PM2.5","µg/m³",dentro("pms","pm25"),ext.pm25,1)
  +R("PM10","µg/m³", dentro("pms","pm10"),ext.pm10,1)
  +R("Temp","°C",    dentro("scd","temp"),ext.temp,1)
  +R("Humedad","%",  dentro("scd","hum"), ext.hum, 0)
  // El CO₂ de fuera no lo mide nadie: es la constante de fondo atmosférico.
  // Va etiquetado como tal en la nota, no disfrazado de medida.
  +R("CO₂","ppm",    dentro("scd","co2"), ext.co2_ext,0)
  +R("Ozono","µg/m³",null,ext.o3,0)
  +R("NO₂","µg/m³",  null,ext.no2,1);

 // El ratio es lo único de este panel que responde a una pregunta accionable:
 // si el aire de dentro está peor, la fuente la tienes tú, no la ciudad.
 const pi=dentro("pms","pm25"), pe=ext.pm25;
 let r="";
 if(hay(pi)&&hay(pe)&&pe>=1){
  const q=pi/pe;
  r="<b>Ratio interior/exterior de PM2.5: "+q.toFixed(2)+"</b> — "
   +(q<0.8?"el aire de dentro está mejor que el de fuera."
    :(q<1.25?"dentro y fuera van parejos: lo que hay dentro es lo que entra."
     :"la fuente está dentro (cocina, velas, tabaco, barrer).")) + "<br>";
 }
 const min=Math.round(ext.hace_s/60);
 const ft=ext.fuente==="aqicn"
   ?"Estación real"+(ext.estacion?" ("+esc(ext.estacion)
      +(ext.distancia_km!==null?", "+ext.distancia_km.toFixed(1)+" km":"")+")":"")+"."
   :"Modelo CAMS sobre rejilla de 11 km, no una estación de medida.";
 no.innerHTML=r
  +(ext.rancio?"<span class='err'>Dato caducado</span> — ":"")
  +"Actualizado hace "+(min<1?"menos de 1 min":min+" min")+" · "+esc(ext.lugar)
  +(ext.aqi===null?"":" · AQI europeo "+ext.aqi)
  +". "+ft+" "
  +"El CO₂ de fuera es el fondo atmosférico ("+ext.co2_ext+" ppm), constante: "
  +"ningún servicio lo mide.";
}

function pintarExtInfo(){
 const el=$("#extinfo");
 if(!ext){el.innerHTML=fila("Estado","no se pudo leer el aparato");return;}
 const fte=ext.fuente==="aqicn"
   ?"Estación real"+(ext.estacion?" — "+esc(ext.estacion):"")
    +(ext.distancia_km!==null?" (a "+ext.distancia_km.toFixed(1)+" km)":"")
   :(ext.fuente==="modelo"?"Modelo CAMS (Open-Meteo)":"—");
 el.innerHTML=
  fila("Localización",ext.configurado
    ?esc(ext.lugar)+" <span class='ayuda'>("+ext.lat.toFixed(3)+", "
     +ext.lon.toFixed(3)+")</span>"
    :"sin elegir")
 +fila("Fuente",fte)
 +fila("Último dato",ext.valido
    ?"hace "+Math.round(ext.hace_s/60)+" min"
     +(ext.rancio?" <span class='err'>— caducado</span>":"")
    :(ext.configurado?"todavía ninguno":"—"))
 +fila("Sondeos",ext.intentos+" · "+ext.fallos+" fallidos"
    +(ext.http?" · último HTTP "+ext.http:""))
 +fila("Cada cuánto se pide",ext.cada_min+" min <span class='ayuda'>"
    +(ext.fuente==="aqicn"?"la estación real se vuelve a consultar a este ritmo"
      :"el modelo solo se recalcula dos veces al día")+"</span>");

 const d=$("#extdisc");
 if(ext.fuente==="aqicn"){
  d.innerHTML="<b>Estación real más cercana:</b> "+esc(ext.estacion||"desconocida")
   +(ext.distancia_km!==null?", a "+ext.distancia_km.toFixed(1)+" km":"")
   +". Sigue sin ser tu balcón, pero es una medida, no un modelo.";
 }else if(ext.fuente==="modelo"){
  d.innerHTML="El dato de fuera <b>no sale de una estación de medida</b>: "
   +"es el modelo CAMS de Copernicus sobre una rejilla de 11 km, con valores "
   +"horarios que se recalculan dos veces al día. Sirve para saber si fuera "
   +"está peor que dentro; no para saber cuánto contamina tu calle.";
 }else{
  d.innerHTML="Todavía no hay ningún dato de fuera que mostrar: "
   +"ni de una estación real ni de un modelo.";
 }
}

async function fijarLugar(lat,lon,nombre){
 const m=$("#extmsg");
 m.className="ayuda"; m.textContent="Guardando...";
 try{
  const d=await(await fetch("/api/exterior?lat="+lat+"&lon="+lon
    +"&nombre="+encodeURIComponent(nombre),{method:"POST"})).json();
  m.className=d.ok?"okmsg":"err"; m.textContent=d.msg;
  if(d.ok){$("#extres").innerHTML=""; setTimeout(cargarExt,2000);}
 }catch(e){m.className="err";m.textContent="No se pudo contactar con el aparato";}
}

$("#btoken").onclick=async()=>{
 const token=$("#qtoken").value.trim();
 const m=$("#tokenmsg");
 m.className="ayuda"; m.textContent="Guardando...";
 try{
  const d=await(await fetch("/api/exterior/token?token="+encodeURIComponent(token),
   {method:"POST"})).json();
  m.className=d.ok?"okmsg":"err"; m.textContent=d.msg;
  if(d.ok) setTimeout(cargarExt,2000);
 }catch(e){m.className="err";m.textContent="No se pudo contactar con el aparato";}
};

$("#presets").innerHTML=LUGARES.map((l,i)=>
 '<button data-l="'+i+'">'+l.n+'</button>').join("");
$("#presets").onclick=e=>{
 const i=e.target.dataset.l;
 if(i===undefined) return;
 fijarLugar(LUGARES[i].lat,LUGARES[i].lon,LUGARES[i].n);
};

// El geocoding lo llama el navegador: tiene internet y CPU de sobra, y así el
// ESP32 no gasta un handshake TLS en rellenar un desplegable. Nominatim
// (OpenStreetMap) resuelve hasta nivel de barrio ("quarter"); el geocoder de
// Open-Meteo (GeoNames) que se usaba antes solo llega a distrito y no
// encuentra, por ejemplo, "Delicias" en Madrid. countrycodes=es limita a
// España: sin el, "Delicias" a secas puede resolver a un homonimo extranjero
// o a un pueblo lejano antes que al barrio buscado. La provincia sale en cada
// resultado (display_name) para desambiguar entre los homonimos que queden.
$("#bbuscar").onclick=async()=>{
 const q=$("#qlugar").value.trim();
 if(!q) return;
 const r=$("#extres");
 r.innerHTML="<p class='ayuda'>Buscando...</p>";
 try{
  const res=await fetch("https://nominatim.openstreetmap.org/search"
   +"?q="+encodeURIComponent(q)+"&format=jsonv2&limit=6&accept-language=es"
   +"&countrycodes=es");
  if(!res.ok){
   r.innerHTML="<p class='err'>Nominatim no responde ahora mismo (HTTP "+res.status+"). Prueba en unos segundos.</p>";
   return;
  }
  const d=await res.json();
  if(!d||!d.length){
   r.innerHTML="<p class='ayuda'>Sin resultados para «"+esc(q)+"».</p>"; return;
  }
  // display_name trae toda la jerarquía separada por comas (barrio, distrito,
  // ciudad, comunidad, código postal, país); las 3 primeras bastan para
  // identificar el sitio sin desbordar el botón.
  const nom=x=>x.display_name.split(",").slice(0,3).join(",").trim();
  r.innerHTML="<div class='bar'>"+d.map((x,i)=>
   '<button data-g="'+i+'">'+esc(nom(x))+'</button>').join("")+"</div>";
  r.onclick=e=>{
   const i=e.target.dataset.g;
   if(i===undefined) return;
   fijarLugar(parseFloat(d[i].lat),parseFloat(d[i].lon),nom(d[i]));
  };
 }catch(e){
  r.innerHTML="<p class='err'>La búsqueda necesita internet en este navegador. "
   +"Usa los atajos de abajo si no lo tienes.</p>";
 }
};

// ---- Sistema ----
const pill=(ok,txtOk,txtNo)=>'<span class="pill '+(ok?"s-ok":"s-bad")+'">'
 +(ok?txtOk:txtNo)+'</span>';
function fila(k,v){return "<tr><td class='k'>"+k+"</td><td>"+v+"</td></tr>";}

async function cargarSis(){
 try{
  const h=await api("/api/health");
  horaOk=h.hora_ok;
  const pms=h.pms, scd=h.scd41;
  $("#salud").innerHTML=
   fila("PMS5003 · partículas", pill(pms.ok,"OK","FALLO")
     +" &nbsp;"+pms.frames+" lecturas · último hace "+pms.hace_s+" s"
     +(pms.durmiendo?" · <i>dormido (duty-cycle)</i>":""))
  +fila("SCD41 · CO₂/temp/hum", pill(scd.ok,"OK","FALLO")
     +" &nbsp;"+(scd.ok?scd.reads+" lecturas"
       :(scd.rancio?"dejó de entregar datos hace "+scd.hace_s+" s · "
                    +scd.reads+" lecturas antes de pararse"
       :(scd.i2c?"responde en I2C · err "+scd.ultimo_error+" en "+esc(scd.etapa)
                :"no responde en I2C 0x62"))))
  +fila("Pantalla OLED", pill(h.oled.ok,"OK","FALLO")
     +(h.oled.ok?" &nbsp;0x"+h.oled.addr.toString(16).toUpperCase():""))
  // El SSID lo elige quien monta el punto de acceso, no el aparato: es la
  // unica cadena de este panel que viene de fuera y va a innerHTML.
  +fila("WiFi", pill(h.wifi.ok,"OK","CAÍDO")
     +" &nbsp;"+esc(h.wifi.ssid)+" · "+h.wifi.rssi+" dBm · "+h.wifi.recon+" reconexiones")
  +fila("Hora NTP", pill(h.hora_ok,"SINCRONIZADA","SIN SINCRONIZAR"));

  const pc=(100*h.heap_libre/h.heap_total).toFixed(0);
  $("#recursos").innerHTML=
   fila("Encendido desde hace", dur(h.uptime_s))
  +fila("Último reinicio", esc(h.reset)
     +(h.reset==="BROWNOUT"?" <span class='err'>— falta de corriente</span>":""))
  +fila("Memoria libre", (h.heap_libre/1024).toFixed(0)+" KB ("+pc+"%)")
  +fila("Mínimo histórico", (h.heap_min/1024).toFixed(0)+" KB"
     +(h.heap_min<20000?" <span class='err'>— bajo</span>":""))
  +fila("Historial", h.historial.muestras+" / "+h.historial.capacidad+" muestras")
  +fila("Eventos", h.log.eventos+" / "+h.log.capacidad);

  const pctFlash=Math.round(100*h.flash.usado/(h.flash.usado+h.flash.libre));
  $("#flashUso").textContent=pctFlash+"% ("+(h.flash.usado/1024).toFixed(0)+" KB / "
    +((h.flash.usado+h.flash.libre)/1024).toFixed(0)+" KB)";
  $("#flashEscrituras").textContent=h.flash.escrituras;

  ascOn=scd.asc;
  $("#basc").textContent=ascOn?"Desactivar autocalibración":"Activar autocalibración";
  $("#scddiag").innerHTML=
   fila("Entrega de datos", scd.rancio
      ?pill(false,"","MUDO")+" &nbsp;último dato hace "+scd.hace_s+" s"
      :pill(true,"AL DÍA","")+" &nbsp;último dato hace "+scd.hace_s+" s")
  // Que coincidan CO₂, temperatura y humedad al decimal durante 10 min no es
  // un ambiente estable: el ruido del propio sensor lo hace imposible.
  +fila("Valor congelado", scd.congelado
      ?pill(false,"","SÍ")+" &nbsp;las tres magnitudes idénticas"
      :pill(true,"NO",""))
  +fila("Autocalibración (ASC)", scd.asc
      ?"Activada — asume que el mínimo de 7 días es aire fresco de 400 ppm"
      :"Desactivada — la referencia solo cambia si calibras a mano")
  +fila("Autotest del chip", scd.selftest===65535?"Sin ejecutar"
      :(scd.selftest===0?pill(true,"SENSOR SANO","")
        :pill(false,"","AVERÍA INTERNA 0x"+scd.selftest.toString(16).toUpperCase())))
  +fila("Reinicios del sensor", scd.reinicios
      +(scd.reinicios>0?" <span class='err'>— se está cayendo</span>":""))
  +fila("Altitud configurada", scd.altitud+" m")
  +fila("Nº de serie", scd.serie);
 }catch(e){fallo($("#salud"),"No se pudo leer el estado");}

 try{
  const l=await api("/api/log");
  const NIV=["INFO","AVISO","ERROR"];
  const vis=l.eventos.filter(e=>e.n>=filtro);
  $("#log").innerHTML=vis.length?vis.map(e=>
   '<li class="l'+e.n+'"><time>'+hora(e.ts)+'</time>'
   +'<span class="lv">'+NIV[e.n]+'</span>'
   +'<span class="m">'+e.m.replace(/[<>&]/g,c=>({"<":"&lt;",">":"&gt;","&":"&amp;"}[c]))
   +(e.r>1?'<span class="rep">×'+e.r+'</span>':'')+'</span></li>').join("")
   :'<li><span class="m">Sin eventos de este nivel</span></li>';
 }catch(e){fallo($("#log"),"No se pudo leer el log");}

 cargarExt();
 cargarCloudEstado();
}

// El estado de la nube es una llamada aparte de /api/health: si el aparato
// no tiene nada configurado no queremos que un campo ausente tumbe el resto
// del panel de Sistema.
async function cargarCloudEstado(){
 try{
  const e=await api("/api/cloud/estado");
  $("#cloudEstado").textContent=e.activo?"activo"
   :(e.configurado?"configurado, inactivo":"sin configurar");
  $("#cloudTokenEstado").textContent=e.hay_token
   ?"guardado (deja el campo en blanco para conservarlo)":"no hay ninguno guardado";
  $("#cloudCola").textContent=e.en_cola;
  $("#cloudUrl").value=e.url||"";
  $("#cloudUser").value=e.usuario||"";
  $("#cloudOn").checked=!!e.activo;
 }catch(e){$("#cloudEstado").textContent="Sin conexión con el aparato";}
}

$("#bcloud").onclick=async()=>{
 const m=$("#cloudMsg");
 m.className="ayuda"; m.textContent="Guardando...";
 try{
  const body=new URLSearchParams({
   url:$("#cloudUrl").value, user:$("#cloudUser").value,
   token:$("#cloudToken").value, on:$("#cloudOn").checked?"1":"0"
  });
  const d=await(await fetch("/api/cloud/config",{method:"POST",body})).json();
  m.className=d.ok?"okmsg":"err"; m.textContent=d.msg;
  if(d.ok){$("#cloudToken").value=""; cargarCloudEstado();}
 }catch(e){m.className="err";m.textContent="No se pudo contactar con el aparato";}
};
// ---- diagnóstico del SCD41 ----
// Los tres botones se deshabilitan a la vez: el aparato es de un solo hilo y
// solapar dos operaciones sobre el sensor lo dejaría en un estado indefinido.
async function accionScd(url,trabajando,confirmar){
 if(confirmar&&!confirm(confirmar)) return;
 const m=$("#scdmsg"), bs=[$("#breset"),$("#btest"),$("#basc")];
 bs.forEach(b=>b.disabled=true);
 m.className="ayuda"; m.textContent=trabajando;
 try{
  const d=await(await fetch(url,{method:"POST"})).json();
  m.className=d.ok?"okmsg":"err";
  m.textContent=d.msg;
  setTimeout(cargarSis,1500);
 }catch(e){m.className="err";m.textContent="No se pudo contactar con el aparato";}
 bs.forEach(b=>b.disabled=false);
}
$("#breset").onclick=()=>accionScd("/api/scd/reset",
 "Apagando y volviendo a encender el sensor...");
// El autotest bloquea el aparato entero: se avisa antes para que un timeout
// del navegador no se confunda con un cuelgue.
$("#btest").onclick=()=>accionScd("/api/scd/autotest",
 "Autotest en curso. El aparato no responde durante ~10 s...",
 "El chip se autoexamina y tarda 10 segundos. Durante ese rato la web y la "
 +"pantalla no responden, y se pierde una medida.\n\n¿Lanzarlo?");
$("#basc").onclick=()=>accionScd("/api/scd/asc?on="+(ascOn?0:1),
 (ascOn?"Desactivando":"Activando")+" autocalibración...",
 "La autocalibración (ASC) asume que el valor más bajo que ve en 7 días es "
 +"aire fresco de 400 ppm y desplaza toda la escala para que cuadre. Si el "
 +"aparato vive siempre dentro de casa y nunca ve aire exterior, eso desvía "
 +"la referencia en lugar de corregirla.\n\n"
 +(ascOn?"¿Desactivarla?":"¿Activarla?")
 +"\n\nSe guarda en la EEPROM del sensor.");

// Escribe en la EEPROM del sensor: se confirma antes. Es repetible, así que
// un error no es irreversible, pero tampoco conviene lanzarlo sin querer.
$("#bcal").onclick=async()=>{
 const ppm=+$("#ppm").value;
 if(!confirm("¿El aparato lleva 3 minutos al aire libre y sin nadie cerca?\n\n"
  +"Se le dirá al sensor que este aire tiene "+ppm+" ppm.")) return;
 const b=$("#bcal"), m=$("#calmsg");
 b.disabled=true; m.className="ayuda"; m.textContent="Calibrando...";
 try{
  const res=await fetch("/api/calibrar?ppm="+ppm,{method:"POST"});
  const d=await res.json();
  m.className=d.ok?"okmsg":"err";
  m.textContent=d.msg;
  if(d.ok) setTimeout(cargarSis,1000);
 }catch(e){m.className="err";m.textContent="No se pudo contactar con el aparato";}
 b.disabled=false;
};

document.querySelectorAll("[data-f]").forEach(b=>b.onclick=()=>{
 document.querySelectorAll("[data-f]").forEach(x=>x.classList.toggle("on",x===b));
 filtro=+b.dataset.f; cargarSis();
});

// ---- ciclo ----
// 5 s en "Ahora" porque el SCD41 no entrega dato nuevo más rápido.
cargarAhora(); setInterval(cargarAhora,5000);
// Las tendencias se recalculan sobre 30 muestras de un minuto: pedirlas más a
// menudo que cada minuto sería gastar peticiones para leer el mismo número.
refrescarTend(); setInterval(refrescarTend,60000);
// El aparato solo sondea el exterior cada media hora: pedirlo cada minuto es
// leer su caché, que cuesta menos que una petición al historial.
cargarExt(); setInterval(cargarExt,60000);
setInterval(()=>{if(!$("#sis").hidden)cargarSis();},15000);
setInterval(()=>{if(!$("#hist").hidden)cargarHist();},60000);
setInterval(()=>{if(!$("#est").hidden)cargarEst();},60000);
$("#pie").textContent="Air Script · ESP32 · PMS5003 + SCD41";
</script>
</body>
</html>
)HTMLPAGE";
