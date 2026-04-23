#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <U8g2lib.h>
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0);

const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
WebServer server(80);

#define PIR_PIN 5
#define RED_LED 19
#define GREEN_LED 18
#define BUZZER 23

enum State { BOOT, WARMUP, IDLE, MOTION, CAPTURE, RESULT, RESTART };
State state = BOOT;
unsigned long stateTime = 0;
String name = "";
String conf = "";
bool isKnown = false;

String systemStatus = "SAFE";
String lastPerson = "None";
String confidence = "0";
String logs = "<div class='ll'><span class='ts'>--:--</span><span class='info'>&gt; System initialized.</span></div>";

bool hold = false;
unsigned long holdStart = 0;
int holdTime = 0;

void show(String l1, String l2, int duration = 0) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(0, 25, l1.c_str());
  u8g2.drawStr(0, 50, l2.c_str());
  u8g2.sendBuffer();
  if (duration > 0) { hold = true; holdTime = duration; holdStart = millis(); }
}

void buzzerBeep() {
  for (int i = 0; i < 5; i++) {
    digitalWrite(BUZZER, HIGH); delay(200);
    digitalWrite(BUZZER, LOW);  delay(200);
  }
}

String getTime() {
  unsigned long s = millis() / 1000;
  unsigned long m = (s / 60) % 60;
  s = s % 60;
  char buf[6];
  sprintf(buf, "%02lu:%02lu", m, s);
  return String(buf);
}

// ... HTML code remains exactly the same ...
String page = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Security Hub</title>
<style>
  *{box-sizing:border-box;margin:0;padding:0}
  body{font-family:system-ui,-apple-system,sans-serif;background:#f0f4f8;color:#1e293b;padding:20px;display:flex;justify-content:center}
  .wrap{width:100%;max-width:480px}
  .topbar{display:flex;align-items:center;justify-content:space-between;padding:16px 0 20px}
  .topbar-left{display:flex;align-items:center;gap:10px}
  .title{font-size:15px;font-weight:600;letter-spacing:1px;color:#1e293b}
  .subtitle{font-size:11px;color:#94a3b8;letter-spacing:2px;text-transform:uppercase}
  .uptime-pill{background:#fff;border:1px solid #e2e8f0;border-radius:999px;padding:5px 12px;font-size:11px;color:#64748b;display:flex;align-items:center;gap:6px}
  .uptime-dot{width:6px;height:6px;border-radius:50%;background:#22c55e;animation:blink 2s infinite}
  @keyframes blink{0%,100%{opacity:1}50%{opacity:.4}}
  @keyframes pulse{0%,100%{box-shadow:0 0 0 0 rgba(239,68,68,.3)}50%{box-shadow:0 0 0 6px rgba(239,68,68,0)}}
  .status-block{border-radius:16px;padding:20px 24px;margin-bottom:14px;display:flex;align-items:center;justify-content:space-between;background:#fff;border:1px solid #e2e8f0;transition:all .3s}
  .status-block.alert{background:#fff5f5;border-color:#fca5a5;animation:pulse 1.2s infinite}
  .slabel{font-size:11px;letter-spacing:2px;text-transform:uppercase;color:#94a3b8;margin-bottom:6px}
  .sbadge{display:inline-flex;align-items:center;gap:7px;padding:7px 16px;border-radius:999px;font-size:13px;font-weight:600;letter-spacing:1.5px;text-transform:uppercase}
  .safe{background:#f0fdf4;color:#16a34a;border:1px solid #bbf7d0}
  .alert-b{background:#fef2f2;color:#dc2626;border:1px solid #fca5a5}
  .sdot{width:7px;height:7px;border-radius:50%}
  .sdot-safe{background:#22c55e;animation:blink 2s infinite}
  .sdot-alert{background:#ef4444;animation:blink .5s infinite}
  .sicon{opacity:.1}
  .status-block.alert .sicon{opacity:.15}
  .grid{display:grid;grid-template-columns:1fr 1fr 1fr;gap:10px;margin-bottom:14px}
  .card{background:#fff;border:1px solid #e2e8f0;border-radius:14px;padding:14px}
  .clabel{font-size:10px;letter-spacing:2px;text-transform:uppercase;color:#94a3b8;margin-bottom:8px}
  .cval{font-size:18px;font-weight:700;color:#1e293b}
  .csub{font-size:11px;color:#94a3b8;margin-top:3px}
  .ecard{background:#fff;border:1px solid #e2e8f0;border-radius:14px;padding:16px;margin-bottom:14px}
  .eheader{display:flex;align-items:center;justify-content:space-between;margin-bottom:12px}
  .etitle{font-size:11px;letter-spacing:2px;text-transform:uppercase;color:#94a3b8}
  .ecount{font-size:11px;background:#f1f5f9;color:#64748b;padding:2px 8px;border-radius:999px}
  .erow{display:flex;align-items:center;gap:10px;padding:8px 0;border-bottom:1px solid #f8fafc}
  .erow:last-child{border-bottom:none;padding-bottom:0}
  .eico{width:28px;height:28px;border-radius:8px;display:flex;align-items:center;justify-content:center;flex-shrink:0}
  .ico-s{background:#f0fdf4} .ico-a{background:#fef2f2} .ico-i{background:#eff6ff}
  .etxt{flex:1;font-size:13px;color:#334155}
  .etime{font-size:11px;color:#94a3b8;white-space:nowrap}
  .lcard{background:#f8fafc;border:1px solid #e2e8f0;border-radius:14px;padding:16px;margin-bottom:14px}
  .lheader{display:flex;align-items:center;justify-content:space-between;margin-bottom:10px}
  .ltitle{font-size:11px;letter-spacing:2px;text-transform:uppercase;color:#94a3b8}
  .llive{display:flex;align-items:center;gap:5px;font-size:10px;color:#22c55e;font-weight:600;letter-spacing:1px;text-transform:uppercase}
  .llive-dot{width:5px;height:5px;border-radius:50%;background:#22c55e;animation:blink .8s infinite}
  .lbody{font-family:monospace;font-size:12px;line-height:1.9;color:#475569;height:120px;overflow-y:auto}
  .lbody::-webkit-scrollbar{width:3px}
  .lbody::-webkit-scrollbar-thumb{background:#e2e8f0;border-radius:2px}
  .ll{display:flex;gap:8px}
  .ts{color:#cbd5e1;min-width:50px}
  .safe-t{color:#16a34a} .alert-t{color:#dc2626} .info-t{color:#2563eb}
  .footer{display:flex;align-items:center;justify-content:space-between;padding-bottom:4px}
  .fl{font-size:11px;color:#cbd5e1;letter-spacing:1px;text-transform:uppercase}
</style>
</head>
<body>
<div class="wrap">
  <div class="topbar">
    <div class="topbar-left">
      <svg width="32" height="32" viewBox="0 0 32 32" fill="none">
        <path d="M16 2L3 8v8c0 6.8 5.4 13.2 13 15 7.6-1.8 13-8.2 13-15V8L16 2z" fill="#eff6ff" stroke="#2563eb" stroke-width="1.5"/>
        <path d="M11 16l3.5 3.5L21 13" stroke="#2563eb" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/>
      </svg>
      <div>
        <div class="title">Security Hub</div>
        <div class="subtitle">WiFi_Security</div>
      </div>
    </div>
    <div class="uptime-pill"><div class="uptime-dot"></div>Online</div>
  </div>

  <div class="status-block" id="sb">
    <div>
      <div class="slabel">System Status</div>
      <div class="sbadge safe" id="badge">
        <div class="sdot sdot-safe" id="sdot"></div>
        <span id="stext">SAFE</span>
      </div>
    </div>
    <svg class="sicon" width="52" height="52" viewBox="0 0 32 32" fill="none">
      <path d="M16 2L3 8v8c0 6.8 5.4 13.2 13 15 7.6-1.8 13-8.2 13-15V8L16 2z" fill="#2563eb"/>
    </svg>
  </div>

  <div class="grid">
    <div class="card">
      <div class="clabel">Target</div>
      <div class="cval" id="person" style="font-size:14px;color:#2563eb">None</div>
      <div class="csub">Last seen</div>
    </div>
    <div class="card">
      <div class="clabel">Confidence</div>
      <div class="cval" id="conf" style="color:#16a34a">0<span style="font-size:12px;font-weight:400;color:#94a3b8">%</span></div>
      <div class="csub">AI match</div>
    </div>
    <div class="card">
      <div class="clabel">Sensor</div>
      <div class="cval" id="sensor" style="font-size:14px;color:#16a34a">Armed</div>
      <div class="csub">PIR active</div>
    </div>
  </div>

  <div class="ecard">
    <div class="eheader">
      <div class="etitle">Recent Events</div>
      <div class="ecount" id="ecount">0 today</div>
    </div>
    <div id="events">
      <div class="erow">
        <div class="eico ico-i">
          <svg width="14" height="14" viewBox="0 0 14 14" fill="none"><circle cx="7" cy="7" r="5" stroke="#2563eb" stroke-width="1.2"/><path d="M7 6v3M7 4.5v.5" stroke="#2563eb" stroke-width="1.2" stroke-linecap="round"/></svg>
        </div>
        <div class="etxt">System armed &amp; monitoring</div>
        <div class="etime">--:--</div>
      </div>
    </div>
  </div>

  <div class="lcard">
    <div class="lheader">
      <div class="ltitle">Activity Log</div>
      <div class="llive"><div class="llive-dot"></div>Live</div>
    </div>
    <div class="lbody" id="log"></div>
  </div>

  <div class="footer">
    <div class="fl">ESP32 &middot; AI Security</div>
    <div class="fl">v2.0</div>
  </div>
</div>

<script>
  let lastLog="";
  let eventCount=0;
  const sb=document.getElementById("sb");
  const badge=document.getElementById("badge");
  const sdot=document.getElementById("sdot");
  const stext=document.getElementById("stext");

  function addEvent(msg, type){
    eventCount++;
    document.getElementById("ecount").innerText=eventCount+" today";
    const icoClass=type==="safe"?"ico-s":type==="alert"?"ico-a":"ico-i";
    const iconSvg=type==="safe"
      ?'<svg width="14" height="14" viewBox="0 0 14 14" fill="none"><path d="M2 7l3.5 3.5L12 4" stroke="#16a34a" stroke-width="1.5" stroke-linecap="round"/></svg>'
      :type==="alert"
      ?'<svg width="14" height="14" viewBox="0 0 14 14" fill="none"><path d="M7 4v4M7 10v.5" stroke="#dc2626" stroke-width="1.5" stroke-linecap="round"/></svg>'
      :'<svg width="14" height="14" viewBox="0 0 14 14" fill="none"><circle cx="7" cy="7" r="5" stroke="#2563eb" stroke-width="1.2"/><path d="M7 6v3M7 4.5v.5" stroke="#2563eb" stroke-width="1.2" stroke-linecap="round"/></svg>';
    const now=new Date();
    const t=now.getHours().toString().padStart(2,"0")+":"+now.getMinutes().toString().padStart(2,"0");
    const row=`<div class="erow"><div class="eico ${icoClass}">${iconSvg}</div><div class="etxt">${msg}</div><div class="etime">${t}</div></div>`;
    const ev=document.getElementById("events");
    ev.innerHTML=row+ev.innerHTML;
    if(ev.children.length>3) ev.removeChild(ev.lastChild);
  }

  let prevStatus="SAFE";
  setInterval(async()=>{
    try{
      let r=await fetch('/data');
      if(!r.ok)return;
      let d=await r.json();
      const isAlert=d.status!=="SAFE";
      stext.innerText=d.status;
      sb.className="status-block"+(isAlert?" alert":"");
      badge.className="sbadge "+(isAlert?"alert-b":"safe");
      sdot.className="sdot "+(isAlert?"sdot-alert":"sdot-safe");
      document.getElementById("person").innerText=d.person;
      document.getElementById("conf").innerHTML=d.conf+'<span style="font-size:12px;font-weight:400;color:#94a3b8">%</span>';
      document.getElementById("sensor").innerText=isAlert?"Triggered":"Armed";
      document.getElementById("sensor").style.color=isAlert?"#dc2626":"#16a34a";
      if(prevStatus!==d.status){
        if(d.status==="ALERT") addEvent("Motion detected — analyzing...", "alert");
        else if(prevStatus==="ALERT") addEvent("System returned to safe", "safe");
        prevStatus=d.status;
      }
      const logDiv=document.getElementById("log");
      if(lastLog!==d.log){logDiv.innerHTML=d.log;logDiv.scrollTop=logDiv.scrollHeight;lastLog=d.log;}
    }catch(e){}
  },500);
</script>
</body>
</html>
)rawliteral";

void handleRoot() { server.send(200, "text/html", page); }
void handleData() {
  String json = "{";
  json += "\"status\":\"" + systemStatus + "\",";
  json += "\"person\":\"" + lastPerson + "\",";
  json += "\"conf\":\"" + confidence + "\",";
  json += "\"log\":\"" + logs + "\"}";
  server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200);
  pinMode(PIR_PIN, INPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  u8g2.begin();
  u8g2.setBusClock(400000);
  WiFi.softAP(ssid, password);
  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.begin();
}

void loop() {
  server.handleClient();
  if (hold) {
    if (millis() - holdStart < holdTime) return;
    else hold = false;
  }
  switch (state) {
    case BOOT:
      show("AI Based", "Security System", 5000);
      logs = "<div class='ll'><span class='ts'>--:--</span><span class='info-t'>&gt; Booting sequence initiated...</span></div>";
      state = WARMUP;
      stateTime = millis();
      break;
    case WARMUP: {
      int sec = (millis() - stateTime) / 1000;
      int remain = 30 - sec;
      show("Initializing", String(remain) + " sec");
      if (sec >= 30) {
        if (digitalRead(PIR_PIN) == LOW) state = IDLE;
        else show("Waiting for", "Clear Room...");
      }
      break;
    }
    case IDLE:
      digitalWrite(GREEN_LED, HIGH);
      digitalWrite(RED_LED, LOW);
      systemStatus = "SAFE";
      show("SAFE", "Monitoring...");
      if (digitalRead(PIR_PIN) == HIGH) {
        delay(50);
        if (digitalRead(PIR_PIN) == HIGH) state = MOTION;
      }
      break;
    case MOTION:
      digitalWrite(GREEN_LED, LOW);
      digitalWrite(RED_LED, HIGH);
      systemStatus = "ALERT";
      logs += "<div class='ll'><span class='ts'>" + getTime() + "</span><span class='alert-t'>&gt; Motion detected! Analyzing...</span></div>";
      show("MOTION!", "Detected", 3000);
      buzzerBeep();
      Serial.println("MOTION");
      state = CAPTURE;
      break;
    case CAPTURE:
      show("Capturing...", "", 3000);
      if (Serial.available()) {
        String msg = Serial.readStringUntil('\n');
        msg.trim();
        if (msg == "CAPTURED") {
          show("Image", "Captured", 3000);
        } else if (msg.startsWith("KNOWN:")) {
          int f = msg.indexOf(':');
          int s = msg.indexOf(':', f + 1);
          name = msg.substring(f + 1, s);
          conf = msg.substring(s + 1);
          isKnown = true;
          lastPerson = name;
          confidence = conf;
          state = RESULT;
          stateTime = millis();
        } else if (msg == "UNKNOWN") {
          isKnown = false;
          lastPerson = "Unknown";
          confidence = "0";
          state = RESULT;
          stateTime = millis();
        }
      }
      break;
    case RESULT:
      if (isKnown) {
        systemStatus = "SAFE";
        logs += "<div class='ll'><span class='ts'>" + getTime() + "</span><span class='safe-t'>&gt; Welcome, " + name + " (" + conf + "%)</span></div>";
        show("Hi " + name, conf + "%", 4000);
      } else {
        systemStatus = "ALERT";
        logs += "<div class='ll'><span class='ts'>" + getTime() + "</span><span class='alert-t'>&gt; CRITICAL: Intruder detected!</span></div>";
        show("INTRUDER!", "Alert!", 4000);
        buzzerBeep();
      }
      state = RESTART;
      stateTime = millis();
      break;
    case RESTART: {
      int sec = (millis() - stateTime) / 1000;
      int remain = 20 - sec;
      show("Restarting", String(remain) + " sec");
      if (sec >= 20) {
        if (digitalRead(PIR_PIN) == LOW) state = IDLE;
        else show("Waiting for", "Clear Room...");
      }
      break;
    }
  }
}
