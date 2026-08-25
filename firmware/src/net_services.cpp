#include "net_services.h"
#include "config.h"
#include "hcs_sensors.h"
#include "mqtt_bridge.h"
#include "hcs_boiler_text.h"
#if defined(ESP32) && defined(HCS_GW_ENABLE)
#include "ot_gateway.h"
#endif

#include <WiFiManager.h>
#include <ArduinoOTA.h>
#include <LittleFS.h>

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266httpUpdate.h>
static ESP8266WebServer server(HTTP_PORT);
#elif defined(ESP32)
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPUpdate.h>
static WebServer server(HTTP_PORT);
#endif

#include <ArduinoJson.h>

NetServices::NetServices(OtMaster& ot) : ot_(ot) {}

bool NetServices::wifiConnected() const {
  return WiFi.status() == WL_CONNECTED;
}

String NetServices::localIp() const {
  if (WiFi.status() != WL_CONNECTED) return "";
  return WiFi.localIP().toString();
}

bool NetServices::beginWifi(HcsSettings& settings) {
  settings_ = settings;
  WiFi.mode(WIFI_STA);

  // Unique identity: two devices with the same name/hostname confuse DHCP
  // displays and mDNS (audit follow-up). Suffix = last 4 MAC hex chars.
  String mac = WiFi.macAddress();
  mac.replace(":", "");
  String suffix = mac.substring(mac.length() - 4);
  suffix.toUpperCase();
#if defined(ESP32)
  WiFi.setHostname(("hcs-" + suffix).c_str());
#else
  WiFi.hostname("hcs-" + suffix);
#endif

  WiFiManager wm;
  wm.setConfigPortalTimeout(CONFIG_PORTAL_TIMEOUT_S);
  wm.setConnectTimeout(WIFI_CONNECT_TIMEOUT_S);
  wm.setTitle("Home Climate System");

  WiFiManagerParameter p_mqtt_host("mqtt_host", "MQTT broker host",
                                   settings.mqtt_host.c_str(), 64);
  char portbuf[8];
  snprintf(portbuf, sizeof(portbuf), "%u", settings.mqtt_port);
  WiFiManagerParameter p_mqtt_port("mqtt_port", "MQTT port", portbuf, 6);
  WiFiManagerParameter p_mqtt_user("mqtt_user", "MQTT user",
                                   settings.mqtt_user.c_str(), 32);
  WiFiManagerParameter p_mqtt_pass("mqtt_pass", "MQTT password",
                                   settings.mqtt_pass.c_str(), 32);
  WiFiManagerParameter p_prefix("mqtt_prefix", "MQTT prefix",
                                settings.mqtt_prefix.c_str(), 16);
  WiFiManagerParameter p_name("dev_name", "Device name",
                              settings.device_name.c_str(), 32);
  WiFiManagerParameter p_ota("ota_pass", "OTA password (optional)",
                             settings.ota_password.c_str(), 32);

  wm.addParameter(&p_mqtt_host);
  wm.addParameter(&p_mqtt_port);
  wm.addParameter(&p_mqtt_user);
  wm.addParameter(&p_mqtt_pass);
  wm.addParameter(&p_prefix);
  wm.addParameter(&p_name);
  wm.addParameter(&p_ota);

  // autoConnect(apName, apPassword) — AP name is NOT the home SSID.
  // Saved STA creds live in WiFiManager's own NVS; optional seed below.
  if (settings.configured && settings.wifi_ssid.length()) {
    WiFi.begin(settings.wifi_ssid.c_str(), settings.wifi_pass.c_str());
#ifdef HCS_BOARD_LOLIN_C3_MINI
    // C3 stacked on the DIYLess shield shares a small LDO with the PIC;
    // cap TX power so radio bursts cannot brown the rail out.
    WiFi.setTxPower(WIFI_POWER_17dBm);
#endif
  }
  // Per-device AP name so simultaneous portals never collide
  ap_name_ = String(PORTAL_AP_NAME) + "-" + suffix;
  Serial.printf("[wifi] portal AP fallback: %s\n", ap_name_.c_str());
  bool ok = wm.autoConnect(ap_name_.c_str(), PORTAL_AP_PASS);

  if (!ok) {
    Serial.println(F("[wifi] failed / portal timeout — retrying portal"));
    ok = wm.startConfigPortal(ap_name_.c_str(), PORTAL_AP_PASS);
  }

  if (!ok) {
    Serial.println(F("[wifi] giving up"));
    return false;
  }

  // Persist whatever we have after successful association
  settings.wifi_ssid = WiFi.SSID();
  settings.wifi_pass = WiFi.psk();
  if (strlen(p_mqtt_host.getValue())) {
    // trim every portal field — phone-typed forms love stray spaces/CRs
    settings.mqtt_host = hcs_trim(p_mqtt_host.getValue());
    settings.mqtt_port = (uint16_t)atoi(p_mqtt_port.getValue());
    if (!settings.mqtt_port) settings.mqtt_port = 1883;
    settings.mqtt_user = hcs_trim(p_mqtt_user.getValue());
    settings.mqtt_pass = p_mqtt_pass.getValue();
    String pref = hcs_trim(p_prefix.getValue());
    if (pref.length()) settings.mqtt_prefix = pref;
    String nm = hcs_trim(p_name.getValue());
    if (nm.length()) settings.device_name = nm;
    settings.ota_password = p_ota.getValue();
  }
  // Compile-time fallback if portal left MQTT empty — but never accept a
  // secrets-template placeholder as "configured".
  if ((!settings.mqtt_host.length() ||
       hcs_host_is_placeholder(settings.mqtt_host)) &&
      strlen(MQTT_HOST) && !hcs_host_is_placeholder(MQTT_HOST)) {
    settings.mqtt_host = MQTT_HOST;
    settings.mqtt_port = MQTT_PORT;
    settings.mqtt_user = MQTT_USER;
    settings.mqtt_pass = MQTT_PASS;
  }
  settings.configured = settings.wifi_ssid.length() > 0;
  SettingsStore store;
  store.begin();
  store.save(settings);
  settings_ = settings;

  // Modem sleep makes some chips (esp. ESP32-C3) drop inbound TCP SYNs —
  // web UI dies while MQTT survives. Not worth 20 µA on a mains device.
#if defined(ESP8266)
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
#else
  WiFi.setSleep(false);
#endif

  Serial.printf("[wifi] ok %s  MQTT %s:%u\n", WiFi.localIP().toString().c_str(),
                settings.mqtt_host.c_str(), settings.mqtt_port);
  return true;
}

// ---------------------------------------------------------------------------
// Device web UI — original single-page app served from PROGMEM.
// Written from scratch for Home Climate System (MIT). Functional inspiration
// only from other OT web UIs; no third-party code or assets are embedded.
// ---------------------------------------------------------------------------
static const char INDEX_HTML[] PROGMEM = R"HTML(<!DOCTYPE html>
<html><head><meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1">
<title>Home Climate</title>
<style>
body{font-family:system-ui,sans-serif;margin:0 auto;max-width:560px;padding:0 12px 32px;
background:#111;color:#e8e8e8}
header{display:flex;align-items:center;gap:10px;padding:16px 0 8px}
h1{font-size:1.2rem;font-weight:600;margin:0;flex:1}
.badge{padding:2px 10px;border-radius:999px;font-size:.72rem;font-weight:700;text-transform:uppercase}
.b-on{background:#1b5e20;color:#c8e6c9}.b-off{background:#333;color:#aaa}.b-warn{background:#7a3b00;color:#ffe0b2}
nav{display:flex;gap:4px;border-bottom:1px solid #333;padding-bottom:8px;margin-bottom:14px}
nav button{flex:1;background:none;border:none;color:#999;padding:8px 2px;cursor:pointer;font-size:.9rem;border-radius:8px 8px 0 0}
nav button.act{color:#03a9f4;background:#1c1c1c;font-weight:600}
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(150px,1fr));gap:10px}
.card{background:#1c1c1c;border:1px solid #2c2c2c;border-radius:12px;padding:12px}
.card h3{margin:0 0 6px;font-size:.7rem;font-weight:600;color:#999;text-transform:uppercase;letter-spacing:.05em}
.v{font-size:1.35rem}.u{font-size:.85rem;color:#999}
section{display:none}section.act{display:block}
button.a{background:#03a9f4;color:#fff;border:none;padding:9px 14px;border-radius:8px;font-weight:600;cursor:pointer;margin:4px 4px 4px 0}
button.g{background:none;color:#03a9f4;border:1px solid #03a9f4;padding:9px 14px;border-radius:8px;cursor:pointer;margin:4px}
input,select{font:inherit;background:#111;color:#eee;border:1px solid #444;border-radius:8px;padding:8px;width:100%;box-sizing:border-box;margin:3px 0 10px}
.row{display:flex;gap:8px;align-items:center}.row input{margin:0}
label{font-size:.78rem;color:#999;display:block;margin-top:8px}
#msg{margin-top:10px;padding:10px;border-radius:8px;display:none;background:#1b5e2033;border:1px solid #66bb6a;color:#c8e6c9}
table{width:100%;border-collapse:collapse;font-size:.88rem}td{padding:6px 2px;border-bottom:1px solid #262626}td:first-child{color:#999}
input[type=range]{padding:0;height:34px}
footer{text-align:center;color:#666;font-size:.75rem;margin-top:24px}
a{color:#03a9f4;text-decoration:none}
</style></head><body>
<header><h1 id=devname>Home Climate</h1><span class="badge b-off" id=fsb style="display:none">FAILSAFE</span><span class="badge b-off" id=otb>OT ?</span></header>
<nav>
<button data-t=status class=act>Status</button>
<button data-t=controls>Controls</button>
<button data-t=gateway>Gateway</button>
<button data-t=sensors>Sensors</button>
<button data-t=settings>Settings</button>
<button data-t=system>System</button>
</nav>
<section id=t-status class=act>
<div class=grid>
<div class=card><h3>Flame</h3><div class=v id=flame>&mdash;</div></div>
<div class=card><h3>CH active</h3><div class=v id=cha>&mdash;</div></div>
<div class=card><h3>Fault</h3><div class=v id=fault>&mdash;</div></div>
<div class=card><h3>Flow temp</h3><div class=v><span id=ftemp>&mdash;</span><span class=u> &deg;C</span></div></div>
<div class=card><h3>Return temp</h3><div class=v><span id=rtemp>&mdash;</span><span class=u> &deg;C</span></div></div>
<div class=card><h3>Outdoor</h3><div class=v><span id=otemp>&mdash;</span><span class=u> &deg;C</span></div></div>
<div class=card><h3>Modulation</h3><div class=v><span id=mod>&mdash;</span><span class=u> %</span></div></div>
<div class=card><h3>Setpoint</h3><div class=v><span id=fsp>&mdash;</span><span class=u> &deg;C</span></div></div>
<div class=card><h3>CH pressure</h3><div class=v><span id=prbar>&mdash;</span><span class=u> bar</span></div></div>
<div class=card style="grid-column:1/-1"><h3>Boiler diagnostics</h3><div class=v id=bdiag style="font-size:1rem">&mdash;</div>
<div style="color:#999;font-size:.8rem;margin-top:6px" id=bident></div></div>
</div></section>
<section id=t-controls>
<div class=card>
<label>Central heating</label>
<div class=row><button class=a onclick="ctl({ch_enable:true})">CH on</button>
<button class=g onclick="ctl({ch_enable:false})">CH off</button>
<span style="margin-left:auto">now: <b id=cch>&mdash;</b></span></div>
<label>DHW enable</label>
<div class=row><button class=a onclick="ctl({dhw_enable:true})">DHW on</button>
<button class=g onclick="ctl({dhw_enable:false})">DHW off</button>
<span style="margin-left:auto">now: <b id=cdhw>&mdash;</b></span></div>
<label>DHW setpoint (&deg;C) <span style="color:#999" id=dhwbounds></span></label>
<div class=row><input type=number id=idsp min=30 max=60 step=1 value=50>
<button class=a onclick="ctl({dhw_setpoint:+idsp.value})">Apply</button>
<button class=g onclick="ctl({dhw_setpoint:'auto'})">Auto (thermostat)</button></div>
<label>Flow setpoint (&deg;C)</label>
<div class=row><input type=number id=isfp min=20 max=90 step=0.5 value=45>
<button class=a onclick="ctl({flow_setpoint:+isfp.value})">Apply</button></div>
<input type=range id=rsfp min=20 max=90 step=0.5 value=45 oninput="isfp.value=this.value">
<label>Max modulation (%) <b id=mmlbl></b></label>
<input type=range id=mm min=0 max=100 step=5 oninput="mmlbl.textContent=' '+this.value+'%'"
 onchange="ctl({max_modulation:+this.value})">
</div>
<div class=card style=margin-top:10px>
<label>Weather compensation <b id=wclbl></b></label>
<div class=row><button class=a onclick="ctl({weather_comp:true})">WC on</button>
<button class=g onclick="ctl({weather_comp:false})">WC off</button>
<span style="margin-left:auto">target: <b id=wct>&mdash;</b> &deg;C</span></div>
<div class=row><span style=flex:1><label>Ref &deg;C</label><input id=wc_ref type=number step=1></span>
<span style=flex:1><label>Design &deg;C</label><input id=wc_design type=number step=1></span></div>
<div class=row><span style=flex:1><label>Flow max</label><input id=wc_fmax type=number step=1 min=20 max=90></span>
<span style=flex:1><label>Flow min</label><input id=wc_fmin type=number step=1 min=10 max=80></span></div>
<button class=a onclick="applyWc()">Apply curve</button>
</div>
<div class=card style=margin-top:10px>
<label>Connection-loss failsafe <b id=fslbl></b></label>
<div style="color:#999;font-size:.8rem;margin-bottom:6px">If WiFi/MQTT stays
down longer than the grace period, CH is forced on at the setpoint below so
the house keeps warm unattended. Weather compensation is bypassed while
active.</div>
<div class=row><button class=a onclick="fsCfg(true)">Enable</button>
<button class=g onclick="fsCfg(false)">Disable</button></div>
<div class=row><span style=flex:1><label>Flow &deg;C</label><input id=fs_flow type=number step=1 min=20 max=90></span>
<span style=flex:1><label>Grace min</label><input id=fs_grace type=number step=1 min=1 max=120></span></div>
<button class=a onclick="applyFs()">Save failsafe values</button>
</div></section>
<section id=t-gateway>
<div id=gw_na class=card style="display:none;color:#999">Gateway not active on this device
(requires an ESP32 <code>*_gw</code> firmware build and gateway mode enabled).</div>
<div id=gw_ui style=display:none>
<div class=card><table>
<tr><td>Mode</td><td><b id=g_mode></b></td></tr>
<tr><td>Thermostat bus</td><td id=g_tstat>&mdash;</td></tr>
<tr><td>Override setpoint</td><td id=g_ov>&mdash;</td></tr>
<tr><td>Frames forwarded</td><td id=g_fwd></td></tr>
<tr><td>Answered locally</td><td id=g_loc></td></tr>
<tr><td>Modified</td><td id=g_mod></td></tr>
<tr><td>Errors</td><td id=g_err></td></tr>
</table></div>
<div class=card style=margin-top:10px>
<label>Mode switch (saves &amp; reboots)</label>
<button class=a onclick="setGwMode('gateway')">Enter gateway</button>
<button onclick="setGwMode('auto')">Auto-detect at boot</button>
<button class=g onclick="setGwMode('master_only')">Back to master-only</button>
<label>Force CH flow setpoint sent to boiler (&deg;C)</label>
<div class=row><input type=number id=g_ov_in min=20 max=90 step=0.5 placeholder=(thermostat value passes through)>
<button class=a onclick="applyGwOv()">Override</button>
<button class=g onclick="releaseGwOv()">Release</button></div>
</div>
</div>
</section>
<section id=t-sensors>
<div class=card>
<label>1-Wire DS18B20 probes (GPIO <b id=sn_pin>&mdash;</b>)</label>
<div class=row><button class=a onclick="senCfg(true)">Enable</button>
<button class=g onclick="senCfg(false)">Disable</button>
<button class=a onclick="senTest()">Test sensors</button>
<span style="margin-left:auto">now: <b id=sn_en>&mdash;</b></span></div>
<table><thead><tr><th>Probe</th><th>Temp</th><th>Health</th><th>Use</th></tr></thead>
<tbody id=sen_rows></tbody></table>
<div style="color:#999;font-size:.8rem;margin-top:6px">
Probes auto-detected on the bus. Assign a use: <b>outdoor</b> (Tout) feeds
weather compensation, <b>return</b> (Tret) backfills return-water, <b>custom</b>
publishes a named sensor to Home Assistant. Health is re-checked every poll
(presence, stuck +85 °C trap, implausible jumps).</div>
</div>
<div class=card style=margin-top:10px>
<label>Effective channels</label>
<table>
<tr><td>Outdoor</td><td id=sn_out>&mdash;</td></tr>
<tr><td>Return</td><td id=sn_ret>&mdash;</td></tr>
</table>
</div></section>
<section id=t-settings>
<div class=card>
<label>Device name</label><input id=s_name maxlength=31>
<label>MQTT broker host</label><input id=s_host maxlength=63 placeholder=e.g. homeassistant.local>
<label>MQTT port</label><input id=s_port type=number value=1883 min=1 max=65535>
<label>MQTT user</label><input id=s_user maxlength=31 autocomplete=off>
<label>MQTT password</label><input id=s_pass maxlength=31 type=password autocomplete=new-password placeholder=(unchanged)>
<label>Topic prefix</label><input id=s_prefix maxlength=15 value=hcs>
<label>OTA password (blank = none)</label><input id=s_otapass maxlength=31 type=password autocomplete=new-password placeholder=(unchanged)>
<button class=a onclick="saveSettings()">Save &amp; reboot</button>
<div id=msg></div>
</div></section>
<section id=t-system>
<div class=card><table>
<tr><td>Board</td><td id=i_board></td></tr>
<tr><td>Firmware</td><td id=i_ver></td></tr>
<tr><td>Node ID</td><td id=i_node></td></tr>
<tr><td>IP address</td><td id=i_ip></td></tr>
<tr><td>WiFi RSSI</td><td id=i_rssi></td></tr>
<tr><td>Uptime</td><td id=i_up></td></tr>
<tr><td>MQTT</td><td id=i_mqtt></td></tr>
</table></div>
<div class=card style=margin-top:10px>
<label>OpenTherm console (last 64 exchanges)</label>
<button class=a onclick="otLog()">Refresh OT log</button>
<button onclick="fetch('/api/otlog?clear').then(()=>otLog())">Clear</button>
<pre id=otlog style="max-height:220px;overflow:auto;background:#111;color:#9f9;padding:8px;font-size:11px;white-space:pre-wrap"></pre>
</div>
<div class=card style=margin-top:10px>
<label>Firmware update over the air</label>

<label>Flash from URL (.bin)</label>
<div class=row><input id=ourl placeholder=https://&#46;&#46;&#46;/firmware.bin>
<button class=a onclick="otaFromUrl()">Flash</button></div>
<label>Maintenance</label>
<button class=g onclick="doReboot()">Reboot device</button>
</div></section>
<footer>Home Climate System &middot; MIT &middot; <a href=/api/status target=_blank>API</a></footer>
<script>
const $=id=>document.getElementById(id);
async function jget(u){const r=await fetch(u);if(!r.ok)throw r.status;return r.json()}
async function jpost(u,b){const r=await fetch(u,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(b||{})});try{return await r.json()}catch(e){return{ok:r.ok}}}
function yn(v){return v?'ON':'OFF'}
let last=null;
function paint(s){
 last=s;
 $('devname').textContent=s.name||'Home Climate';
 const ob=$('otb');ob.textContent=s.ot_valid?'OT linked':'OT no link';
 ob.className='badge '+(s.ot_valid?'b-on':'b-off');
 $('flame').textContent=s.flame?'ON':'OFF';$('flame').style.color=s.flame?'#ff7043':'inherit';
 $('cha').textContent=s.ch_active?'YES':'NO';
 $('fault').textContent=s.fault?'FAULT':'OK';$('fault').style.color=s.fault?'#ef5350':'#81c784';
 $('ftemp').textContent=s.flow_temp??'—';$('rtemp').textContent=s.return_temp??'—';
  $('otemp').textContent=s.outdoor_temp??'—';$('mod').textContent=s.modulation??'—';
  const bd=s.boiler_diag;
  if(bd){$('bdiag').textContent=bd.text;$('bdiag').style.color=
    bd.state==='fault'?'#ef5350':bd.state==='ok'?'#81c784':'inherit';}
  $('prbar').textContent=s.pressure_bar??'—';
  let ident=[];
  if(s.boiler){if(s.boiler.identity)ident.push(s.boiler.identity);
   if(s.boiler.fault_history)ident.push('Fault history: '+s.boiler.fault_history);}
  $('bident').textContent=ident.join(' · ');
  if(s.dhw_setpoint!=null&&!['idsp'].includes(document.activeElement?.id)){
   idsp.value=s.dhw_setpoint;
   dhwbounds.textContent=s.boiler?.dhw_bounds?('boiler allows '+s.boiler.dhw_bounds):'';}
 $('fsp').textContent=s.flow_setpoint;$('cch').textContent=yn(s.ch_enable);
 $('cdhw').textContent=yn(s.dhw_enable);
 $('i_board').textContent=s.board+' · '+s.ip;
 $('i_ver').textContent=s.version;$('i_node').textContent=s.node_id;
 $('i_ip').textContent=s.ip;$('i_rssi').textContent=(s.rssi??'—')+' dBm';
 $('i_up').textContent=Math.floor((s.uptime||0)/3600)+'h '+Math.floor(((s.uptime||0)%3600)/60)+'m';
 $('i_mqtt').textContent=(s.mqtt_host||'not set')+':'+(s.mqtt_port||1883);
 if(document.activeElement&&document.activeElement.id!=='isfp'&&document.activeElement.id!=='rsfp'){
   isfp.value=s.flow_setpoint;if(+rsfp.value!==+s.flow_setpoint)rsfp.value=s.flow_setpoint;}
 mm.value=s.max_modulation;mmlbl.textContent=' '+s.max_modulation+'%';
  wclbl.textContent=s.wc_enable?' ON':' OFF';wct.textContent=s.wc_target??'—';
  const fs=s.failsafe;
  if(fs){
   const f=$('fsb');f.style.display=fs.active?'inline-block':'none';
   f.className='badge '+(fs.active?'b-on':'b-off');
   fslbl.textContent=fs.enable?' ARMED':' OFF';
   if(!['fs_flow','fs_grace'].includes(document.activeElement?.id)){
    fs_flow.value=fs.flow;fs_grace.value=fs.grace_min;}
  }
 if(!['wc_ref','wc_design','wc_fmax','wc_fmin'].includes(document.activeElement?.id)){
  wc_ref.value=s.wc_ref;wc_design.value=s.wc_design;wc_fmax.value=s.wc_fmax;wc_fmin.value=s.wc_fmin;}
 const g=s.gw;
 $('gw_ui').style.display=g?'block':'none';
 $('gw_na').style.display=g?'none':'block';
 if(g){
  g_mode.textContent=g.mode+(g.cfg&&g.cfg!=='gateway'?' ('+g.cfg+')':'');g_tstat.textContent=g.tstat_online?'ONLINE':'silent';
  g_ov.textContent=g.override_setpoint!=null?g.override_setpoint+' °C':'pass-through';
  g_fwd.textContent=g.forwarded;g_loc.textContent=g.answered_local;
  g_mod.textContent=g.modified;g_err.textContent=g.errors;}
}
function applyWc(){ctl({wc_ref:+wc_ref.value,wc_design:+wc_design.value,
 wc_fmax:+wc_fmax.value,wc_fmin:+wc_fmin.value})}
function applyFs(){jpost('/api/failsafe',{enable:fslbl.textContent.includes('ARMED'),
 flow:+fs_flow.value,grace_min:+fs_grace.value}).then(refresh)}
function fsCfg(on){jpost('/api/failsafe',{enable:on,flow:+fs_flow.value,grace_min:+fs_grace.value}).then(refresh)}
async function refresh(){try{paint(await jget('/api/status'))}catch(e){$('otb').textContent='API err'}}
function ctl(b){jpost('/api/control',b).then(refresh)}
async function loadSensors(){
 try{const s=await jget('/api/sensors');
  sn_pin.textContent=s.pin>=0?s.pin:'n/a';
  sn_en.textContent=yn(s.enabled);
  const rows=$('sen_rows');rows.innerHTML='';
  (s.devices||[]).forEach(d=>{
   const tr=document.createElement('tr');
   const role=d.role||'none';
   const short=d.addr?d.addr.slice(-8):'—';
   tr.innerHTML=`<td title="${d.addr||''}" style="font-family:monospace">${short}</td>`+
    `<td>${d.temp_c!=null?d.temp_c+' °C':'—'}</td>`+
    `<td style="color:${d.health==='ok'?'#6c6':(d.health==='unknown'?'#999':'#c66')}">${d.health||'—'}${d.name?' · '+d.name:''}</td>`;
   const td=document.createElement('td');
   const sel=document.createElement('select');
   sel.style.cssText='padding:4px;background:#1a1a1a;color:#eee;border:1px solid #333';
   ['none','outdoor','return','custom'].forEach(r=>{const o=document.createElement('option');
     o.value=r;o.textContent=r;if(r===role)o.selected=true;sel.appendChild(o);});
   sel.onchange=async()=>{
     let name='';
     if(sel.value==='custom'){
       name=prompt('Custom sensor name (letters/digits/_ , becomes HA entity)',d.name||'');
       if(name===null){loadSensors();return;}
     }
     const r=await jpost('/api/sensors/assign',{addr:d.addr,role:sel.value,name:name||''});
     if(r&&r.ok===false)alert(r.error||'assign failed');
     loadSensors();
   };
   td.appendChild(sel);tr.appendChild(td);rows.appendChild(tr);
  });
  if(!(s.devices||[]).length){rows.innerHTML='<tr><td colspan=4 style="color:#777">No probes found'+(s.pin<0?' — pin not configured for this board':'')+'</td></tr>';}
  const f=(c)=>c&&c.c!=null?c.c+' °C ('+c.src+')':'—';
  sn_out.textContent=f(s.effective?.outdoor);sn_ret.textContent=f(s.effective?.return);
 }catch(e){}
}
function senCfg(on){jpost('/api/sensors/config',{enabled:on}).then(loadSensors)}
async function senTest(){
 try{const r=await jpost('/api/sensors/test',{});
  if(r&&r.ok===false)alert(r.error||'test failed');
  loadSensors();
 }catch(e){alert('test failed');}
}
function setGwMode(m){if(!confirm('Switch gateway mode to '+m+'?\nDevice will save and reboot.'))return;
 jpost('/api/gw/mode',{mode:m}).then(()=>{const x=$('msg')||document.body;
  alert('Saved. Rebooting…');setTimeout(()=>location.reload(),4000)})}
function applyGwOv(){jpost('/api/gw/override',{setpoint:+g_ov_in.value}).then(refresh)}
function releaseGwOv(){jpost('/api/gw/override',{release:true}).then(refresh)}
function show(t){
 document.querySelectorAll('nav button').forEach(b=>b.classList.toggle('act',b.dataset.t===t));
 document.querySelectorAll('section').forEach(s=>s.classList.toggle('act',s.id==='t-'+t));
 if(t==='settings')loadSettings();
 if(t==='sensors')loadSensors();
}
setInterval(()=>{if(document.getElementById('t-sensors').classList.contains('act'))loadSensors();},5000);
document.querySelectorAll('nav button').forEach(b=>b.onclick=()=>show(b.dataset.t));
async function loadSettings(){
 try{const c=await jget('/api/settings');
 s_name.value=c.device_name||'';s_host.value=c.mqtt_host||'';s_port.value=c.mqtt_port||1883;
 s_user.value=c.mqtt_user||'';s_prefix.value=c.mqtt_prefix||'hcs';
 }catch(e){}
 s_pass.value='';s_otapass.value='';
}
async function otLog(){try{const r=await fetch('/api/otlog');const j=await r.json();$('otlog').textContent=(j.lines||[]).join('\n')||'(empty — waiting for frames)';}catch(e){$('otlog').textContent='error: '+e;}}
async function saveSettings(){
 const b={device_name:s_name.value,mqtt_host:s_host.value,mqtt_port:+s_port.value,
 mqtt_user:s_user.value,mqtt_prefix:s_prefix.value||'hcs'};
 if(s_pass.value)b.mqtt_pass=s_pass.value;
 if(s_otapass.value)b.ota_password=s_otapass.value;
 await jpost('/api/settings',b);
 const m=$('msg');m.style.display='block';m.textContent='Saved. Rebooting…';
 setTimeout(()=>location.reload(),9000);
}
async function otaFromUrl(){const u=ourl.value.trim();if(!u)return;
 if(!confirm('Flash firmware from\n'+u+'\n\nDevice will download and reboot.'))return;
 await jpost('/api/ota',{url:u});alert('Update started.');}
async function doReboot(){if(confirm('Reboot device?')){await jpost('/api/reboot');}}
setInterval(refresh,3000);refresh();
</script></body></html>)HTML";

void NetServices::beginHttp(const HcsSettings& settings, const String& nodeId) {
  node_id_ = nodeId;
  settings_ = settings;

  server.on("/", HTTP_GET, [this]() {
    server.send_P(200, "text/html", INDEX_HTML);
  });

  server.on("/api/otlog", HTTP_GET, [this]() {
    bool clear = server.hasArg("clear");
    if (clear) ot_.ot_log.clear();
    JsonDocument doc;
    JsonArray arr = doc["lines"].to<JsonArray>();
    char line[112];
    uint8_t n = ot_.ot_log.count();
    for (uint8_t i = 0; i < n; i++) {
      hcs::OtLog::format(*ot_.ot_log.entry(i), line, sizeof(line));
      arr.add(line);
    }
    String out;
    serializeJson(doc, out);
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", out);
  });

  server.on("/api/status", HTTP_GET, [this]() {
    const OtSnapshot& s = ot_.snap();
    JsonDocument doc;
    doc["node_id"] = node_id_;
    doc["board"] = HCS_BOARD_NAME;
    doc["version"] = HCS_FW_VERSION;
    doc["ip"] = WiFi.localIP().toString();
    doc["name"] = settings_.device_name;
    doc["mqtt_host"] = settings_.mqtt_host;
    doc["mqtt_port"] = settings_.mqtt_port;
    doc["rssi"] = WiFi.RSSI();
    doc["uptime"] = millis() / 1000UL;
    doc["reset_reason"] = reset_reason_;
    doc["unclean_boots"] = unclean_boots_;
    doc["ot_valid"] = s.valid;
    doc["ch_enable"] = ot_.chEnable();
    doc["dhw_enable"] = ot_.dhwEnable();
    doc["ch_active"] = s.ch_active;
    doc["fault"] = s.fault;
    doc["flame"] = s.flame;
    doc["flow_setpoint"] = ot_.flowSetpoint();
    doc["max_modulation"] = ot_.maxModulation();

    doc["wc_enable"] = ot_.weatherComp();
    doc["wc_ref"] = ot_.weatherCompCfg().t_out_ref;
    doc["wc_design"] = ot_.weatherCompCfg().t_out_design;
    doc["wc_fmax"] = ot_.weatherCompCfg().flow_max;
    doc["wc_fmin"] = ot_.weatherCompCfg().flow_min;
    if (!isnan(ot_.wcTarget()))
      doc["wc_target"] = roundf(ot_.wcTarget() * 10) / 10.0f;

#if defined(ESP32) && defined(HCS_GW_ENABLE)
    if (gw_) {
      const hcs::GwCounters& c = gw_->counters();
      JsonObject g = doc["gw"].to<JsonObject>();
      g["mode"] = "gateway";
      g["cfg"] = hcs_gw_cfg_name(settings_.gw_cfg);
      g["tstat_online"] = gw_->thermostatOnline();
      float ov = gw_->overrideSetpointC();
      if (!isnan(ov)) g["override_setpoint"] = ov;
      g["requests"] = c.requests;
      g["forwarded"] = c.forwarded;
      g["answered_local"] = c.answered_local;
      g["modified"] = c.modified;
      g["errors"] = c.errors;
    } else {
      JsonObject g = doc["gw"].to<JsonObject>();
      g["mode"] = "master_only";
      g["cfg"] = hcs_gw_cfg_name(settings_.gw_cfg);
    }
#endif
    // Boiler diagnostics
    {
      hcs::BoilerDiag bd;
      bd.valid_asf = s.valid_asf;
      bd.valid_oem = s.valid_oem;
      bd.asf = s.asf_flags;
      bd.oem = s.oem_diag;
      char txt[160];
      hcs::boiler_diag_text(bd, txt, sizeof(txt));
      JsonObject bdd = doc["boiler_diag"].to<JsonObject>();
      bdd["state"] = hcs::boiler_diag_state(bd);
      bdd["text"] = txt;
      if (bd.valid_asf) bdd["asf"] = bd.asf;
      if (bd.valid_oem) bdd["oem"] = bd.oem;
    }
    // Boiler identity / capability summary
    {
      JsonObject bid = doc["boiler"].to<JsonObject>();
      String ident = "";
      if (s.valid_slave_cfg)
        ident += "slave member " + String(s.slave_member_id) + " config 0x" +
                 String(s.slave_config, HEX);
      if (s.valid_master_cfg) {
        if (ident.length()) ident += "; ";
        ident += "master member " + String(s.master_member_id) + " config 0x" +
                 String(s.master_config, HEX);
      }
      if (s.valid_capacity && ident.length())
        ident += "; " + String(s.capacity_kw) + " kW min-mod " +
                 String(s.min_mod_pct) + "%";
      if (ident.length()) bid["identity"] = ident;
      if (s.valid_fhb && s.fhb_count) {
        char fhb[64];
        hcs::ot_fhb_format(s.fhb_codes, s.fhb_count, fhb, sizeof(fhb));
        bid["fault_history"] = fhb;
      }
      if (s.valid_dhw_bounds)
        bid["dhw_bounds"] =
            String((int)s.dhw_lb) + ".." + String((int)s.dhw_ub);
    }
    // Temperatures present on the snapshot
    auto putc = [&](const char* k, float v) {
      if (!isnan(v)) doc[k] = roundf(v * 10) / 10.0f;
    };
    putc("flow_temp", s.flow_temp);
    putc("return_temp", s.return_temp);
    putc("outdoor_temp", s.outdoor_temp);
    putc("modulation", s.modulation);
    putc("pressure_bar", s.valid_pressure ? s.pressure_bar : NAN);
    if (!isnan(ot_.dhwSetpoint())) doc["dhw_setpoint"] = ot_.dhwSetpoint();

    // Connection-loss failsafe
    {
      HcsSettings& cfg = shared_ ? *shared_ : settings_;
      hcs::FsState st =
          fs_state_ptr_ ? *fs_state_ptr_ : hcs::FsState::CONNECTED;
      const char* names[3] = {"connected", "hold", "failsafe"};
      JsonObject f = doc["failsafe"].to<JsonObject>();
      f["state"] = names[(int)st];
      f["active"] = (st == hcs::FsState::FAILSAFE);
      f["enable"] = cfg.fs_enable;
      f["flow"] = cfg.fs_flow_c;
      f["grace_min"] = cfg.fs_grace_min;
    }
    // MQTT link visibility (host shown post-sanitisation)
    if (MqttBridge* mb = MqttBridge::active()) {
      JsonObject m = doc["mqtt_link"].to<JsonObject>();
      m["connected"] = mb->connectedOrNever();
      m["host"] = mb->host();
      m["port"] = mb->port();
    }

    String j;
    j.reserve(measureJson(doc) + 16);
    serializeJson(doc, j);
    server.send(200, "application/json", j);
  });


  // All mutating endpoints require the admin/OTA password when one is set.
  auto authOk = [this]() -> bool {
    if (server.hasHeader("Origin")) {
      String origin = server.header("Origin");
      String host = server.hostHeader();
      if (origin.length() && host.length() &&
          origin.indexOf(host) < 0) {
        server.send(403, "application/json",
                    "{\"ok\":false,\"error\":\"cross-origin rejected\"}");
        return false;
      }
    }
    if (settings_.ota_password.length() == 0) return true;
    if (server.authenticate("admin", settings_.ota_password.c_str())) return true;
    server.requestAuthentication();
    return false;
  };

  server.on("/api/control", HTTP_POST, [this, authOk]() {
    if (!authOk()) return;
    JsonDocument d;
    DeserializationError e = deserializeJson(d, server.arg("plain"));
    if (e) {
      server.send(400, "application/json",
                  "{\"ok\":false,\"error\":\"bad json\"}");
      return;
    }
    if (!d["ch_enable"].isNull()) ot_.setChEnable(d["ch_enable"].as<bool>());
    if (!d["dhw_enable"].isNull()) ot_.setDhwEnable(d["dhw_enable"].as<bool>());
    if (!d["flow_setpoint"].isNull())
      ot_.setFlowSetpoint(constrain(d["flow_setpoint"].as<float>(), 20.0f, 90.0f));
    if (!d["max_modulation"].isNull())
      ot_.setMaxModulation(constrain(d["max_modulation"].as<int>(), 0, 100));
    if (!d["weather_comp"].isNull()) ot_.setWeatherComp(d["weather_comp"].as<bool>());
    if (!(d["wc_ref"].isNull() && d["wc_design"].isNull() &&
          d["wc_fmax"].isNull() && d["wc_fmin"].isNull())) {
      HcsWeatherComp wc = ot_.weatherCompCfg();
      if (!d["wc_ref"].isNull()) wc.t_out_ref = d["wc_ref"].as<float>();
      if (!d["wc_design"].isNull()) wc.t_out_design = d["wc_design"].as<float>();
      if (!d["wc_fmax"].isNull()) wc.flow_max = d["wc_fmax"].as<float>();
      if (!d["wc_fmin"].isNull()) wc.flow_min = d["wc_fmin"].as<float>();
      char csv[48];
      snprintf(csv, sizeof(csv), "%.1f,%.1f,%.1f,%.1f", wc.t_out_ref,
               wc.t_out_design, wc.flow_max, wc.flow_min);
      ot_.setWeatherCompCfg(csv);  // invalid combos are rejected atomically
    }
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/settings", HTTP_GET, [this]() {
    auto esc = [](const String& v) {
      String o;
      o.reserve(v.length() + 8);
      for (unsigned i = 0; i < v.length(); ++i) {
        char c = v[i];
        if (c == '"' || c == '\\') o += '\\', o += c;
        else if (c == '\n') o += "\\n";
        else if (c == '\r') {}
        else o += c;
      }
      return o;
    };
    auto isSet = [](const String& v) { return v.length() > 0; };
    String j = "{";
    j += "\"device_name\":\"" + esc(settings_.device_name) + "\",";
    j += "\"mqtt_host\":\"" + esc(settings_.mqtt_host) + "\",";
    j += "\"mqtt_port\":" + String(settings_.mqtt_port) + ",";
    j += "\"mqtt_user\":\"" + esc(settings_.mqtt_user) + "\",";
    j += "\"mqtt_user_set\":" + String(isSet(settings_.mqtt_user) ? "true" : "false") + ",";
    j += "\"mqtt_prefix\":\"" + esc(settings_.mqtt_prefix) + "\",";
    j += "\"ota_password_set\":" +
         String(settings_.ota_password.length() ? "true" : "false");
    j += "}";
    server.send(200, "application/json", j);
  });

  server.on("/api/settings", HTTP_POST, [this, authOk]() {
    if (!authOk()) return;
    if (!applySettingsJson(server.arg("plain"))) {
      server.send(400, "application/json",
                  "{\"ok\":false,\"error\":\"bad json\"}");
      return;
    }
    server.send(200, "application/json",
                "{\"ok\":true,\"message\":\"saved, rebooting\"}");
  });

  server.on("/api/ota", HTTP_POST, [this]() {
    if (settings_.ota_password.length()) {
      if (!server.authenticate("admin", settings_.ota_password.c_str())) {
        return server.requestAuthentication();
      }
    }
    String body = server.arg("plain");
    String url;
    if (server.hasArg("url")) url = server.arg("url");
    if (!url.length()) {
      int i = body.indexOf("\"url\"");
      if (i >= 0) {
        int q1 = body.indexOf('"', body.indexOf(':', i) + 1);
        int q2 = body.indexOf('"', q1 + 1);
        if (q1 >= 0 && q2 > q1) url = body.substring(q1 + 1, q2);
      }
    }
    if (!url.length()) {
      server.send(400, "application/json",
                  "{\"ok\":false,\"error\":\"missing url\"}");
      return;
    }
    server.send(200, "application/json",
                "{\"ok\":true,\"message\":\"OTA starting\"}");
    server.client().flush();
    delay(300);
    startHttpUpdate(url);
  });

  server.on("/api/reboot", HTTP_POST, [this]() {
    server.send(200, "application/json", "{\"ok\":true}");
    scheduleReboot();
  });

  // ---- 1-Wire sensors -------------------------------------------------
  // Failsafe configuration (web UI path; MQTT path lands via onFailsafeCfg)
  server.on("/api/failsafe", HTTP_POST, [this, authOk]() {
    if (!authOk()) return;
    JsonDocument d;
    DeserializationError e = deserializeJson(d, server.arg("plain"));
    if (e) {
      server.send(400, "application/json", "{\"ok\":false,\"error\":\"bad json\"}");
      return;
    }
    HcsSettings& cfg = shared_ ? *shared_ : settings_;
    if (d["enable"].is<bool>()) cfg.fs_enable = d["enable"].as<bool>();
    if (d["flow"].is<float>()) {
      float f = d["flow"].as<float>();
      cfg.fs_flow_c = f < 20 ? 20 : (f > 90 ? 90 : f);
    }
    if (d["grace_min"].is<int>()) {
      int g = d["grace_min"].as<int>();
      cfg.fs_grace_min = (uint8_t)constrain(g, 1, 120);
    }
    SettingsStore store;
    store.begin();
    store.save(cfg);
    if (fs_state_ptr_ && *fs_state_ptr_ == hcs::FsState::FAILSAFE &&
        cfg.fs_enable) {
      ot_.setChEnable(true);
      ot_.setFlowSetpoint(cfg.fs_flow_c);
    }
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/sensors", HTTP_GET, [this]() {
    HcsSettings& cfgR = shared_ ? *shared_ : settings_;
    JsonDocument d;
    d["enabled"] = sensors_ ? sensors_->enabled() : false;
    d["pin"] = HCS_ONEWIRE_PIN;
    size_t n = sensors_ ? sensors_->count() : 0;
    d["count"] = (int)n;
    JsonArray arr = d["devices"].to<JsonArray>();
    unsigned long now = millis();
    for (size_t i = 0; i < n; i++) {
      const hcs::OwDevice& dev = sensors_->device(i);
      hcs::OwSlot sl = sensors_->slotForDevice(i);
      JsonObject o = arr.add<JsonObject>();
      char hex[17];
      hcs::ow_addr_to_hex(dev.addr, hex);
      o["addr"] = hex;
      o["health"] = hcs::ow_health_name(dev.health);
      o["family"] = dev.family;
      o["role"] = hcs::ow_role_name((hcs::OwRole)sl.role);
      if (sl.role == hcs::OW_ROLE_CUSTOM && sl.name[0]) o["name"] = sl.name;
      if (dev.valid && dev.health == hcs::OW_HEALTH_OK &&
          now - dev.ts_ms <= hcs::kOwStaleMs)
        o["temp_c"] = roundf(dev.celsius * 10) / 10.0f;
      else
        o["temp_c"] = nullptr;
    }
    JsonObject roles = d["roles"].to<JsonObject>();
    roles["outdoor"] = cfgR.ow_addr_outdoor();
    roles["return"] = cfgR.ow_addr_return();
    JsonObject eff = d["effective"].to<JsonObject>();
    auto chan = [&](const char* key, bool assigned, float snap_c) {
      hcs::TempValue s = sensors_
            ? sensors_->roleValue(
                  strcmp(key, "outdoor") == 0 ? hcs::OW_ROLE_OUTDOOR
                                              : hcs::OW_ROLE_RETURN,
                  now)
            : hcs::TempValue();
      hcs::TempValue v = hcs::resolve_temp(assigned, s.valid, s.celsius,
                                           !isnan(snap_c), snap_c);
      JsonObject c = eff[key].to<JsonObject>();
      if (v.valid) c["c"] = roundf(v.celsius * 10) / 10.0f; else c["c"] = nullptr;
      c["src"] = (assigned && s.valid) ? "sensor"
               : !isnan(snap_c)        ? "opentherm"
                                       : "none";
    };
    chan("outdoor", sensors_ && sensors_->outdoorAssigned(),
         ot_.snap().outdoor_temp);
    chan("return", sensors_ && sensors_->returnAssigned(),
         ot_.snap().return_temp);
    String j;
    serializeJson(d, j);
    server.send(200, "application/json", j);
  });

  server.on("/api/sensors/config", HTTP_POST, [this, authOk]() {
    if (!authOk()) return;
    JsonDocument d;
    DeserializationError e = deserializeJson(d, server.arg("plain"));
    if (e || d["enabled"].is<bool>() == false) {
      server.send(400, "application/json",
                  "{\"ok\":false,\"error\":\"enabled:boolean required\"}");
      return;
    }
    HcsSettings& cfg = shared_ ? *shared_ : settings_;
    cfg.ow_enable = d["enabled"].as<bool>();
    SettingsStore store;
    store.begin();
    store.save(cfg);
    if (sensors_) {
      sensors_->configure(cfg.ow_enable, cfg.ow_slots, hcs::kOwMaxSlots);
    }
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/sensors/assign", HTTP_POST, [this, authOk]() {
    if (!authOk()) return;
    JsonDocument d;
    DeserializationError e = deserializeJson(d, server.arg("plain"));
    const char* addr = e ? "" : (const char*)(d["addr"] | "");
    const char* role = e ? "" : (const char*)(d["role"] | "");
    const char* name = e ? "" : (const char*)(d["name"] | "");
    hcs::OwRole r;
    if (!hcs::ow_role_from_name(role, &r)) {
      server.send(400, "application/json",
                  "{\"ok\":false,\"error\":\"role must be "
                  "none|outdoor|return|custom\"}");
      return;
    }
    HcsSettings& cfg = shared_ ? *shared_ : settings_;
    if (!hcs::ow_assign(cfg.ow_slots, hcs::kOwMaxSlots, addr, r, name)) {
      server.send(400, "application/json",
                  "{\"ok\":false,\"error\":\"bad addr, full table, or "
                  "invalid/duplicate custom name\"}");
      return;
    }
    cfg.ow_slot_count = hcs::kOwMaxSlots;
    SettingsStore store;
    store.begin();
    store.save(cfg);
    if (sensors_) {
      sensors_->configure(cfg.ow_enable, cfg.ow_slots, hcs::kOwMaxSlots);
    }
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/sensors/test", HTTP_POST, [this, authOk]() {
    if (!authOk()) return;
    if (!sensors_) {
      server.send(400, "application/json",
                  "{\"ok\":false,\"error\":\"no sensors\"}");
      return;
    }
    sensors_->selftestAll();
    JsonDocument d;
    d["ok"] = true;
    d["enabled"] = sensors_->enabled();
    d["pin"] = HCS_ONEWIRE_PIN;
    size_t n = sensors_->count();
    d["count"] = (int)n;
    JsonArray arr = d["devices"].to<JsonArray>();
    for (size_t i = 0; i < n; i++) {
      const hcs::OwDevice& dev = sensors_->device(i);
      hcs::OwSlot sl = sensors_->slotForDevice(i);
      JsonObject o = arr.add<JsonObject>();
      char hex[17];
      hcs::ow_addr_to_hex(dev.addr, hex);
      o["addr"] = hex;
      o["health"] = hcs::ow_health_name(dev.health);
      o["role"] = hcs::ow_role_name((hcs::OwRole)sl.role);
      if (sl.role == hcs::OW_ROLE_CUSTOM && sl.name[0]) o["name"] = sl.name;
      if (dev.valid && dev.health == hcs::OW_HEALTH_OK)
        o["temp_c"] = roundf(dev.celsius * 10) / 10.0f;
      else
        o["temp_c"] = nullptr;
    }
    String j;
    serializeJson(d, j);
    server.send(200, "application/json", j);
  });

#if defined(ESP32) && defined(HCS_GW_ENABLE)
  server.on("/api/gw/mode", HTTP_POST, [this, authOk]() {
    if (!authOk()) return;
    JsonDocument d;
    DeserializationError e = deserializeJson(d, server.arg("plain"));
    const char* m = e ? "" : (const char*)(d["mode"] | "");
    if (!strlen(m) || (strcmp(m, "gateway") && strcmp(m, "master_only") &&
                       strcmp(m, "auto"))) {
      server.send(400, "application/json",
                  "{\"ok\":false,\"error\":\"mode must be auto|gateway|master_only\"}");
      return;
    }
    uint8_t cfg = strcmp(m, "gateway") == 0   ? HCS_GW_GATEWAY
                  : strcmp(m, "master_only") == 0 ? HCS_GW_MASTER_ONLY
                                                  : HCS_GW_AUTO;
    settings_.gw_cfg = cfg;
    SettingsStore store;
    store.begin();
    store.save(settings_);
    server.send(200, "application/json",
                "{\"ok\":true,\"message\":\"saved, rebooting\"}");
    scheduleReboot(800);
  });

  server.on("/api/gw/override", HTTP_POST, [this, authOk]() {
    if (!authOk()) return;
    if (!gw_) {
      server.send(409, "application/json",
                  "{\"ok\":false,\"error\":\"gateway not active\"}");
      return;
    }
    JsonDocument d;
    DeserializationError e = deserializeJson(d, server.arg("plain"));
    if (e) {
      server.send(400, "application/json",
                  "{\"ok\":false,\"error\":\"bad json\"}");
      return;
    }
    if (!d["release"].isNull() && d["release"].as<bool>()) {
      gw_->setOverrideSetpointC((float)NAN);
    } else if (!d["setpoint"].isNull()) {
      gw_->setOverrideSetpointC(
          constrain(d["setpoint"].as<float>(), 20.0f, 90.0f));
    }
    server.send(200, "application/json", "{\"ok\":true}");
  });
#endif

  // Audit F14: collect Origin so mutating handlers can reject cross-site
  // form posts (drive-by CSRF) even when no password is configured.
#ifdef ESP8266
  server.collectHeaders(String("Origin"));
#else
  static const char* kCorsHeaders[] = {"Origin"};
  server.collectHeaders(kCorsHeaders, 1);
#endif

  server.begin();
  http_started_ = true;
  Serial.printf("[http] http://%s/  OTA POST /api/ota\n",
                WiFi.localIP().toString().c_str());
}

void NetServices::beginArduinoOta(const HcsSettings& settings,
                                  const String& hostname) {
  ArduinoOTA.setHostname(hostname.c_str());
  if (settings.ota_password.length()) {
    ArduinoOTA.setPassword(settings.ota_password.c_str());
  }
  ArduinoOTA.onStart([]() { Serial.println(F("[ota] ArduinoOTA start")); });
  ArduinoOTA.onEnd([]() { Serial.println(F("[ota] ArduinoOTA end")); });
  ArduinoOTA.onError([](ota_error_t e) {
    Serial.printf("[ota] error %u\n", (unsigned)e);
  });
  ArduinoOTA.begin();
}

void NetServices::loop() {
  otaRollbackTick();
  httpSelfProbeTick_();
  if (reboot_pending_ && millis() > reboot_at_ms_) {
    Serial.println(F("[http] rebooting now"));
    delay(50);
    ESP.restart();
  }
  if (http_started_) {
    server.handleClient();
  }
  ArduinoOTA.handle();
}

void NetServices::otaReport(const String& state, int progress,
                            const String& error) {
  if (!ota_report_) return;
  String j = "{\"state\":\"" + state + "\"";
  if (progress >= 0) j += ",\"progress\":" + String(progress);
  if (error.length()) {
    String e = error;
    e.replace("\\", "\\\\");
    e.replace("\"", "\\\"");
    j += ",\"error\":\"" + e + "\"";
  }
  j += "}";
  ota_last_report_ms_ = millis();
  ota_report_(j);
}

String NetServices::settingsSnapshotJson() const {
  auto esc = [](const String& v) {
    String o;
    o.reserve(v.length() + 8);
    for (unsigned i = 0; i < v.length(); ++i) {
      char c = v[i];
      if (c == '"' || c == '\\') o += '\\', o += c;
      else if (c == '\n') o += "\\n";
      else if (c == '\r') {}
      else o += c;
    }
    return o;
  };
  auto isSet = [](const String& v) { return v.length() > 0; };
  String j = "{";
  j += "\"device_name\":\"" + esc(settings_.device_name) + "\",";
  j += "\"mqtt_host\":\"" + esc(settings_.mqtt_host) + "\",";
  j += "\"mqtt_port\":" + String(settings_.mqtt_port) + ",";
  j += "\"mqtt_user\":\"" + esc(settings_.mqtt_user) + "\",";
  j += "\"mqtt_user_set\":" + String(isSet(settings_.mqtt_user) ? "true" : "false") + ",";
  j += "\"mqtt_prefix\":\"" + esc(settings_.mqtt_prefix) + "\",";
  j += "\"ota_password_set\":" +
       String(settings_.ota_password.length() ? "true" : "false");
  j += "}";
  return j;
}

bool NetServices::applySettingsJson(const String& json) {
  JsonDocument d;
  if (deserializeJson(d, json)) return false;

  const char* v;
  if ((v = d["device_name"] | (const char*)nullptr))
    settings_.device_name = hcs_trim(v).substring(0, 31);
  if ((v = d["mqtt_host"] | (const char*)nullptr))
    settings_.mqtt_host = hcs_trim(v).substring(0, 63);
  int p = d["mqtt_port"] | -1;
  if (p > 0 && p < 65536) settings_.mqtt_port = (uint16_t)p;
  if ((v = d["mqtt_user"] | (const char*)nullptr))
    settings_.mqtt_user = hcs_trim(v).substring(0, 31);
  if ((v = d["mqtt_pass"] | (const char*)nullptr))
    settings_.mqtt_pass = String(v).substring(0, 31);
  if ((v = d["mqtt_prefix"] | (const char*)nullptr))
    settings_.mqtt_prefix = hcs_trim(v).substring(0, 15);
  if ((v = d["ota_password"] | (const char*)nullptr))
    settings_.ota_password = String(v).substring(0, 31);

  SettingsStore store;
  store.begin();
  store.save(settings_);

  // Echo the new state before the reboot pulls the MQTT link down.
  if (cfg_report_) cfg_report_(settingsSnapshotJson());
  scheduleReboot(1500);
  return true;
}

bool NetServices::startHttpUpdate(const String& url) {
  // Reentrancy guard: MQTT + HTTP fallback can both deliver the command.
  if (ota_busy_) return false;
  ota_busy_ = true;
  ota_last_progress_ = -1;

  Serial.printf("[ota] pulling %s\n", url.c_str());
  otaMarkTarget(url);
  otaReport("starting", 0, "");

  auto progress = [this](int cur, int total) {
    if (total <= 0) return;
    int pct = (int)((long long)cur * 100 / total);
    unsigned long now = millis();
    if (pct != ota_last_progress_ &&
        (pct - ota_last_progress_ >= 4 ||
         now - ota_last_report_ms_ >= 500 || pct >= 100)) {
      ota_last_progress_ = pct;
      otaReport("downloading", pct, "");
    }
  };

  // Scheme-aware client: TLS only for https:// URLs. Plain-LAN mirrors
  // (and any http:// source) must NOT be spoken TLS to.
  const bool use_tls = url.startsWith("https");
  t_httpUpdate_return ret;
#if defined(ESP8266)
  ESPhttpUpdate.rebootOnUpdate(false);  // we publish "done", then reboot
  ESPhttpUpdate.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  ESPhttpUpdate.onProgress(progress);
  if (use_tls) {
    WiFiClientSecure client;
    client.setInsecure();  // release assets are md5-verified upstream
    client.setTimeout(12);
    ret = ESPhttpUpdate.update(client, url);
  } else {
    WiFiClient client;
    client.setTimeout(12);
    ret = ESPhttpUpdate.update(client, url);
  }
#elif defined(ESP32)
  httpUpdate.rebootOnUpdate(false);
  httpUpdate.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  httpUpdate.onProgress(progress);
  if (use_tls) {
    WiFiClientSecure client;
    client.setInsecure();
    ret = httpUpdate.update(client, url);
  } else {
    WiFiClient client;
    ret = httpUpdate.update(client, url);
  }
#else
  ota_busy_ = false;
  return false;
#endif

  switch (ret) {
    case HTTP_UPDATE_FAILED: {
#if defined(ESP8266)
      String err = ESPhttpUpdate.getLastErrorString();
      int code = (int)ESPhttpUpdate.getLastError();
#else
      String err = httpUpdate.getLastErrorString();
      int code = (int)httpUpdate.getLastError();
#endif
      Serial.printf("[ota] fail (%d) %s\n", code, err.c_str());
      String msg = err.length() ? err : "update failed";
      msg += " (code " + String(code) + ")";
      otaReport("failed", -1, msg);
      break;
    }
    case HTTP_UPDATE_NO_UPDATES:
      Serial.println(F("[ota] no updates"));
      otaReport("failed", -1, "server sent no update image");
      break;
    case HTTP_UPDATE_OK:
      Serial.println(F("[ota] ok — rebooting"));
      otaReport("done", 100, "");
      scheduleReboot(1200);  // let the "done" report drain first
      break;
  }
  ota_busy_ = false;
  return ret == HTTP_UPDATE_OK;
}

void NetServices::scheduleReboot(unsigned long delayMs) {
  reboot_pending_ = true;
  reboot_at_ms_ = millis() + delayMs;
  Serial.printf("[http] reboot scheduled in %lums\n", (unsigned long)delayMs);
}


// ── OTA rollback watchdog ────────────────────────────────────────────────
// Before every remote flash we persist the target URL. After boot we wait
// OTA_ROLL_CONFIRM_MS for MQTT to prove the image works; a healthy boot
// promotes target→known-good. If MQTT never comes up within
// OTA_ROVERT_MS and we still know a good image, we pull that one back.
// Attempts are capped so a genuinely broken pair of images can't loop.

static const char* ROLL_PATH = "/otaroll.json";
constexpr unsigned long OTA_CONFIRM_AFTER_MS = 90000;
constexpr unsigned long OTA_REVERT_AFTER_MS  = 180000;
constexpr uint8_t      OTA_MAX_ATTEMPTS      = 3;

void NetServices::otaRollLoad_() {
  if (roll_loaded_) return;
  roll_loaded_ = true;
  File f = LittleFS.open(ROLL_PATH, "r");
  if (!f) return;
  JsonDocument d;
  if (!deserializeJson(d, f)) {
    roll_target_url_ = d["t"] | "";
    roll_good_url_   = d["g"] | "";
    roll_attempts_   = d["a"] | 0;
    roll_pending_    = roll_target_url_.length() > 0;
  }
  f.close();
}

void NetServices::otaRollSave_() {
  JsonDocument d;
  d["t"] = roll_target_url_;
  d["g"] = roll_good_url_;
  d["a"] = roll_attempts_;
  File f = LittleFS.open(ROLL_PATH, "w");
  if (!f) return;
  serializeJson(d, f);
  f.close();
}

void NetServices::otaMarkTarget(const String& url) {
  otaRollLoad_();
  roll_target_url_ = url;
  roll_pending_    = true;
  otaRollSave_();
  Serial.printf("[roll] target marked (attempt %u): %s\n", roll_attempts_ + 1, url.c_str());
}

void NetServices::otaRollbackTick() {
  if (!roll_pending_) return;
  otaRollLoad_();
  const unsigned long up = millis();
  const bool mqtt_ok = mqtt_ok_fn_ ? mqtt_ok_fn_() : false;

  if (up >= OTA_CONFIRM_AFTER_MS && mqtt_ok) {
    Serial.println(F("[roll] image confirmed healthy"));
    roll_good_url_    = roll_target_url_;
    roll_target_url_  = "";
    roll_attempts_    = 0;
    roll_pending_     = false;
    otaRollSave_();
    return;
  }

  if (up >= OTA_REVERT_AFTER_MS && !mqtt_ok) {
    if (roll_attempts_ + 1 >= OTA_MAX_ATTEMPTS || roll_good_url_.length() == 0) {
      Serial.printf("[roll] no safe revert possible (attempts=%u, good=%s) — giving up\n",
                    roll_attempts_, roll_good_url_.c_str());
      roll_pending_ = false;
      otaRollSave_();
      return;
    }
    roll_attempts_++;
    otaRollSave_();
    Serial.printf("[roll] reverting to known-good: %s\n", roll_good_url_.c_str());
    roll_pending_ = false;          // otaMarkTarget will re-mark with prev URL
    startHttpUpdate(roll_good_url_);
  }
}


// ── HTTP self-probe ──────────────────────────────────────────────────────
// The web layer can wedge (TCP accepted by lwip, app never answers) while
// MQTT in the same superloop keeps working. Probe ourselves every minute;
// two consecutive failures → reboot, so a stuck board heals unattended.
constexpr unsigned long HTTP_PROBE_INTERVAL_MS = 60000;

void NetServices::httpSelfProbeTick_() {
  if (!http_started_ || ota_busy_) return;
  if (millis() - http_probe_ms_ < HTTP_PROBE_INTERVAL_MS) return;
  http_probe_ms_ = millis();

  bool ok = false;
  WiFiClient c;
  // Hard deadline read: WiFiClient::setTimeout units differ per core
  // (ESP32 = seconds!), which once blocked the whole superloop here.
  if (c.connect(IPAddress(127, 0, 0, 1), HTTP_PORT)) {
    c.print(F("GET /api/status HTTP/1.0\r\n\r\n"));
    const unsigned long deadline = millis() + 2000;
    String line;
    while (millis() < deadline) {
      while (c.available()) {
        char ch = (char)c.read();
        if (ch == '\n') break;
        line += ch;
      }
      if (line.length()) break;
      delay(2);
    }
    ok = line.startsWith("HTTP/1");
    c.stop();
  }
  http_probe_fail_ = ok ? 0 : (uint8_t)(http_probe_fail_ + 1);
  if (http_probe_fail_ >= 2) {
    Serial.println(F("[http] self-probe failed twice — restarting"));
    http_probe_fail_ = 0;
    scheduleReboot();
  }
}
