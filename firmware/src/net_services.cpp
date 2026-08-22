#include "net_services.h"
#include "config.h"

#include <WiFiManager.h>
#include <ArduinoOTA.h>

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

#include <ElegantOTA.h>
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
  WiFiManagerParameter p_node("otgw_node", "OTGW compat node id",
                              settings.otgw_node.c_str(), 32);
  WiFiManagerParameter p_name("dev_name", "Device name",
                              settings.device_name.c_str(), 32);
  WiFiManagerParameter p_ota("ota_pass", "OTA password (optional)",
                             settings.ota_password.c_str(), 32);

  wm.addParameter(&p_mqtt_host);
  wm.addParameter(&p_mqtt_port);
  wm.addParameter(&p_mqtt_user);
  wm.addParameter(&p_mqtt_pass);
  wm.addParameter(&p_prefix);
  wm.addParameter(&p_node);
  wm.addParameter(&p_name);
  wm.addParameter(&p_ota);

  // autoConnect(apName, apPassword) — AP name is NOT the home SSID.
  // Saved STA creds live in WiFiManager's own NVS; optional seed below.
  if (settings.configured && settings.wifi_ssid.length()) {
    WiFi.begin(settings.wifi_ssid.c_str(), settings.wifi_pass.c_str());
  }
  Serial.printf("[wifi] portal AP fallback: %s\n", PORTAL_AP_NAME);
  bool ok = wm.autoConnect(PORTAL_AP_NAME, PORTAL_AP_PASS);

  if (!ok) {
    Serial.println(F("[wifi] failed / portal timeout — retrying portal"));
    ok = wm.startConfigPortal(PORTAL_AP_NAME, PORTAL_AP_PASS);
  }

  if (!ok) {
    Serial.println(F("[wifi] giving up"));
    return false;
  }

  // Persist whatever we have after successful association
  settings.wifi_ssid = WiFi.SSID();
  settings.wifi_pass = WiFi.psk();
  if (strlen(p_mqtt_host.getValue())) {
    settings.mqtt_host = p_mqtt_host.getValue();
    settings.mqtt_port = (uint16_t)atoi(p_mqtt_port.getValue());
    if (!settings.mqtt_port) settings.mqtt_port = 1883;
    settings.mqtt_user = p_mqtt_user.getValue();
    settings.mqtt_pass = p_mqtt_pass.getValue();
    String pref = p_prefix.getValue();
    if (pref.length()) settings.mqtt_prefix = pref;
    String node = p_node.getValue();
    if (node.length()) settings.otgw_node = node;
    String nm = p_name.getValue();
    if (nm.length()) settings.device_name = nm;
    settings.ota_password = p_ota.getValue();
  }
  // Compile-time fallbacks if portal left MQTT empty
  if (!settings.mqtt_host.length() && strlen(MQTT_HOST)) {
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
<header><h1 id=devname>Home Climate</h1><span class="badge b-off" id=otb>OT ?</span></header>
<nav>
<button data-t=status class=act>Status</button>
<button data-t=controls>Controls</button>
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
</div></section>
<section id=t-settings>
<div class=card>
<label>Device name</label><input id=s_name maxlength=31>
<label>MQTT broker host</label><input id=s_host maxlength=63 placeholder=e.g. homeassistant.local>
<label>MQTT port</label><input id=s_port type=number value=1883 min=1 max=65535>
<label>MQTT user</label><input id=s_user maxlength=31 autocomplete=off>
<label>MQTT password</label><input id=s_pass maxlength=31 type=password autocomplete=new-password placeholder=(unchanged)>
<label>Topic prefix</label><input id=s_prefix maxlength=15 value=hcs>
<label>OTGW-compat node id</label><input id=s_node maxlength=31 value=hcs-device>
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
<label>Firmware update over the air</label>
<a class=a href=/update>ElegantOTA updater</a>
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
async function jpost(u,b){await fetch(u,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(b||{})})}
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
 if(!['wc_ref','wc_design','wc_fmax','wc_fmin'].includes(document.activeElement?.id)){
  wc_ref.value=s.wc_ref;wc_design.value=s.wc_design;wc_fmax.value=s.wc_fmax;wc_fmin.value=s.wc_fmin;}
}
function applyWc(){ctl({wc_ref:+wc_ref.value,wc_design:+wc_design.value,
 wc_fmax:+wc_fmax.value,wc_fmin:+wc_fmin.value})}
async function refresh(){try{paint(await jget('/api/status'))}catch(e){$('otb').textContent='API err'}}
function ctl(b){jpost('/api/control',b).then(refresh)}
function show(t){
 document.querySelectorAll('nav button').forEach(b=>b.classList.toggle('act',b.dataset.t===t));
 document.querySelectorAll('section').forEach(s=>s.classList.toggle('act',s.id==='t-'+t));
 if(t==='settings')loadSettings();
}
document.querySelectorAll('nav button').forEach(b=>b.onclick=()=>show(b.dataset.t));
async function loadSettings(){
 try{const c=await jget('/api/settings');
 s_name.value=c.device_name||'';s_host.value=c.mqtt_host||'';s_port.value=c.mqtt_port||1883;
 s_user.value=c.mqtt_user||'';s_prefix.value=c.mqtt_prefix||'hcs';s_node.value=c.otgw_node||'hcs-device';
 }catch(e){}
 s_pass.value='';s_otapass.value='';
}
async function saveSettings(){
 const b={device_name:s_name.value,mqtt_host:s_host.value,mqtt_port:+s_port.value,
 mqtt_user:s_user.value,mqtt_prefix:s_prefix.value||'hcs',otgw_node:s_node.value||'hcs-device'};
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

  server.on("/api/status", HTTP_GET, [this]() {
    OtSnapshot s = ot_.snap();
    String j = "{";
    j += "\"node_id\":\"" + node_id_ + "\",";
    j += "\"board\":\"" + String(HCS_BOARD_NAME) + "\",";
    j += "\"version\":\"" + String(HCS_FW_VERSION) + "\",";
    j += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
    j += "\"name\":\"" + settings_.device_name + "\",";
    j += "\"mqtt_host\":\"" + settings_.mqtt_host + "\",";
    j += "\"mqtt_port\":" + String(settings_.mqtt_port) + ",";
    j += "\"rssi\":" + String(WiFi.RSSI()) + ",";
    j += "\"uptime\":" + String(millis() / 1000UL) + ",";
    j += "\"ot_valid\":" + String(s.valid ? "true" : "false") + ",";
    j += "\"ch_enable\":" + String(ot_.chEnable() ? "true" : "false") + ",";
    j += "\"dhw_enable\":" + String(ot_.dhwEnable() ? "true" : "false") + ",";
    j += "\"ch_active\":" + String(s.ch_active ? "true" : "false") + ",";
    j += "\"fault\":" + String(s.fault ? "true" : "false") + ",";
    j += "\"flame\":" + String(s.flame ? "true" : "false") + ",";
    j += "\"flow_setpoint\":" + String(ot_.flowSetpoint(), 1) + ",";
    j += "\"max_modulation\":" + String(ot_.maxModulation());
    const HcsWeatherComp& wc = ot_.weatherCompCfg();
    j += ",\"wc_enable\":" + String(ot_.weatherComp() ? "true" : "false");
    j += ",\"wc_ref\":" + String(wc.t_out_ref, 1);
    j += ",\"wc_design\":" + String(wc.t_out_design, 1);
    j += ",\"wc_fmax\":" + String(wc.flow_max, 1);
    j += ",\"wc_fmin\":" + String(wc.flow_min, 1);
    if (!isnan(ot_.wcTarget())) j += ",\"wc_target\":" + String(ot_.wcTarget(), 1);
    if (!isnan(s.flow_temp)) j += ",\"flow_temp\":" + String(s.flow_temp, 1);
    if (!isnan(s.return_temp))
      j += ",\"return_temp\":" + String(s.return_temp, 1);
    if (!isnan(s.outdoor_temp)) j += ",\"outdoor_temp\":" + String(s.outdoor_temp, 1);
    if (!isnan(s.modulation)) j += ",\"modulation\":" + String(s.modulation, 1);
    j += "}";
    server.send(200, "application/json", j);
  });

  server.on("/api/control", HTTP_POST, [this]() {
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
    auto isSet = [](const String& v) { return v.length() > 0; };
    String j = "{";
    j += "\"device_name\":\"" + settings_.device_name + "\",";
    j += "\"mqtt_host\":\"" + settings_.mqtt_host + "\",";
    j += "\"mqtt_port\":" + String(settings_.mqtt_port) + ",";
    j += "\"mqtt_user\":\"" + settings_.mqtt_user + "\",";
    j += "\"mqtt_user_set\":" + String(isSet(settings_.mqtt_user) ? "true" : "false") + ",";
    j += "\"mqtt_prefix\":\"" + settings_.mqtt_prefix + "\",";
    j += "\"otgw_node\":\"" + settings_.otgw_node + "\",";
    j += "\"ota_password_set\":" +
         String(settings_.ota_password.length() ? "true" : "false");
    j += "}";
    server.send(200, "application/json", j);
  });

  server.on("/api/settings", HTTP_POST, [this]() {
    JsonDocument d;
    DeserializationError e = deserializeJson(d, server.arg("plain"));
    if (e) {
      server.send(400, "application/json",
                  "{\"ok\":false,\"error\":\"bad json\"}");
      return;
    }
    const char* v;
    if ((v = d["device_name"] | (const char*)nullptr))
      settings_.device_name = String(v).substring(0, 31);
    if ((v = d["mqtt_host"] | (const char*)nullptr))
      settings_.mqtt_host = String(v).substring(0, 63);
    int p = d["mqtt_port"] | -1;
    if (p > 0 && p < 65536) settings_.mqtt_port = (uint16_t)p;
    if ((v = d["mqtt_user"] | (const char*)nullptr))
      settings_.mqtt_user = String(v).substring(0, 31);
    if ((v = d["mqtt_pass"] | (const char*)nullptr))
      settings_.mqtt_pass = String(v).substring(0, 31);
    if ((v = d["mqtt_prefix"] | (const char*)nullptr))
      settings_.mqtt_prefix = String(v).substring(0, 15);
    if ((v = d["otgw_node"] | (const char*)nullptr))
      settings_.otgw_node = String(v).substring(0, 31);
    if ((v = d["ota_password"] | (const char*)nullptr))
      settings_.ota_password = String(v).substring(0, 31);

    SettingsStore store;
    store.begin();
    store.save(settings_);

    server.send(200, "application/json",
                "{\"ok\":true,\"message\":\"saved, rebooting\"}");
    scheduleReboot();
  });

  server.on("/api/ota", HTTP_POST, [this]() {
    if (settings_.ota_password.length()) {
      if (!server.authenticate("ota", settings_.ota_password.c_str())) {
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

  ElegantOTA.begin(&server);
  if (settings.ota_password.length()) {
    ElegantOTA.setAuth("ota", settings.ota_password.c_str());
  }

  server.begin();
  http_started_ = true;
  Serial.printf("[http] http://%s/  OTA /update\n",
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
  if (reboot_pending_ && millis() > reboot_at_ms_) {
    Serial.println(F("[http] rebooting now"));
    delay(50);
    ESP.restart();
  }
  if (http_started_) {
    server.handleClient();
    ElegantOTA.loop();
  }
  ArduinoOTA.handle();
}

bool NetServices::startHttpUpdate(const String& url) {
  Serial.printf("[ota] pulling %s\n", url.c_str());
  WiFiClient client;
#if defined(ESP8266)
  ESPhttpUpdate.rebootOnUpdate(true);
  t_httpUpdate_return ret = ESPhttpUpdate.update(client, url);
#elif defined(ESP32)
  httpUpdate.rebootOnUpdate(true);
  t_httpUpdate_return ret = httpUpdate.update(client, url);
#else
  return false;
#endif
  switch (ret) {
    case HTTP_UPDATE_FAILED:
#if defined(ESP8266)
      Serial.printf("[ota] fail %s\n", ESPhttpUpdate.getLastErrorString().c_str());
#else
      Serial.printf("[ota] fail %s\n", httpUpdate.getLastErrorString().c_str());
#endif
      return false;
    case HTTP_UPDATE_NO_UPDATES:
      Serial.println(F("[ota] no updates"));
      return false;
    case HTTP_UPDATE_OK:
      Serial.println(F("[ota] ok"));
      return true;
  }
  return false;
}

void NetServices::scheduleReboot(unsigned long delayMs) {
  reboot_pending_ = true;
  reboot_at_ms_ = millis() + delayMs;
  Serial.printf("[http] reboot scheduled in %lums\n", (unsigned long)delayMs);
}
