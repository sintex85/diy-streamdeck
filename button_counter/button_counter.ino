#include "LGFX_ESP32S3_RGB_ESP32-8048S043.h"
#include <Preferences.h>
#define USE_NIMBLE
#include <BleKeyboard.h>

LGFX lcd;
Preferences prefs;
BleKeyboard bleKb("StreamDeck", "BitsyTornillos", 100);

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

struct Button {
  char label[20];
  uint8_t r, g, b;
  char action[128];
  uint8_t actionType;  // 0=none, 1=url, 2=keyboard, 3=app, 4=text
  uint8_t iconSizeIdx, borderStyle;
  bool showLabel;
};

Button buttons[NUM_BUTTONS];
uint16_t* iconData[NUM_BUTTONS];
bool hasIcon[NUM_BUTTONS];
int iconPixelSize[NUM_BUTTONS];
int activeButton = -1;
bool locked = false;
uint8_t brightness = 255;
int activeSidebar = -1;

static const char* defaultLabels[] = {"1","2","3","4","5","6","7","8","9","10","11","12"};
static const uint8_t defaultColors[][3] = {
  {231,76,60},{46,204,113},{52,152,219},{241,196,15},
  {155,89,182},{230,126,34},{26,188,156},{236,64,122},
  {52,73,94},{127,140,141},{39,174,96},{41,128,185},
};

// ─── Base64 ───
int b64val(char c){if(c>='A'&&c<='Z')return c-'A';if(c>='a'&&c<='z')return c-'a'+26;if(c>='0'&&c<='9')return c-'0'+52;if(c=='+')return 62;if(c=='/')return 63;return -1;}
int base64_decode(const char*in,int inLen,uint8_t*out,int outMax){int o=0;uint32_t buf=0;int bits=0;for(int i=0;i<inLen&&o<outMax;i++){int v=b64val(in[i]);if(v<0)continue;buf=(buf<<6)|v;bits+=6;if(bits>=8){bits-=8;out[o++]=(buf>>bits)&0xFF;}}return o;}

// ─── Config ───
void saveConfig(){
  prefs.begin("deck",false);
  for(int i=0;i<NUM_BUTTONS;i++){char k[8];
    snprintf(k,8,"l%d",i);prefs.putString(k,buttons[i].label);
    snprintf(k,8,"r%d",i);prefs.putUChar(k,buttons[i].r);
    snprintf(k,8,"g%d",i);prefs.putUChar(k,buttons[i].g);
    snprintf(k,8,"b%d",i);prefs.putUChar(k,buttons[i].b);
    snprintf(k,8,"a%d",i);prefs.putString(k,buttons[i].action);
    snprintf(k,8,"y%d",i);prefs.putUChar(k,buttons[i].actionType);
    snprintf(k,8,"z%d",i);prefs.putUChar(k,buttons[i].iconSizeIdx);
    snprintf(k,8,"d%d",i);prefs.putUChar(k,buttons[i].borderStyle);
    snprintf(k,8,"t%d",i);prefs.putBool(k,buttons[i].showLabel);}
  prefs.putUChar("bri",brightness);
  prefs.end();}

void loadConfig(){
  prefs.begin("deck",true);
  for(int i=0;i<NUM_BUTTONS;i++){char k[8];
    snprintf(k,8,"l%d",i);String lbl=prefs.getString(k,defaultLabels[i]);strncpy(buttons[i].label,lbl.c_str(),19);buttons[i].label[19]='\0';
    snprintf(k,8,"r%d",i);buttons[i].r=prefs.getUChar(k,defaultColors[i][0]);
    snprintf(k,8,"g%d",i);buttons[i].g=prefs.getUChar(k,defaultColors[i][1]);
    snprintf(k,8,"b%d",i);buttons[i].b=prefs.getUChar(k,defaultColors[i][2]);
    snprintf(k,8,"a%d",i);String act=prefs.getString(k,"");strncpy(buttons[i].action,act.c_str(),127);buttons[i].action[127]='\0';
    snprintf(k,8,"y%d",i);buttons[i].actionType=prefs.getUChar(k,0);
    snprintf(k,8,"z%d",i);buttons[i].iconSizeIdx=prefs.getUChar(k,1);
    snprintf(k,8,"d%d",i);buttons[i].borderStyle=prefs.getUChar(k,0);
    snprintf(k,8,"t%d",i);buttons[i].showLabel=prefs.getBool(k,true);}
  brightness=prefs.getUChar("bri",255);
  prefs.end();}

// ─── BLE Keyboard Actions ───
void openUrlViaBLE(const char*url){
  if(!bleKb.isConnected())return;

  // Open new tab (Cmd+T on Mac, Ctrl+T on Win)
  bleKb.press(KEY_LEFT_GUI);bleKb.press('t');delay(50);bleKb.releaseAll();
  delay(500);

  // Strip protocol and www - browser auto-completes
  String u = String(url);
  u.replace("https://www.","");
  u.replace("http://www.","");
  u.replace("https://","");
  u.replace("http://","");

  // Type only safe characters
  for(int i=0; i<u.length(); i++){
    char c = u[i];
    if(c==':' || c=='/' || c=='?' || c=='#' || c=='@' || c=='&' || c=='=') continue;
    bleKb.press(c);delay(10);bleKb.release(c);delay(10);
  }
  delay(50);
  bleKb.write(KEY_BACKSPACE); // Clear Chrome's autocomplete suggestion
  delay(50);
  bleKb.write(KEY_RETURN);}

void sendKeyCombo(const char*combo){
  if(!bleKb.isConnected())return;
  String s=String(combo);s.toLowerCase();
  if(s=="vol_up"){bleKb.write(KEY_MEDIA_VOLUME_UP);return;}
  if(s=="vol_down"){bleKb.write(KEY_MEDIA_VOLUME_DOWN);return;}
  if(s=="vol_mute"){bleKb.write(KEY_MEDIA_MUTE);return;}
  if(s=="play_pause"){bleKb.write(KEY_MEDIA_PLAY_PAUSE);return;}
  if(s=="next_track"){bleKb.write(KEY_MEDIA_NEXT_TRACK);return;}
  if(s=="prev_track"){bleKb.write(KEY_MEDIA_PREVIOUS_TRACK);return;}
  bool ctrl=false,shift=false,alt=false,gui=false;int last=-1;
  while(true){int p=s.indexOf('+',last+1);if(p<0)break;String m=s.substring(last+1,p);m.trim();
    if(m=="ctrl"||m=="control")ctrl=true;else if(m=="shift")shift=true;
    else if(m=="alt"||m=="option")alt=true;else if(m=="cmd"||m=="command"||m=="win"||m=="gui")gui=true;last=p;}
  String key=s.substring(last+1);key.trim();
  if(ctrl)bleKb.press(KEY_LEFT_CTRL);if(shift)bleKb.press(KEY_LEFT_SHIFT);
  if(alt)bleKb.press(KEY_LEFT_ALT);if(gui)bleKb.press(KEY_LEFT_GUI);
  if(key.length()==1)bleKb.press(key[0]);
  else if(key=="enter"||key=="return")bleKb.press(KEY_RETURN);
  else if(key=="esc")bleKb.press(KEY_ESC);else if(key=="tab")bleKb.press(KEY_TAB);
  else if(key=="space")bleKb.press(' ');else if(key=="backspace"||key=="delete")bleKb.press(KEY_BACKSPACE);
  else if(key=="up")bleKb.press(KEY_UP_ARROW);else if(key=="down")bleKb.press(KEY_DOWN_ARROW);
  else if(key=="left")bleKb.press(KEY_LEFT_ARROW);else if(key=="right")bleKb.press(KEY_RIGHT_ARROW);
  else if(key.startsWith("f")&&key.length()<=3){int f=key.substring(1).toInt();if(f>=1&&f<=12)bleKb.press(KEY_F1+f-1);}
  delay(50);bleKb.releaseAll();}

void executeAction(int idx){
  if(idx<0||idx>=NUM_BUTTONS||buttons[idx].actionType==0||strlen(buttons[idx].action)==0)return;
  Serial.printf("[ACT] Btn %d type=%d action=%s\n",idx,buttons[idx].actionType,buttons[idx].action);

  // Always send via serial for Chrome tab to handle
  Serial.printf("BTN:%d:%d:%s\n", idx, buttons[idx].actionType, buttons[idx].action);

  switch(buttons[idx].actionType) {
    case 1: // URL - handled by Python service via serial BTN: message
    case 3: // App - handled by Python service via serial BTN: message
      // No BLE typing - the service opens URLs cleanly without interfering
      break;
    case 2: // Keyboard shortcut
      sendKeyCombo(buttons[idx].action);
      break;
    case 4: // Text
      if(bleKb.isConnected()) bleKb.print(buttons[idx].action);
      break;
  }
}

// ─── Sidebar ───
void drawGearIcon(int cx,int cy,uint16_t col){lcd.fillCircle(cx,cy,8,col);lcd.fillCircle(cx,cy,4,SB_BG);for(int a=0;a<360;a+=45){float r=a*3.14159/180;lcd.fillCircle(cx+cos(r)*11,cy+sin(r)*11,3,col);}}
void drawSunIcon(int cx,int cy,uint16_t col,bool big){int r=big?7:5;lcd.fillCircle(cx,cy,r,col);int rl=big?13:10;for(int a=0;a<360;a+=45){float rd=a*3.14159/180;lcd.drawLine(cx+cos(rd)*(r+2),cy+sin(rd)*(r+2),cx+cos(rd)*rl,cy+sin(rd)*rl,col);}}
void drawLockIcon(int cx,int cy,uint16_t col,bool isLocked){lcd.fillRoundRect(cx-8,cy-2,16,12,2,col);if(isLocked)lcd.drawArc(cx,cy-2,7,5,180,360,col);else lcd.drawArc(cx+3,cy-2,7,5,180,360,col);lcd.fillCircle(cx,cy+3,2,SB_BG);}

void drawSidebar(){
  lcd.fillRect(SB_X,0,SIDEBAR_W,480,SB_BG);
  lcd.drawFastVLine(SB_X,0,480,lcd.color565(40,40,50));
  int cx=SB_X+SIDEBAR_W/2,y=0;
  // BLE
  bool conn=bleKb.isConnected();
  lcd.fillCircle(cx,y+25,6,conn?lcd.color565(0,150,255):lcd.color565(80,80,80));
  if(conn){lcd.drawCircle(cx,y+25,8,lcd.color565(0,70,130));lcd.drawCircle(cx,y+25,10,lcd.color565(0,35,65));}
  lcd.setTextColor(lcd.color565(120,120,130));lcd.setTextDatum(middle_center);lcd.setFont(&fonts::Font0);
  lcd.drawString(conn?"BT OK":"BT...",cx,y+42);y+=SB_ITEM_H;
  lcd.drawFastHLine(SB_X+8,y,SIDEBAR_W-16,lcd.color565(40,40,50));
  // Brillo
  drawSunIcon(cx,y+25,lcd.color565(255,220,50),true);lcd.drawString("Brillo+",cx,y+46);y+=SB_ITEM_H;
  drawSunIcon(cx,y+25,lcd.color565(150,130,30),false);lcd.drawString("Brillo-",cx,y+46);y+=SB_ITEM_H;
  lcd.drawFastHLine(SB_X+8,y,SIDEBAR_W-16,lcd.color565(40,40,50));
  // Lock
  uint16_t lc=locked?lcd.color565(231,76,60):lcd.color565(120,120,140);
  drawLockIcon(cx,y+25,lc,locked);
  lcd.setTextColor(locked?lcd.color565(231,76,60):lcd.color565(120,120,130));
  lcd.drawString(locked?"Bloq":"Libre",cx,y+46);y+=SB_ITEM_H;
  lcd.drawFastHLine(SB_X+8,y,SIDEBAR_W-16,lcd.color565(40,40,50));
  // Info
  lcd.setTextColor(lcd.color565(80,80,100));
  lcd.drawString("Config:",cx,y+15);lcd.drawString("Chrome",cx,y+28);lcd.drawString("USB",cx,y+41);y+=SB_ITEM_H;
  // Brightness bar
  int bx=SB_X+10,bw=SIDEBAR_W-20;
  lcd.fillRoundRect(bx,440,bw,6,3,lcd.color565(40,40,50));
  lcd.fillRoundRect(bx,440,(brightness*bw)/255,6,3,lcd.color565(255,220,50));
  char buf[8];snprintf(buf,8,"%d%%",(brightness*100)/255);
  lcd.setTextColor(lcd.color565(100,100,110));lcd.drawString(buf,cx,458);}

int sidebarHitTest(int32_t tx,int32_t ty){if(tx<SB_X)return-1;return ty/SB_ITEM_H;}
void handleSidebarTouch(int item){switch(item){
  case 1:brightness=min(255,brightness+30);lcd.setBrightness(brightness);saveConfig();drawSidebar();break;
  case 2:brightness=max(25,brightness-30);lcd.setBrightness(brightness);saveConfig();drawSidebar();break;
  case 3:locked=!locked;drawSidebar();break;}}

// ─── Buttons ───
void getBtnRect(int idx,int&x,int&y){x=PAD+(idx%COLS)*(BTN_W+PAD);y=PAD+(idx/COLS)*(BTN_H+PAD);}
void drawBorder(int bx,int by,int w,int h,uint8_t s,uint8_t cr,uint8_t cg,uint8_t cb){if(s==0)return;uint16_t bc=lcd.color565(min(255,cr+80),min(255,cg+80),min(255,cb+80));if(s==1)lcd.drawRoundRect(bx,by,w,h,RADIUS,bc);else if(s==2)for(int i=0;i<3;i++)lcd.drawRoundRect(bx+i,by+i,w-i*2,h-i*2,RADIUS-i,bc);else if(s==3)for(int g=4;g>=0;g--){uint8_t a=60+(4-g)*45;lcd.drawRoundRect(bx-g,by-g,w+g*2,h+g*2,RADIUS+g,lcd.color565(min(255,(int)cr+a),min(255,(int)cg+a),min(255,(int)cb+a)));}}

void drawButton(int idx,bool pressed){
  int bx,by;getBtnRect(idx,bx,by);
  uint8_t r=buttons[idx].r,g=buttons[idx].g,b=buttons[idx].b;
  uint16_t color=pressed?lcd.color565(r*0.6,g*0.6,b*0.6):lcd.color565(r,g,b);
  int yOff=pressed?3:0;
  lcd.fillRect(bx-5,by-5,BTN_W+14,BTN_H+16,TFT_BLACK);
  if(!pressed)lcd.fillRoundRect(bx+3,by+3,BTN_W,BTN_H,RADIUS,lcd.color565(20,20,20));
  lcd.fillRoundRect(bx,by+yOff,BTN_W,BTN_H,RADIUS,color);
  drawBorder(bx,by+yOff,BTN_W,BTN_H,buttons[idx].borderStyle,r,g,b);
  int cx=bx+BTN_W/2;
  if(hasIcon[idx]&&iconData[idx]){
    int sz=iconPixelSize[idx],ix=cx-sz/2;
    if(buttons[idx].showLabel){lcd.pushImage(ix,by+yOff+(BTN_H/2)-sz/2-10,sz,sz,iconData[idx]);lcd.setTextColor(TFT_WHITE);lcd.setTextDatum(middle_center);lcd.setFont(&fonts::Font2);lcd.drawString(buttons[idx].label,cx,by+yOff+BTN_H-20);}
    else lcd.pushImage(ix,by+yOff+(BTN_H-sz)/2,sz,sz,iconData[idx]);
  }else if(buttons[idx].showLabel){lcd.setTextColor(TFT_WHITE);lcd.setTextDatum(middle_center);lcd.setFont(&fonts::Font4);lcd.drawString(buttons[idx].label,cx,by+yOff+BTN_H/2);}}

void drawAll(){lcd.fillScreen(TFT_BLACK);for(int i=0;i<NUM_BUTTONS;i++)drawButton(i,false);drawSidebar();}
int hitTest(int32_t tx,int32_t ty){if(tx>=SB_X)return-1;for(int i=0;i<NUM_BUTTONS;i++){int bx,by;getBtnRect(i,bx,by);if(tx>=bx&&tx<=bx+BTN_W&&ty>=by&&ty<=by+BTN_H)return i;}return-1;}

// ─── Serial Config (Web Serial from Chrome) ───
String serialBuf="";

void processSerialCmd(const String&cmd){
  if(cmd=="GETALL"){
    for(int i=0;i<NUM_BUTTONS;i++)
      Serial.printf("CFG:%d:%s:%d,%d,%d:%d:%d,%d,%d:%d:%s\n",i,buttons[i].label,buttons[i].r,buttons[i].g,buttons[i].b,hasIcon[i]?1:0,buttons[i].iconSizeIdx,buttons[i].borderStyle,buttons[i].showLabel?1:0,buttons[i].actionType,buttons[i].action);
    Serial.println("END");return;}
  if(cmd.startsWith("SET:")){
    int p1=4,p2=cmd.indexOf(':',p1);if(p2<0)return;
    int p3=cmd.indexOf(':',p2+1);if(p3<0)return;
    int idx=cmd.substring(p1,p2).toInt();if(idx<0||idx>=NUM_BUTTONS)return;
    strncpy(buttons[idx].label,cmd.substring(p2+1,p3).c_str(),19);buttons[idx].label[19]='\0';
    String rest=cmd.substring(p3+1);int c1=rest.indexOf(','),c2=rest.indexOf(',',c1+1);if(c1<0||c2<0)return;
    int nc=rest.indexOf(':',c2+1);String cp=(nc>=0)?rest.substring(0,nc):rest;
    c1=cp.indexOf(',');c2=cp.indexOf(',',c1+1);
    buttons[idx].r=cp.substring(0,c1).toInt();buttons[idx].g=cp.substring(c1+1,c2).toInt();buttons[idx].b=cp.substring(c2+1).toInt();
    if(nc>=0){String ss=rest.substring(nc+1);int s1=ss.indexOf(','),s2=ss.indexOf(',',s1+1);
      if(s1>=0&&s2>=0){buttons[idx].iconSizeIdx=constrain(ss.substring(0,s1).toInt(),0,3);buttons[idx].borderStyle=constrain(ss.substring(s1+1,s2).toInt(),0,3);buttons[idx].showLabel=ss.substring(s2+1).toInt()!=0;}}
    saveConfig();drawButton(idx,false);Serial.println("OK");return;}
  if(cmd.startsWith("ACT:")){
    int p1=4,p2=cmd.indexOf(':',p1);if(p2<0)return;
    int p3=cmd.indexOf(':',p2+1);if(p3<0)return;
    int idx=cmd.substring(p1,p2).toInt();if(idx<0||idx>=NUM_BUTTONS)return;
    buttons[idx].actionType=cmd.substring(p2+1,p3).toInt();
    strncpy(buttons[idx].action,cmd.substring(p3+1).c_str(),127);buttons[idx].action[127]='\0';
    saveConfig();Serial.println("OK");return;}
  if(cmd=="STATUS"){Serial.printf("BLE:%s\n",bleKb.isConnected()?"CONNECTED":"WAITING");Serial.println("END");return;}
  if(cmd=="TESTBLE"){if(bleKb.isConnected()){bleKb.print("StreamDeck OK! ");Serial.println("SENT");}else Serial.println("NOBLE");return;}
  if(cmd=="PING"){Serial.println("PONG");return;}}

void handleSerial(){while(Serial.available()){char c=Serial.read();if(c=='\n'){serialBuf.trim();if(serialBuf.length()>0)processSerialCmd(serialBuf);serialBuf="";}else if(c!='\r')serialBuf+=c;}}

// ─── Main ───
void setup(){
  Serial.begin(115200);delay(500);
  Serial.println("[BOOT] Starting...");
  lcd.init();lcd.setRotation(0);
  for(int i=0;i<NUM_BUTTONS;i++){iconData[i]=NULL;hasIcon[i]=false;iconPixelSize[i]=32;buttons[i].action[0]='\0';buttons[i].actionType=0;}
  loadConfig();lcd.setBrightness(brightness);drawAll();
  Serial.println("[BOOT] Starting BLE Keyboard (NimBLE)...");
  bleKb.begin();
  Serial.println("[BOOT] Ready! Pair 'StreamDeck' via Bluetooth");}

void loop(){
  handleSerial();
  static bool lastBle=false;bool curBle=bleKb.isConnected();
  if(curBle!=lastBle){lastBle=curBle;drawSidebar();Serial.printf("[BLE] %s\n",curBle?"Connected":"Disconnected");}
  int32_t tx,ty;bool touched=lcd.getTouch(&tx,&ty);
  if(touched&&activeButton==-1&&activeSidebar==-1){
    int sb=sidebarHitTest(tx,ty);
    if(sb>=0){activeSidebar=sb;handleSidebarTouch(sb);}
    else if(!locked){int hit=hitTest(tx,ty);if(hit>=0){activeButton=hit;drawButton(hit,true);executeAction(hit);}}}
  if(!touched){if(activeButton>=0){drawButton(activeButton,false);activeButton=-1;}activeSidebar=-1;}
  delay(10);}
