#include "LGFX_ESP32S3_RGB_ESP32-8048S043.h"
#include "embedded_files.h"
#include <Preferences.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>

LGFX lcd;
Preferences prefs;
WebServer webServer(80);
DNSServer dnsServer;

// ─── Layout ───
static const int SIDEBAR_W = 56;
static const int GRID_W = 800 - SIDEBAR_W;
static const int COLS = 4;
static const int ROWS = 3;
static const int NUM_BUTTONS = COLS * ROWS;
static const int PAD = 10;
static const int BTN_W = (GRID_W - (COLS + 1) * PAD) / COLS;
static const int BTN_H = (480 - (ROWS + 1) * PAD) / ROWS;
static const int RADIUS = 12;
static const int SB_X = GRID_W;
static const int SB_ITEM_H = 70;
static const uint16_t SB_BG = 0x1082;

static const int ICON_SIZES[] = {24, 32, 48, 64};
enum BorderStyle { BORDER_NONE=0, BORDER_THIN=1, BORDER_THICK=2, BORDER_GLOW=3 };

struct Button {
  char label[20];
  uint8_t r, g, b;
  uint8_t iconSizeIdx, borderStyle;
  bool showLabel;
};

Button buttons[NUM_BUTTONS];
uint16_t* iconData[NUM_BUTTONS];
bool hasIcon[NUM_BUTTONS];
int iconPixelSize[NUM_BUTTONS];
int activeButton = -1;

// State
bool pcConnected = false;
unsigned long lastSerialTime = 0;
bool locked = false;
uint8_t brightness = 255;
int activeSidebar = -1;
bool setupShown = false;
bool everConnected = false;
bool wifiStarted = false;

// Defaults
static const char* defaultLabels[NUM_BUTTONS] = {"1","2","3","4","5","6","7","8","9","10","11","12"};
static const uint8_t defaultColors[NUM_BUTTONS][3] = {
  {231,76,60},{46,204,113},{52,152,219},{241,196,15},
  {155,89,182},{230,126,34},{26,188,156},{236,64,122},
  {52,73,94},{127,140,141},{39,174,96},{41,128,185},
};

// ─── Base64 ───
int b64val(char c) {
  if(c>='A'&&c<='Z')return c-'A'; if(c>='a'&&c<='z')return c-'a'+26;
  if(c>='0'&&c<='9')return c-'0'+52; if(c=='+')return 62; if(c=='/')return 63; return -1;
}
int base64_decode(const char* in, int inLen, uint8_t* out, int outMax) {
  int outLen=0; uint32_t buf=0; int bits=0;
  for(int i=0;i<inLen&&outLen<outMax;i++){
    int v=b64val(in[i]); if(v<0)continue;
    buf=(buf<<6)|v; bits+=6;
    if(bits>=8){bits-=8; out[outLen++]=(buf>>bits)&0xFF;}
  }
  return outLen;
}

// ─── Config ───
void saveConfig() {
  prefs.begin("deck",false);
  for(int i=0;i<NUM_BUTTONS;i++){
    char k[8];
    snprintf(k,8,"l%d",i); prefs.putString(k,buttons[i].label);
    snprintf(k,8,"r%d",i); prefs.putUChar(k,buttons[i].r);
    snprintf(k,8,"g%d",i); prefs.putUChar(k,buttons[i].g);
    snprintf(k,8,"b%d",i); prefs.putUChar(k,buttons[i].b);
    snprintf(k,8,"z%d",i); prefs.putUChar(k,buttons[i].iconSizeIdx);
    snprintf(k,8,"d%d",i); prefs.putUChar(k,buttons[i].borderStyle);
    snprintf(k,8,"t%d",i); prefs.putBool(k,buttons[i].showLabel);
  }
  prefs.putUChar("bri",brightness);
  prefs.end();
}
void loadConfig() {
  prefs.begin("deck",true);
  for(int i=0;i<NUM_BUTTONS;i++){
    char k[8];
    snprintf(k,8,"l%d",i); String lbl=prefs.getString(k,defaultLabels[i]);
    strncpy(buttons[i].label,lbl.c_str(),19); buttons[i].label[19]='\0';
    snprintf(k,8,"r%d",i); buttons[i].r=prefs.getUChar(k,defaultColors[i][0]);
    snprintf(k,8,"g%d",i); buttons[i].g=prefs.getUChar(k,defaultColors[i][1]);
    snprintf(k,8,"b%d",i); buttons[i].b=prefs.getUChar(k,defaultColors[i][2]);
    snprintf(k,8,"z%d",i); buttons[i].iconSizeIdx=prefs.getUChar(k,1);
    snprintf(k,8,"d%d",i); buttons[i].borderStyle=prefs.getUChar(k,0);
    snprintf(k,8,"t%d",i); buttons[i].showLabel=prefs.getBool(k,true);
  }
  brightness=prefs.getUChar("bri",255);
  prefs.end();
}

// ─── WiFi Setup Portal ───
static const char SETUP_HTML[] PROGMEM = R"rawhtml(<!DOCTYPE html>
<html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>DIY Stream Deck - Setup</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:-apple-system,sans-serif;background:#1a1a2e;color:#eee;min-height:100vh;display:flex;flex-direction:column;align-items:center;padding:30px 16px}
h1{font-size:24px;margin-bottom:8px;background:linear-gradient(135deg,#667eea,#764ba2);-webkit-background-clip:text;-webkit-text-fill-color:transparent}
.sub{color:#888;margin-bottom:30px;font-size:14px}
.card{background:#16213e;border-radius:16px;padding:24px;max-width:500px;width:100%;margin-bottom:16px}
.card h2{font-size:18px;margin-bottom:16px}
.step{display:flex;gap:12px;margin-bottom:16px;align-items:flex-start}
.num{background:#667eea;color:#fff;min-width:28px;height:28px;border-radius:50%;display:flex;align-items:center;justify-content:center;font-weight:700;font-size:14px}
.step p{font-size:14px;color:#ccc;line-height:1.5}
.btn{display:block;width:100%;padding:14px;border-radius:10px;border:none;font-size:15px;font-weight:600;cursor:pointer;text-align:center;text-decoration:none;margin-bottom:10px;transition:background .2s}
.btn-primary{background:#667eea;color:#fff}.btn-primary:hover{background:#5a6fd6}
.btn-secondary{background:#2a2a3e;color:#aaa}.btn-secondary:hover{background:#333350}
.os{display:flex;gap:8px;margin-bottom:16px}
.os label{flex:1;text-align:center;padding:10px;border-radius:8px;background:#0f1a30;border:2px solid #333;cursor:pointer;font-size:14px;transition:all .2s}
.os input{display:none}
.os label:has(input:checked){border-color:#667eea;background:#1a2540;color:#667eea}
.win,.mac{display:none}.win.active,.mac.active{display:block}
.footer{color:#555;font-size:12px;margin-top:20px}
</style></head><body>
<h1>DIY Stream Deck</h1>
<p class="sub">Instalacion</p>
<div class="card">
<h2>1. Descarga los archivos</h2>
<div class="os"><label><input type="radio" name="os" value="win" checked onchange="toggleOS()"><span>Windows</span></label>
<label><input type="radio" name="os" value="mac" onchange="toggleOS()"><span>Mac</span></label></div>
<div class="win active" id="winFiles">
<a class="btn btn-primary" href="/dl/streamdeck_app.py" download>streamdeck_app.py</a>
<a class="btn btn-secondary" href="/dl/Instalar.bat" download>Instalar.bat</a>
<a class="btn btn-secondary" href="/dl/StreamDeck.bat" download="Stream Deck.bat">Stream Deck.bat</a>
</div>
<div class="mac" id="macFiles">
<a class="btn btn-primary" href="/dl/streamdeck_app.py" download>streamdeck_app.py</a>
<a class="btn btn-secondary" href="/dl/Instalar.command" download>Instalar.command</a>
<a class="btn btn-secondary" href="/dl/StreamDeck.command" download="Stream Deck.command">Stream Deck.command</a>
</div>
<p style="font-size:12px;color:#666;margin-top:8px">Pon los 3 archivos en la misma carpeta</p>
</div>
<div class="card">
<h2>2. Instala</h2>
<div class="step"><div class="num">1</div><p>Haz doble-click en <b>Instalar</b> (solo una vez)</p></div>
<div class="step"><div class="num">2</div><p>Di <b>si</b> al auto-arranque</p></div>
<div class="step"><div class="num">3</div><p>Desconecta del WiFi "StreamDeck" y conecta el USB al PC</p></div>
<div class="step"><div class="num">4</div><p>Se abrira la app automaticamente!</p></div>
</div>
<p class="footer">DIY Stream Deck &bull; ESP32-8048S043</p>
<script>
function toggleOS(){const w=document.getElementById('winFiles'),m=document.getElementById('macFiles');
if(document.querySelector('input[name=os]:checked').value==='win'){w.classList.add('active');m.classList.remove('active')}
else{m.classList.add('active');w.classList.remove('active')}}
</script></body></html>)rawhtml";

void sendGzipFile(const uint8_t* data, size_t len, const char* filename, const char* mimeType) {
  webServer.sendHeader("Content-Disposition", String("attachment; filename=\"") + filename + "\"");
  webServer.sendHeader("Content-Encoding", "gzip");
  webServer.send_P(200, mimeType, (const char*)data, len);
}

void startWiFiPortal() {
  if (wifiStarted) return;

  WiFi.mode(WIFI_AP);
  WiFi.softAP("StreamDeck", "");  // Open network
  delay(100);

  // DNS captive portal - redirect all domains to us
  dnsServer.start(53, "*", WiFi.softAPIP());

  // Routes
  webServer.on("/", HTTP_GET, []() {
    webServer.send_P(200, "text/html", SETUP_HTML);
  });

  // File downloads (served gzip, browser decompresses)
  webServer.on("/dl/streamdeck_app.py", HTTP_GET, []() {
    sendGzipFile(FILE_streamdeck_app_py, FILE_streamdeck_app_py_LEN, "streamdeck_app.py", "application/octet-stream");
  });
  webServer.on("/dl/Instalar.bat", HTTP_GET, []() {
    sendGzipFile(FILE_Instalar_bat, FILE_Instalar_bat_LEN, "Instalar.bat", "application/octet-stream");
  });
  webServer.on("/dl/Instalar.command", HTTP_GET, []() {
    sendGzipFile(FILE_Instalar_command, FILE_Instalar_command_LEN, "Instalar.command", "application/octet-stream");
  });
  webServer.on("/dl/StreamDeck.bat", HTTP_GET, []() {
    sendGzipFile(FILE_Stream_Deck_bat, FILE_Stream_Deck_bat_LEN, "Stream Deck.bat", "application/octet-stream");
  });
  webServer.on("/dl/StreamDeck.command", HTTP_GET, []() {
    sendGzipFile(FILE_Stream_Deck_command, FILE_Stream_Deck_command_LEN, "Stream Deck.command", "application/octet-stream");
  });

  // Captive portal: redirect any unknown request to setup page
  webServer.onNotFound([]() {
    webServer.sendHeader("Location", "http://192.168.4.1/");
    webServer.send(302, "text/plain", "Redirect");
  });

  webServer.begin();
  wifiStarted = true;
}

void stopWiFiPortal() {
  if (!wifiStarted) return;
  webServer.stop();
  dnsServer.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  wifiStarted = false;
}

// ─── Sidebar drawing ───
void drawGearIcon(int cx, int cy, uint16_t col) {
  lcd.fillCircle(cx,cy,8,col); lcd.fillCircle(cx,cy,4,SB_BG);
  for(int a=0;a<360;a+=45){float r=a*3.14159/180; lcd.fillCircle(cx+cos(r)*11,cy+sin(r)*11,3,col);}
}
void drawSunIcon(int cx, int cy, uint16_t col, bool big) {
  int r=big?7:5; lcd.fillCircle(cx,cy,r,col);
  int rl=big?13:10;
  for(int a=0;a<360;a+=45){float rd=a*3.14159/180; lcd.drawLine(cx+cos(rd)*(r+2),cy+sin(rd)*(r+2),cx+cos(rd)*rl,cy+sin(rd)*rl,col);}
}
void drawLockIcon(int cx, int cy, uint16_t col, bool isLocked) {
  lcd.fillRoundRect(cx-8,cy-2,16,12,2,col);
  if(isLocked) lcd.drawArc(cx,cy-2,7,5,180,360,col);
  else lcd.drawArc(cx+3,cy-2,7,5,180,360,col);
  lcd.fillCircle(cx,cy+3,2,SB_BG);
}

void drawSidebar() {
  lcd.fillRect(SB_X,0,SIDEBAR_W,480,SB_BG);
  lcd.drawFastVLine(SB_X,0,480,lcd.color565(40,40,50));
  int cx=SB_X+SIDEBAR_W/2, y=0;

  // LED
  uint16_t ledC=pcConnected?lcd.color565(0,220,80):lcd.color565(80,80,80);
  lcd.fillCircle(cx,y+25,6,ledC);
  if(pcConnected){lcd.drawCircle(cx,y+25,8,lcd.color565(0,100,40));lcd.drawCircle(cx,y+25,10,lcd.color565(0,50,20));}
  lcd.setTextColor(lcd.color565(120,120,130)); lcd.setTextDatum(middle_center); lcd.setFont(&fonts::Font0);
  lcd.drawString(pcConnected?"ON":"OFF",cx,y+42); y+=SB_ITEM_H;
  lcd.drawFastHLine(SB_X+8,y,SIDEBAR_W-16,lcd.color565(40,40,50));

  // Gear
  drawGearIcon(cx,y+28,lcd.color565(180,180,200));
  lcd.setTextColor(lcd.color565(120,120,130)); lcd.drawString("Config",cx,y+48); y+=SB_ITEM_H;
  lcd.drawFastHLine(SB_X+8,y,SIDEBAR_W-16,lcd.color565(40,40,50));

  // Brightness +/-
  drawSunIcon(cx,y+25,lcd.color565(255,220,50),true); lcd.drawString("Brillo+",cx,y+46); y+=SB_ITEM_H;
  drawSunIcon(cx,y+25,lcd.color565(150,130,30),false); lcd.drawString("Brillo-",cx,y+46); y+=SB_ITEM_H;
  lcd.drawFastHLine(SB_X+8,y,SIDEBAR_W-16,lcd.color565(40,40,50));

  // Lock
  uint16_t lc=locked?lcd.color565(231,76,60):lcd.color565(120,120,140);
  drawLockIcon(cx,y+25,lc,locked);
  lcd.setTextColor(locked?lcd.color565(231,76,60):lcd.color565(120,120,130));
  lcd.drawString(locked?"Bloq":"Libre",cx,y+46); y+=SB_ITEM_H;
  lcd.drawFastHLine(SB_X+8,y,SIDEBAR_W-16,lcd.color565(40,40,50));

  // WiFi indicator
  if(wifiStarted){
    lcd.fillCircle(cx,y+25,6,lcd.color565(52,152,219));
    lcd.drawCircle(cx,y+25,8,lcd.color565(30,90,130));
    lcd.setTextColor(lcd.color565(52,152,219));
    lcd.drawString("WiFi",cx,y+42);
  }
  y+=SB_ITEM_H;

  // Brightness bar
  int bx=SB_X+10, bw=SIDEBAR_W-20;
  lcd.fillRoundRect(bx,440,bw,6,3,lcd.color565(40,40,50));
  lcd.fillRoundRect(bx,440,(brightness*bw)/255,6,3,lcd.color565(255,220,50));
  char buf[8]; snprintf(buf,8,"%d%%",(brightness*100)/255);
  lcd.setTextColor(lcd.color565(100,100,110)); lcd.drawString(buf,cx,458);
}

int sidebarHitTest(int32_t tx, int32_t ty) {
  if(tx<SB_X)return -1; return ty/SB_ITEM_H;
}

void handleSidebarTouch(int item) {
  switch(item){
    case 1: Serial.println("OPENCONFIG"); break;
    case 2: brightness=min(255,brightness+30); lcd.setBrightness(brightness); saveConfig(); drawSidebar(); break;
    case 3: brightness=max(25,brightness-30); lcd.setBrightness(brightness); saveConfig(); drawSidebar(); break;
    case 4: locked=!locked; drawSidebar(); break;
  }
}

// ─── Button drawing ───
void getBtnRect(int idx, int &x, int &y) {
  x=PAD+(idx%COLS)*(BTN_W+PAD); y=PAD+(idx/COLS)*(BTN_H+PAD);
}
void drawBorder(int bx, int by, int w, int h, uint8_t style, uint8_t cr, uint8_t cg, uint8_t cb) {
  if(style==BORDER_NONE)return;
  uint16_t bc=lcd.color565(min(255,cr+80),min(255,cg+80),min(255,cb+80));
  if(style==BORDER_THIN) lcd.drawRoundRect(bx,by,w,h,RADIUS,bc);
  else if(style==BORDER_THICK) for(int i=0;i<3;i++) lcd.drawRoundRect(bx+i,by+i,w-i*2,h-i*2,RADIUS-i,bc);
  else if(style==BORDER_GLOW) for(int g=4;g>=0;g--){
    uint8_t a=60+(4-g)*45;
    lcd.drawRoundRect(bx-g,by-g,w+g*2,h+g*2,RADIUS+g,lcd.color565(min(255,(int)cr+a),min(255,(int)cg+a),min(255,(int)cb+a)));
  }
}
void drawButton(int idx, bool pressed) {
  int bx,by; getBtnRect(idx,bx,by);
  uint8_t r=buttons[idx].r, g=buttons[idx].g, b=buttons[idx].b;
  uint16_t color=pressed?lcd.color565(r*0.6,g*0.6,b*0.6):lcd.color565(r,g,b);
  int yOff=pressed?3:0;
  lcd.fillRect(bx-5,by-5,BTN_W+14,BTN_H+16,TFT_BLACK);
  if(!pressed) lcd.fillRoundRect(bx+3,by+3,BTN_W,BTN_H,RADIUS,lcd.color565(20,20,20));
  lcd.fillRoundRect(bx,by+yOff,BTN_W,BTN_H,RADIUS,color);
  drawBorder(bx,by+yOff,BTN_W,BTN_H,buttons[idx].borderStyle,r,g,b);
  int cx=bx+BTN_W/2;
  if(hasIcon[idx]&&iconData[idx]){
    int sz=iconPixelSize[idx], ix=cx-sz/2;
    if(buttons[idx].showLabel){
      lcd.pushImage(ix,by+yOff+(BTN_H/2)-sz/2-10,sz,sz,iconData[idx]);
      lcd.setTextColor(TFT_WHITE); lcd.setTextDatum(middle_center); lcd.setFont(&fonts::Font2);
      lcd.drawString(buttons[idx].label,cx,by+yOff+BTN_H-20);
    } else lcd.pushImage(ix,by+yOff+(BTN_H-sz)/2,sz,sz,iconData[idx]);
  } else if(buttons[idx].showLabel){
    lcd.setTextColor(TFT_WHITE); lcd.setTextDatum(middle_center); lcd.setFont(&fonts::Font4);
    lcd.drawString(buttons[idx].label,cx,by+yOff+BTN_H/2);
  }
}
void drawAll() { lcd.fillScreen(TFT_BLACK); for(int i=0;i<NUM_BUTTONS;i++) drawButton(i,false); drawSidebar(); }
int hitTest(int32_t tx, int32_t ty) {
  if(tx>=SB_X)return -1;
  for(int i=0;i<NUM_BUTTONS;i++){int bx,by; getBtnRect(i,bx,by); if(tx>=bx&&tx<=bx+BTN_W&&ty>=by&&ty<=by+BTN_H)return i;}
  return -1;
}

// ─── Setup screen ───
void drawSetupScreen() {
  lcd.fillScreen(lcd.color565(15,15,30));
  lcd.setTextDatum(middle_center);

  lcd.setTextColor(lcd.color565(100,126,234)); lcd.setFont(&fonts::Font4);
  lcd.drawString("DIY Stream Deck",400,40);

  lcd.setTextColor(lcd.color565(180,180,200)); lcd.setFont(&fonts::Font2);
  lcd.drawString("Primera configuracion",400,75);

  lcd.fillRoundRect(60,100,680,300,16,lcd.color565(25,30,50));
  lcd.drawRoundRect(60,100,680,300,16,lcd.color565(60,60,80));

  // WiFi icon
  lcd.fillCircle(400,150,20,lcd.color565(52,152,219));
  lcd.setTextColor(TFT_WHITE); lcd.setFont(&fonts::Font4);
  lcd.drawString("WiFi",400,150);

  lcd.setTextColor(lcd.color565(100,200,255)); lcd.setFont(&fonts::Font4);
  lcd.drawString("1. Conectate al WiFi:",400,200);

  // SSID big
  lcd.setTextColor(TFT_WHITE); lcd.setFont(&fonts::Font4);
  lcd.drawString("\"StreamDeck\"",400,235);

  lcd.setTextColor(lcd.color565(150,150,170)); lcd.setFont(&fonts::Font2);
  lcd.drawString("(sin contrasena)",400,260);

  lcd.setTextColor(lcd.color565(100,200,255)); lcd.setFont(&fonts::Font4);
  lcd.drawString("2. Abre el navegador",400,300);

  lcd.setTextColor(lcd.color565(220,220,230)); lcd.setFont(&fonts::Font2);
  lcd.drawString("Se abrira automaticamente la pagina de descarga",400,330);

  lcd.setTextColor(lcd.color565(46,204,113)); lcd.setFont(&fonts::Font4);
  lcd.drawString("3. Descarga e instala",400,370);

  lcd.setTextColor(lcd.color565(80,80,100)); lcd.setFont(&fonts::Font2);
  lcd.drawString("Esperando conexion...",400,430);

  setupShown = true;
}

// ─── Serial ───
String serialBuffer = "";
void processSerialCommand(String cmd) {
  cmd.trim(); lastSerialTime=millis();
  if(cmd=="PING"){if(!pcConnected){pcConnected=true; drawSidebar();} Serial.println("PONG"); return;}
  if(cmd.startsWith("SET:")){
    int p1=4,p2=cmd.indexOf(':',p1); if(p2<0)return;
    int p3=cmd.indexOf(':',p2+1); if(p3<0)return;
    int idx=cmd.substring(p1,p2).toInt(); if(idx<0||idx>=NUM_BUTTONS)return;
    strncpy(buttons[idx].label,cmd.substring(p2+1,p3).c_str(),19); buttons[idx].label[19]='\0';
    String rest=cmd.substring(p3+1);
    int c1=rest.indexOf(','),c2=rest.indexOf(',',c1+1); if(c1<0||c2<0)return;
    int nc=rest.indexOf(':',c2+1);
    String cp=(nc>=0)?rest.substring(0,nc):rest;
    c1=cp.indexOf(','); c2=cp.indexOf(',',c1+1);
    buttons[idx].r=cp.substring(0,c1).toInt(); buttons[idx].g=cp.substring(c1+1,c2).toInt(); buttons[idx].b=cp.substring(c2+1).toInt();
    if(nc>=0){String ss=rest.substring(nc+1); int s1=ss.indexOf(','),s2=ss.indexOf(',',s1+1);
      if(s1>=0&&s2>=0){buttons[idx].iconSizeIdx=constrain(ss.substring(0,s1).toInt(),0,3);
        buttons[idx].borderStyle=constrain(ss.substring(s1+1,s2).toInt(),0,3);
        buttons[idx].showLabel=ss.substring(s2+1).toInt()!=0;}}
    saveConfig(); drawButton(idx,false); Serial.println("OK");
  }
  else if(cmd.startsWith("ICON:")){
    int p1=5,p2=cmd.indexOf(':',p1); if(p2<0)return;
    int p3=cmd.indexOf(':',p2+1); if(p3<0)return;
    int idx=cmd.substring(p1,p2).toInt(); if(idx<0||idx>=NUM_BUTTONS)return;
    int ps=cmd.substring(p2+1,p3).toInt(); if(ps<16||ps>64)return;
    int eb=ps*ps*2;
    if(iconData[idx])free(iconData[idx]);
    iconData[idx]=(uint16_t*)ps_malloc(eb); if(!iconData[idx]){Serial.println("ERR:NOMEM");return;}
    int d=base64_decode(cmd.substring(p3+1).c_str(),cmd.length()-p3-1,(uint8_t*)iconData[idx],eb);
    if(d>=eb){hasIcon[idx]=true;iconPixelSize[idx]=ps;drawButton(idx,false);Serial.println("OK");}
    else{hasIcon[idx]=false;Serial.printf("ERR:DECODE:%d/%d\n",d,eb);}
  }
  else if(cmd.startsWith("NOICON:")){int idx=cmd.substring(7).toInt();
    if(idx>=0&&idx<NUM_BUTTONS){hasIcon[idx]=false;drawButton(idx,false);Serial.println("OK");}}
  else if(cmd=="GETALL"){for(int i=0;i<NUM_BUTTONS;i++)
    Serial.printf("CFG:%d:%s:%d,%d,%d:%d:%d,%d,%d\n",i,buttons[i].label,buttons[i].r,buttons[i].g,buttons[i].b,
      hasIcon[i]?1:0,buttons[i].iconSizeIdx,buttons[i].borderStyle,buttons[i].showLabel?1:0);
    Serial.println("END");}
  else if(cmd=="RESETALL"){for(int i=0;i<NUM_BUTTONS;i++){
    strncpy(buttons[i].label,defaultLabels[i],19);
    buttons[i].r=defaultColors[i][0];buttons[i].g=defaultColors[i][1];buttons[i].b=defaultColors[i][2];
    buttons[i].iconSizeIdx=1;buttons[i].borderStyle=0;buttons[i].showLabel=true;hasIcon[i]=false;}
    saveConfig();drawAll();Serial.println("OK");}
}
void handleSerial(){while(Serial.available()){char c=Serial.read();
  if(c=='\n'){processSerialCommand(serialBuffer);serialBuffer="";}
  else if(c!='\r')serialBuffer+=c;}}

// ─── Main ───
void setup() {
  Serial.begin(115200); Serial.setRxBufferSize(16384);
  lcd.init(); lcd.setRotation(0);
  for(int i=0;i<NUM_BUTTONS;i++){iconData[i]=NULL;hasIcon[i]=false;iconPixelSize[i]=32;}
  loadConfig(); lcd.setBrightness(brightness); drawAll();
}

void loop() {
  handleSerial();

  // Connection timeout
  if(pcConnected && millis()-lastSerialTime>5000){pcConnected=false; drawSidebar();}

  // After 8s with no PC: show setup + start WiFi
  if(!everConnected && !pcConnected && millis()>8000 && !setupShown){
    startWiFiPortal();
    drawSetupScreen();
  }

  // Handle WiFi clients
  if(wifiStarted){dnsServer.processNextRequest(); webServer.handleClient();}

  // When PC connects, switch to deck view and stop WiFi
  if(pcConnected && !everConnected){
    everConnected=true;
    if(wifiStarted) stopWiFiPortal();
    if(setupShown){setupShown=false; drawAll();}
  }
  if(pcConnected && setupShown){setupShown=false; drawAll();}

  // Touch
  int32_t tx,ty; bool touched=lcd.getTouch(&tx,&ty);
  if(touched && activeButton==-1 && activeSidebar==-1 && !setupShown){
    int sb=sidebarHitTest(tx,ty);
    if(sb>=0){activeSidebar=sb; handleSidebarTouch(sb);}
    else if(!locked){int hit=hitTest(tx,ty);
      if(hit>=0){activeButton=hit; drawButton(hit,true); Serial.printf("BTN:%d\n",hit);}}
  }
  if(!touched){if(activeButton>=0){drawButton(activeButton,false);activeButton=-1;} activeSidebar=-1;}
  delay(20);
}
