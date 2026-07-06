// ============================================================================
//  DIY Stream Deck  ·  ESP32-32E 4" (ST7796 480x320 + touch XPT2046)
//  by Bits y Tornillos
//  Mismo protocolo serie que la version S3 -> la web docs/index.html vale igual.
//  ESP32 clasico (sin PSRAM): iconos en RAM normal (malloc).
// ============================================================================
#include "LGFX_ESP32_ST7796.h"
#include <Preferences.h>
#define USE_NIMBLE
#include <BleKeyboard.h>

LGFX lcd;
Preferences prefs;
BleKeyboard bleKb("StreamDeck", "BitsyTornillos", 100);

// ─── Layout (pantalla 480x320 landscape) ───
static const int SCREEN_W = 480;
static const int SCREEN_H = 320;
static const int SIDEBAR_W = 52;
static const int GRID_W = SCREEN_W - SIDEBAR_W;   // 428
static const int COLS = 4;
static const int ROWS = 3;
static const int PER_PAGE = COLS * ROWS;          // 12 botones por pagina
static const int PAGES = 3;                       // numero de paginas
static const int NUM_BUTTONS = PER_PAGE * PAGES;  // 36 botones en total
static const int PAD = 8;
static const int BTN_W = (GRID_W - (COLS + 1) * PAD) / COLS;   // 97
static const int BTN_H = (SCREEN_H - (ROWS + 1) * PAD) / ROWS; // 96
static const int RADIUS = 10;
static const int SB_X = GRID_W;                   // 428
static const int SB_ITEM_H = 45;                  // 7 items en 320px
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
bool infoShown = false;
uint8_t brightness = 255;
int activeSidebar = -1;
int curPage = 0;        // pagina visible (0..PAGES-1)
uint16_t touchCal[8];   // calibracion tactil (affine LovyanGFX)

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
  for(int i=0;i<NUM_BUTTONS;i++){char k[8];char dl[6];snprintf(dl,6,"%d",i+1);
    snprintf(k,8,"l%d",i);String lbl=prefs.getString(k,dl);strncpy(buttons[i].label,lbl.c_str(),19);buttons[i].label[19]='\0';
    snprintf(k,8,"r%d",i);buttons[i].r=prefs.getUChar(k,defaultColors[i%12][0]);
    snprintf(k,8,"g%d",i);buttons[i].g=prefs.getUChar(k,defaultColors[i%12][1]);
    snprintf(k,8,"b%d",i);buttons[i].b=prefs.getUChar(k,defaultColors[i%12][2]);
    snprintf(k,8,"a%d",i);String act=prefs.getString(k,"");strncpy(buttons[i].action,act.c_str(),127);buttons[i].action[127]='\0';
    snprintf(k,8,"y%d",i);buttons[i].actionType=prefs.getUChar(k,0);
    snprintf(k,8,"z%d",i);buttons[i].iconSizeIdx=prefs.getUChar(k,1);
    snprintf(k,8,"d%d",i);buttons[i].borderStyle=prefs.getUChar(k,0);
    snprintf(k,8,"t%d",i);buttons[i].showLabel=prefs.getBool(k,true);}
  brightness=prefs.getUChar("bri",255);
  prefs.end();}

// ─── Calibracion tactil ───
void saveCalib(){prefs.begin("deck",false);prefs.putBytes("cal",touchCal,sizeof(touchCal));prefs.end();}
bool loadCalib(){prefs.begin("deck",true);size_t n=prefs.getBytesLength("cal");bool ok=false;
  if(n==sizeof(touchCal)){prefs.getBytes("cal",touchCal,sizeof(touchCal));ok=true;}
  prefs.end();return ok;}

void runCalibration(){
  lcd.fillScreen(TFT_BLACK);
  lcd.setTextColor(TFT_WHITE);lcd.setTextDatum(middle_center);lcd.setFont(&fonts::Font2);
  lcd.drawString("CALIBRACION TACTIL",lcd.width()/2,lcd.height()/2-16);
  lcd.setTextColor(lcd.color565(150,180,255));
  lcd.drawString("Toca el centro de cada marca de las esquinas",lcd.width()/2,lcd.height()/2+8);
  delay(1200);
  lcd.calibrateTouch(touchCal, TFT_WHITE, TFT_BLACK, 20);
  lcd.setTouchCalibrate(touchCal);
  saveCalib();
  Serial.print("CALDATA:");for(int i=0;i<8;i++){Serial.print(touchCal[i]);if(i<7)Serial.print(',');}Serial.println();
  Serial.println("[CAL] guardada");
}

// ─── BLE Keyboard Actions ───
void openUrlViaBLE(const char*url){
  if(!bleKb.isConnected())return;
  bleKb.press(KEY_LEFT_GUI);bleKb.press('t');delay(50);bleKb.releaseAll();
  delay(500);
  String u = String(url);
  u.replace("https://www.","");u.replace("http://www.","");
  u.replace("https://","");u.replace("http://","");
  for(int i=0; i<u.length(); i++){
    char c = u[i];
    if(c==':' || c=='/' || c=='?' || c=='#' || c=='@' || c=='&' || c=='=') continue;
    bleKb.press(c);delay(10);bleKb.release(c);delay(10);
  }
  delay(50);bleKb.write(KEY_BACKSPACE);delay(50);bleKb.write(KEY_RETURN);}

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
  // Siempre por serie para que la pestana de Chrome lo gestione (URL/App)
  Serial.printf("BTN:%d:%d:%s\n", idx, buttons[idx].actionType, buttons[idx].action);
  switch(buttons[idx].actionType) {
    case 1: // URL  -> lo abre Chrome via serie
    case 3: // App  -> lo abre Chrome via serie
      break;
    case 2: // Atajo de teclado -> BLE
      sendKeyCombo(buttons[idx].action);
      break;
    case 4: // Texto -> BLE
      if(bleKb.isConnected()) bleKb.print(buttons[idx].action);
      break;
  }
}

// ─── Sidebar ───
void drawGearIcon(int cx,int cy,uint16_t col){lcd.fillCircle(cx,cy,7,col);lcd.fillCircle(cx,cy,3,SB_BG);for(int a=0;a<360;a+=45){float r=a*3.14159/180;lcd.fillCircle(cx+cos(r)*10,cy+sin(r)*10,2,col);}}
void drawSunIcon(int cx,int cy,uint16_t col,bool big){int r=big?6:4;lcd.fillCircle(cx,cy,r,col);int rl=big?11:8;for(int a=0;a<360;a+=45){float rd=a*3.14159/180;lcd.drawLine(cx+cos(rd)*(r+2),cy+sin(rd)*(r+2),cx+cos(rd)*rl,cy+sin(rd)*rl,col);}}
void drawLockIcon(int cx,int cy,uint16_t col,bool isLocked){lcd.fillRoundRect(cx-7,cy-1,14,10,2,col);if(isLocked)lcd.drawArc(cx,cy-1,6,4,180,360,col);else lcd.drawArc(cx+3,cy-1,6,4,180,360,col);lcd.fillCircle(cx,cy+3,2,SB_BG);}

void drawChevron(int cx,int cy,int dir,uint16_t col){
  if(dir<0)lcd.fillTriangle(cx+5,cy-7,cx+5,cy+7,cx-6,cy,col);
  else     lcd.fillTriangle(cx-5,cy-7,cx-5,cy+7,cx+6,cy,col);}

void changePage(int d){int np=curPage+d;if(np<0)np=PAGES-1;if(np>=PAGES)np=0;if(np==curPage)return;curPage=np;infoShown=false;drawAll();}

void drawSidebar(){
  lcd.fillRect(SB_X,0,SIDEBAR_W,SCREEN_H,SB_BG);
  lcd.drawFastVLine(SB_X,0,SCREEN_H,lcd.color565(40,40,50));
  int cx=SB_X+SIDEBAR_W/2,y=0;
  lcd.setTextDatum(middle_center);lcd.setFont(&fonts::Font0);
  // 0: estado BLE
  bool conn=bleKb.isConnected();
  lcd.fillCircle(cx,y+15,5,conn?lcd.color565(0,150,255):lcd.color565(80,80,80));
  if(conn)lcd.drawCircle(cx,y+15,7,lcd.color565(0,70,130));
  lcd.setTextColor(lcd.color565(120,120,130));lcd.drawString(conn?"BT OK":"BT...",cx,y+33);y+=SB_ITEM_H;
  lcd.drawFastHLine(SB_X+6,y,SIDEBAR_W-12,lcd.color565(40,40,50));
  // 1: pagina anterior
  drawChevron(cx,y+16,-1,lcd.color565(150,180,255));
  lcd.setTextColor(lcd.color565(120,120,130));lcd.drawString("Pag",cx,y+34);y+=SB_ITEM_H;
  // 2: indicador de pagina
  char pg[8];snprintf(pg,8,"%d/%d",curPage+1,PAGES);
  lcd.setTextColor(TFT_WHITE);lcd.setFont(&fonts::Font2);lcd.drawString(pg,cx,y+18);
  lcd.setFont(&fonts::Font0);y+=SB_ITEM_H;
  // 3: pagina siguiente
  drawChevron(cx,y+16,1,lcd.color565(150,180,255));
  lcd.setTextColor(lcd.color565(120,120,130));lcd.drawString("Pag",cx,y+34);y+=SB_ITEM_H;
  lcd.drawFastHLine(SB_X+6,y,SIDEBAR_W-12,lcd.color565(40,40,50));
  // 4: Config
  drawGearIcon(cx,y+15,lcd.color565(180,180,200));
  lcd.setTextColor(lcd.color565(120,120,130));lcd.drawString("Config",cx,y+34);y+=SB_ITEM_H;
  // 5: Brillo (cicla al pulsar)
  drawSunIcon(cx,y+15,lcd.color565(255,220,50),true);
  char bb[8];snprintf(bb,8,"%d%%",(brightness*100)/255);lcd.drawString(bb,cx,y+34);y+=SB_ITEM_H;
  lcd.drawFastHLine(SB_X+6,y,SIDEBAR_W-12,lcd.color565(40,40,50));
  // 6: Lock
  uint16_t lc=locked?lcd.color565(231,76,60):lcd.color565(120,120,140);
  drawLockIcon(cx,y+15,lc,locked);
  lcd.setTextColor(locked?lcd.color565(231,76,60):lcd.color565(120,120,130));
  lcd.drawString(locked?"Bloq":"Libre",cx,y+34);}

int sidebarHitTest(int32_t tx,int32_t ty){if(tx<SB_X)return-1;return ty/SB_ITEM_H;}
void handleSidebarTouch(int item){switch(item){
  case 0: break;                  // estado BLE
  case 1: changePage(-1); break;  // pagina anterior
  case 2: break;                  // indicador de pagina
  case 3: changePage(1); break;   // pagina siguiente
  case 4: // Config -> pantalla info con URL
    if(infoShown) hideInfoScreen();
    else { drawInfoScreen(); Serial.println("BTN:99:1:https://sintex85.github.io/diy-streamdeck"); }
    break;
  case 5: // Brillo: cicla 100->70->40->20%
    if(brightness>=255)brightness=180;else if(brightness>=180)brightness=110;else if(brightness>=110)brightness=50;else brightness=255;
    lcd.setBrightness(brightness);saveConfig();drawSidebar();break;
  case 6: locked=!locked;drawSidebar();break;}}

// ─── Botones ───
void getBtnRect(int idx,int&x,int&y){int p=idx%PER_PAGE;x=PAD+(p%COLS)*(BTN_W+PAD);y=PAD+(p/COLS)*(BTN_H+PAD);}
void drawBorder(int bx,int by,int w,int h,uint8_t s,uint8_t cr,uint8_t cg,uint8_t cb){if(s==0)return;uint16_t bc=lcd.color565(min(255,cr+80),min(255,cg+80),min(255,cb+80));if(s==1)lcd.drawRoundRect(bx,by,w,h,RADIUS,bc);else if(s==2)for(int i=0;i<3;i++)lcd.drawRoundRect(bx+i,by+i,w-i*2,h-i*2,RADIUS-i,bc);else if(s==3)for(int g=4;g>=0;g--){uint8_t a=60+(4-g)*45;lcd.drawRoundRect(bx-g,by-g,w+g*2,h+g*2,RADIUS+g,lcd.color565(min(255,(int)cr+a),min(255,(int)cg+a),min(255,(int)cb+a)));}}

void drawButton(int idx,bool pressed){
  if(idx/PER_PAGE!=curPage)return;   // no en la pagina visible
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
    if(buttons[idx].showLabel){lcd.pushImage(ix,by+yOff+(BTN_H/2)-sz/2-8,sz,sz,iconData[idx]);lcd.setTextColor(TFT_WHITE);lcd.setTextDatum(middle_center);lcd.setFont(&fonts::Font2);lcd.drawString(buttons[idx].label,cx,by+yOff+BTN_H-16);}
    else lcd.pushImage(ix,by+yOff+(BTN_H-sz)/2,sz,sz,iconData[idx]);
  }else if(buttons[idx].showLabel){lcd.setTextColor(TFT_WHITE);lcd.setTextDatum(middle_center);lcd.setFont(&fonts::Font4);lcd.drawString(buttons[idx].label,cx,by+yOff+BTN_H/2);}}

void drawAll(){lcd.fillScreen(TFT_BLACK);for(int i=0;i<PER_PAGE;i++)drawButton(curPage*PER_PAGE+i,false);drawSidebar();}
int hitTest(int32_t tx,int32_t ty){if(tx>=SB_X)return-1;for(int i=0;i<PER_PAGE;i++){int g=curPage*PER_PAGE+i;int bx,by;getBtnRect(g,bx,by);if(tx>=bx&&tx<=bx+BTN_W&&ty>=by&&ty<=by+BTN_H)return g;}return-1;}

// ─── Serial Config (Web Serial desde Chrome) ───
static char sBuf[14000];
static int sLen=0;

int sFindCh(char ch,int from){for(int i=from;i<sLen;i++)if(sBuf[i]==ch)return i;return-1;}
int sToInt(int from,int to){char t[12];int n=min(to-from,11);memcpy(t,sBuf+from,n);t[n]=0;return atoi(t);}

void processCmd(){
  while(sLen>0&&(sBuf[sLen-1]==' '||sBuf[sLen-1]=='\t'))sLen--;
  sBuf[sLen]=0;
  if(sLen==0)return;
  Serial.printf("[CMD] %.*s (%d)\n",min(sLen,40),sBuf,sLen);

  if(sLen==6&&memcmp(sBuf,"GETALL",6)==0){
    for(int i=0;i<NUM_BUTTONS;i++)
      Serial.printf("CFG:%d:%s:%d,%d,%d:%d:%d,%d,%d:%d:%s\n",i,buttons[i].label,buttons[i].r,buttons[i].g,buttons[i].b,hasIcon[i]?1:0,buttons[i].iconSizeIdx,buttons[i].borderStyle,buttons[i].showLabel?1:0,buttons[i].actionType,buttons[i].action);
    Serial.println("END");return;}

  if(sLen>=4&&memcmp(sBuf,"SET:",4)==0){
    int p2=sFindCh(':',4);if(p2<0)return;int p3=sFindCh(':',p2+1);if(p3<0)return;
    int idx=sToInt(4,p2);if(idx<0||idx>=NUM_BUTTONS)return;
    int ll=min(p3-p2-1,19);memcpy(buttons[idx].label,sBuf+p2+1,ll);buttons[idx].label[ll]='\0';
    int c1=sFindCh(',',p3+1),c2=sFindCh(',',c1+1);if(c1<0||c2<0)return;
    int nc=sFindCh(':',c2+1);int ce=(nc>=0)?nc:sLen;
    buttons[idx].r=sToInt(p3+1,c1);buttons[idx].g=sToInt(c1+1,c2);buttons[idx].b=sToInt(c2+1,ce);
    if(nc>=0){int s1=sFindCh(',',nc+1),s2=sFindCh(',',s1+1);
      if(s1>=0&&s2>=0){buttons[idx].iconSizeIdx=constrain(sToInt(nc+1,s1),0,3);buttons[idx].borderStyle=constrain(sToInt(s1+1,s2),0,3);buttons[idx].showLabel=sToInt(s2+1,sLen)!=0;}}
    saveConfig();drawButton(idx,false);Serial.println("OK");return;}

  if(sLen>=4&&memcmp(sBuf,"ACT:",4)==0){
    int p2=sFindCh(':',4);if(p2<0)return;int p3=sFindCh(':',p2+1);if(p3<0)return;
    int idx=sToInt(4,p2);if(idx<0||idx>=NUM_BUTTONS)return;
    buttons[idx].actionType=sToInt(p2+1,p3);
    int al=min(sLen-p3-1,127);memcpy(buttons[idx].action,sBuf+p3+1,al);buttons[idx].action[al]='\0';
    saveConfig();Serial.println("OK");return;}

  if(sLen>=5&&memcmp(sBuf,"ICON:",5)==0){
    int p2=sFindCh(':',5);if(p2<0)return;int p3=sFindCh(':',p2+1);if(p3<0)return;
    int idx=sToInt(5,p2);if(idx<0||idx>=NUM_BUTTONS)return;
    int ps=sToInt(p2+1,p3);if(ps<16||ps>64)return;
    int eb=ps*ps*2;
    Serial.printf("[ICON] idx=%d sz=%d b64=%d\n",idx,ps,sLen-p3-1);
    if(iconData[idx])free(iconData[idx]);
    iconData[idx]=(uint16_t*)malloc(eb);   // ESP32 clasico: RAM normal, no PSRAM
    if(!iconData[idx]){Serial.println("ERR:MEM");return;}
    int d=base64_decode(sBuf+p3+1,sLen-p3-1,(uint8_t*)iconData[idx],eb);
    Serial.printf("[ICON] dec=%d exp=%d\n",d,eb);
    if(d>=eb){hasIcon[idx]=true;iconPixelSize[idx]=ps;drawButton(idx,false);Serial.println("OK");}
    else{hasIcon[idx]=false;Serial.printf("ERR:DEC:%d/%d\n",d,eb);}return;}

  if(sLen>=7&&memcmp(sBuf,"NOICON:",7)==0){int idx=atoi(sBuf+7);if(idx>=0&&idx<NUM_BUTTONS){hasIcon[idx]=false;if(iconData[idx]){free(iconData[idx]);iconData[idx]=NULL;}drawButton(idx,false);}Serial.println("OK");return;}
  if(sLen==6&&memcmp(sBuf,"STATUS",6)==0){Serial.printf("BLE:%s\n",bleKb.isConnected()?"CONNECTED":"WAITING");Serial.println("END");return;}
  if(sLen==7&&memcmp(sBuf,"TESTBLE",7)==0){if(bleKb.isConnected()){bleKb.print("StreamDeck OK! ");Serial.println("SENT");}else Serial.println("NOBLE");return;}
  if(sLen==4&&memcmp(sBuf,"PING",4)==0){Serial.println("PONG");return;}
  if(sLen==5&&memcmp(sBuf,"CALIB",5)==0){runCalibration();drawAll();if(infoShown)infoShown=false;return;}
  if(sLen>=5&&memcmp(sBuf,"PAGE:",5)==0){int p=atoi(sBuf+5);if(p>=0&&p<PAGES){curPage=p;infoShown=false;drawAll();}Serial.printf("PAGE:%d\n",curPage);return;}
}

void handleSerial(){
  while(Serial.available()){
    char c=Serial.read();
    if(c=='\n'){processCmd();sLen=0;}
    else if(c!='\r'&&sLen<(int)sizeof(sBuf)-1){sBuf[sLen++]=c;}
  }
}

// ─── Pantalla info / setup ───
void drawInfoScreen(){
  lcd.fillRect(0,0,GRID_W,SCREEN_H,lcd.color565(15,15,30));
  lcd.setTextDatum(middle_center);
  lcd.setTextColor(lcd.color565(100,126,234));lcd.setFont(&fonts::Font4);
  lcd.drawString("DIY Stream Deck",GRID_W/2,26);
  lcd.setTextColor(lcd.color565(120,120,140));lcd.setFont(&fonts::Font2);
  lcd.drawString("by Bits y Tornillos",GRID_W/2,48);
  // Caja URL
  lcd.fillRoundRect(24,64,GRID_W-48,62,10,lcd.color565(25,30,50));
  lcd.drawRoundRect(24,64,GRID_W-48,62,10,lcd.color565(100,126,234));
  lcd.setTextColor(lcd.color565(180,180,200));lcd.setFont(&fonts::Font2);
  lcd.drawString("Abre en Chrome:",GRID_W/2,80);
  lcd.setTextColor(TFT_WHITE);lcd.setFont(&fonts::Font4);
  lcd.drawString("sintex85.github.io",GRID_W/2,100);
  lcd.setTextColor(lcd.color565(200,200,220));lcd.setFont(&fonts::Font2);
  lcd.drawString("/diy-streamdeck",GRID_W/2,118);
  // Pasos
  int sy=148;lcd.setFont(&fonts::Font2);
  const char* steps[]={
    "1. Conecta el USB al PC",
    "2. Abre Chrome con la URL de arriba",
    "3. Pulsa 'Conectar USB'",
    "4. Configura tus botones",
    "5. Deja la pestana abierta",
    "",
    "Bluetooth: empareja 'StreamDeck'",
    "para volumen y atajos de teclado"
  };
  for(int i=0;i<8;i++){
    lcd.setTextColor(i<5?lcd.color565(200,200,210):i==5?0:lcd.color565(100,150,200));
    lcd.drawString(steps[i],GRID_W/2,sy+i*19);
  }
  lcd.setTextColor(lcd.color565(80,80,100));
  lcd.drawString("Toca para volver",GRID_W/2,308);
  infoShown=true;
}

void hideInfoScreen(){infoShown=false;drawAll();}

// ─── Main ───
void setup(){
  Serial.begin(115200);Serial.setRxBufferSize(16384);
  delay(500);Serial.println("[BOOT] Starting...");
  lcd.init();lcd.setRotation(1);   // landscape 480x320
  lcd.setSwapBytes(true);          // iconos: la web manda RGB565 big-endian
  for(int i=0;i<NUM_BUTTONS;i++){iconData[i]=NULL;hasIcon[i]=false;iconPixelSize[i]=32;buttons[i].action[0]='\0';buttons[i].actionType=0;}
  loadConfig();lcd.setBrightness(brightness);

  bool haveCal=loadCalib();
  if(haveCal) lcd.setTouchCalibrate(touchCal);   // aplicar calibracion guardada
  else runCalibration();                         // primer arranque: calibrar tactil

  bool anyAction=false;
  for(int i=0;i<NUM_BUTTONS;i++)if(buttons[i].actionType>0)anyAction=true;
  drawAll();
  if(!anyAction) drawInfoScreen();   // primer arranque: instrucciones

  Serial.println("[BOOT] Starting BLE...");
  bleKb.begin();
  Serial.println("[BOOT] Ready!");}

void loop(){
  handleSerial();
  static bool lastBle=false;bool curBle=bleKb.isConnected();
  if(curBle!=lastBle){lastBle=curBle;if(!infoShown)drawSidebar();Serial.printf("[BLE] %s\n",curBle?"Connected":"Disconnected");}
  int32_t tx,ty;bool touched=lcd.getTouch(&tx,&ty);
  if(touched&&activeButton==-1&&activeSidebar==-1){
    if(infoShown&&tx<SB_X){hideInfoScreen();}
    else{
      int sb=sidebarHitTest(tx,ty);
      if(sb>=0){activeSidebar=sb;handleSidebarTouch(sb);}
      else if(!locked&&!infoShown){int hit=hitTest(tx,ty);if(hit>=0){activeButton=hit;drawButton(hit,true);executeAction(hit);}}}}
  if(!touched){if(activeButton>=0){drawButton(activeButton,false);activeButton=-1;}activeSidebar=-1;}
  delay(10);}
