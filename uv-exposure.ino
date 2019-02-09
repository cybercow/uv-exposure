/////////////////////////////////////////////////////////////////////////////////
//
//   UV exposure box controller for making PCB's @ home
//   ARDUINO UNO + LCD 20x4 (2004)
//   (c)2019 cybercow222 / v 0.2b
//
/////////////////////////////////////////////////////////////////////////////////

#include <Wire.h>
#include <millisDelay.h>
#include <hd44780.h>
#include <hd44780ioClass/hd44780_I2Cexp.h>

//////////////////////////////////////////////////////////////////////////

hd44780_I2Cexp lcd;

//////////////////////////////////////////////////////////////////////////

typedef enum {
   MENU_OPTION_DEFAULT,
   MENU_OPTION_SET,
   MENU_OPTION_START,
   MENU_OPTION_CONFIG,
   MENU_OPTION_STOP_DEFAULT,
   MENU_OPTION_STOP
} MenuOption;

typedef enum {
   MENU_STATE_DEFAULT,
   MENU_STATE_AWAITING_CONFIRM,
   MENU_STATE_OPTION_ENABLED
} MenuState;

typedef enum {
   SUBMENU_OPTION_DEFAULT,
   SUBMENU_OPTION_SET_RESET,
   SUBMENU_OPTION_SET_INCR,
   SUBMENU_OPTION_START_NO,
   SUBMENU_OPTION_START_YES,
   SUBMENU_OPTION_STOP_NO,
   SUBMENU_OPTION_STOP_YES,
   SUBMENU_OPTION_LED_NO,
   SUBMENU_OPTION_LED_ONE,
   SUBMENU_OPTION_LED_TWO,
   SUBMENU_OPTION_LED_FULL
} SubMenuOption;

typedef enum {
   BUTTON_DEFAULT,
   BUTTON_DEFAULT_HI,
   BUTTON_CLICK_2_PENDING,
   BUTTON_CLICK_2,
   BUTTON_CLICK_1
} ButtonState;

String menuItems[6] = { 
  " SET  START  CONFIG ",
  "<SET> START  CONFIG ", 
  " SET <START> CONFIG ",
  " SET  START <CONFIG>",
  " -----  STOP  ----- ",
  " ----- <STOP> ----- "
};

String subMenuItems[11] = {
  "                    ",
  "reset [x] - incr [ ]",
  "reset [ ] - incr [x]",
  "start   yes  - <no> ",
  "start  <yes> -  no  ",
  "stop   yes  - <no>  ",
  "stop  <yes> -  no   ",
  "led1 [ ] - led2 [ ] ",
  "led1 [x] - led2 [ ] ",
  "led1 [ ] - led2 [x] ",
  "led1 [x] - led2 [x] "
};

//////////////////////////////////////////////////////////////////////////

MenuOption menuOption = MENU_OPTION_DEFAULT;
MenuState menuState = MENU_STATE_DEFAULT;
SubMenuOption subMenuOption = SUBMENU_OPTION_DEFAULT;

millisDelay masterTimer;
millisDelay menuBlink;
millisDelay menuOptionExpiring;
millisDelay subMenuOptionCommit;
millisDelay screenSaverTimer;

//////////////////////////////////////////////////////////////////////////

// interval between 2 consequent clicks that will be registered as "double click"
const int DOUBLE_CLICK_INTERVAL = 300; // 0.3 seconds

// interval between blinking option on screen menu
const int MENU_BLINK_INTERVAL = 400; // 0.4 seconds

// time when activated menu option will expire to default menu state
const int MENU_OPTION_EXPIRING_INTERVAL = 10000; // 10 seconds

// time when selected submenu option will be accepted
const int SUBMENU_OPTION_COMMIT_INTERVAL = 1250; // 1.25 seconds

// the main timer default length, usually the PCB UV exposure goes for ~5 minutes or such
const long MASTER_TIMER_DEFAULT = 300000; // 5 minutes

// maximum main interval length that can be reached
const long MASTER_TIMER_MAX = MASTER_TIMER_DEFAULT * 3;

// increments (or resolution) of setting the interval - 30 seconds should be enough
const int MASTER_TIMER_INCREMENT = 30000; // 30 seconds

// simple screen saver to save some power
const bool SCREEN_SAVER_ENABLED = true;

// time before main timer finishing the lcd will be lit
const long SCREEN_SAVER_WAKEUP = MASTER_TIMER_DEFAULT * 0.25;

// the changeable length of the main timer
unsigned long masterTimerLength = MASTER_TIMER_DEFAULT;

// built-in led pin
const int LED_PIN = 13;

// control button pin
const int SWITCH_PIN = 12;

// output for first control channel
const int LED_STRIP_PIN1 = 8;

// output for second control channel
const int LED_STRIP_PIN2 = 9;

//////////////////////////////////////////////////////////////////////////

bool menuBlinkState = false;
bool screenSaverActive = false;

String lastMenuLine = "";
String lastSubMenuLine = "";
String lastTimerDisplay = "";
String lastStrip1Display = "";
String lastStrip2Display = "";

//////////////////////////////////////////////////////////////////////////

unsigned long buttonLastClickTime = 0;
ButtonState buttonState = BUTTON_DEFAULT;
ButtonState lastButtonState = buttonState;

//////////////////////////////////////////////////////////////////////////

bool ledStripState1 = false;
bool ledStripState2 = false;

//////////////////////////////////////////////////////////////////////////

char* timeToString(unsigned long ms) {
  static char str[12];
  unsigned long t = ms / 1000;
  t = t % 3600;
  int m = t / 60;
  int s = t % 60;
  sprintf(str, "%02d:%02d", m, s);
  return str;
}

//////////////////////////////////////////////////////////////////////////

void switchOn(bool state = false) {
  if (state) {
    digitalWrite(LED_STRIP_PIN1, ledStripState1 ? HIGH : LOW);
    digitalWrite(LED_STRIP_PIN2, ledStripState2 ? HIGH : LOW);
    return;
  }

  digitalWrite(LED_STRIP_PIN1, LOW);
  digitalWrite(LED_STRIP_PIN2, LOW);
}

//////////////////////////////////////////////////////////////////////////

void screenSaver(bool reset = false) {
   if (SCREEN_SAVER_ENABLED == false)
      return;

   if (reset) {
      screenSaverActive = false;
      screenSaverTimer.stop();
      lcd.backlight();
      return;
   }

   if (!screenSaverTimer.isRunning())
      screenSaverTimer.start(MASTER_TIMER_DEFAULT * 0.25);
}

//////////////////////////////////////////////////////////////////////////

void handleButton() { 
  int readSwitch = digitalRead(SWITCH_PIN);
  unsigned long currentMillis = millis();
  
  if (readSwitch == HIGH && buttonState == BUTTON_DEFAULT) {
     buttonLastClickTime = currentMillis;
     buttonState = BUTTON_DEFAULT_HI;
     return;
  }

  if (readSwitch == LOW && buttonState == BUTTON_DEFAULT_HI && currentMillis - buttonLastClickTime <= DOUBLE_CLICK_INTERVAL) {
     buttonState = BUTTON_CLICK_2_PENDING;
     return;
  }

  if (readSwitch == HIGH && buttonState == BUTTON_CLICK_2_PENDING && currentMillis - buttonLastClickTime <= DOUBLE_CLICK_INTERVAL) {
     buttonState = BUTTON_CLICK_2;
     return;
  }

  if (readSwitch == LOW && (buttonState == BUTTON_CLICK_2_PENDING) && currentMillis - buttonLastClickTime > DOUBLE_CLICK_INTERVAL) {
     buttonState = BUTTON_CLICK_1;
     return;
  }
  
  if (readSwitch == LOW && buttonState != BUTTON_DEFAULT && currentMillis - buttonLastClickTime > DOUBLE_CLICK_INTERVAL + 50) {
     buttonState = BUTTON_DEFAULT;
     return;
  }
}

//////////////////////////////////////////////////////////////////////////

SubMenuOption getSubMenuOption(bool next = false) {
  
  SubMenuOption res = SUBMENU_OPTION_DEFAULT;
  
  switch(menuOption) {
     case MENU_OPTION_SET: {
      /////////////////////////////////////////////
      res = SUBMENU_OPTION_SET_RESET;
      SubMenuOption last = subMenuOption;
      
      if (next)
         res = SUBMENU_OPTION_SET_INCR;

      if (next && last != res)
         break;
      
      if (next && res == SUBMENU_OPTION_SET_INCR)
         masterTimerLength += MASTER_TIMER_INCREMENT;

      if (next && masterTimerLength > MASTER_TIMER_MAX)
         masterTimerLength = MASTER_TIMER_INCREMENT;
      break;
    }
    
    case MENU_OPTION_START: {
      /////////////////////////////////////////////
      res = SUBMENU_OPTION_START_NO;
      if (next)
         res = subMenuOption == SUBMENU_OPTION_START_NO ? SUBMENU_OPTION_START_YES : SUBMENU_OPTION_START_NO;
      break;
    }
    
    case MENU_OPTION_STOP: {
      /////////////////////////////////////////////
      res = SUBMENU_OPTION_STOP_NO;
      if (next)
        res = subMenuOption == SUBMENU_OPTION_STOP_NO ? SUBMENU_OPTION_STOP_YES : SUBMENU_OPTION_STOP_NO;
      break;
    }
    
    case MENU_OPTION_CONFIG: {
      /////////////////////////////////////////////
      int ledStripState = 0;
      ledStripState += ledStripState1 ? 1 : 0;
      ledStripState += ledStripState2 ? 2 : 0;
      res = SubMenuOption(SUBMENU_OPTION_LED_NO + ledStripState);      
      if (next)
         res = SubMenuOption(res + 1);   
      if (res > SUBMENU_OPTION_LED_FULL)
         res = SUBMENU_OPTION_LED_NO;
      ledStripState1 = res == SUBMENU_OPTION_LED_ONE || res == SUBMENU_OPTION_LED_FULL;
      ledStripState2 = res == SUBMENU_OPTION_LED_TWO || res == SUBMENU_OPTION_LED_FULL;
      break;
    }
    
    default:
      break;
  }

  return SubMenuOption(res);
}

//////////////////////////////////////////////////////////////////////////

int digitalReadOutputPin(uint8_t pin) {
   uint8_t bit = digitalPinToBitMask(pin);
   uint8_t port = digitalPinToPort(pin);
   if (port == NOT_A_PIN) 
     return LOW;

   return (*portOutputRegister(port) & bit) ? HIGH : LOW;
}

//////////////////////////////////////////////////////////////////////////

void handleMenuOptions() {

    if (SCREEN_SAVER_ENABLED && screenSaverActive && masterTimer.isRunning() && masterTimer.remaining() <= SCREEN_SAVER_WAKEUP) {
        screenSaver(true);
        return;
    }

    if (SCREEN_SAVER_ENABLED && screenSaverTimer.isFinished()) {
        lcd.noBacklight();
        screenSaverActive = true;
        return;
    }

    if (masterTimer.isFinished() && (menuOption == MENU_OPTION_STOP_DEFAULT || menuOption == MENU_OPTION_STOP)) {
        switchOn(false);
        menuState = MENU_STATE_DEFAULT;
        menuOption = MENU_OPTION_DEFAULT;
        subMenuOption = SUBMENU_OPTION_DEFAULT;
        screenSaver();
        return;
    }
  
    if (subMenuOptionCommit.isFinished() && menuState == MENU_STATE_OPTION_ENABLED) {
      
      if (subMenuOption == SUBMENU_OPTION_START_YES) {
         masterTimer.start(masterTimerLength);
         switchOn(true);
      }

      if (subMenuOption == SUBMENU_OPTION_STOP_YES) {
         masterTimer.stop();
         switchOn(false);
      }

      if (subMenuOption == SUBMENU_OPTION_SET_RESET)
         masterTimerLength = MASTER_TIMER_DEFAULT;
         
       menuState = MENU_STATE_DEFAULT;
       menuOption = masterTimer.isRunning() ?  MENU_OPTION_STOP_DEFAULT : MENU_OPTION_DEFAULT;
       subMenuOption = SUBMENU_OPTION_DEFAULT;
       return; 
    }
  
    if (menuOptionExpiring.isFinished() && menuState == MENU_STATE_AWAITING_CONFIRM) {
        menuState = MENU_STATE_DEFAULT;
        menuOption = !masterTimer.isRunning() ? MENU_OPTION_DEFAULT : MENU_OPTION_STOP_DEFAULT;
        return;
    }
  
    if (buttonState == lastButtonState)
        return;
      
    switch (buttonState) {
      case BUTTON_DEFAULT_HI:
          screenSaver(true);
          break;

      case BUTTON_CLICK_1: {
          /////////////////////////////////////////////
          if (screenSaverActive) {
              screenSaverActive = false;
              break;
          }
          
          /////////////////////////////////////////////
          if (menuState == MENU_STATE_OPTION_ENABLED) {
            subMenuOptionCommit.stop();
            subMenuOption = getSubMenuOption(true);
            break;
          }
        
          /////////////////////////////////////////////
          MenuOption opt = MenuOption(menuOption + 1);
          if (!masterTimer.isRunning() && opt > MENU_OPTION_CONFIG) 
            opt = MENU_OPTION_SET;

          if (masterTimer.isRunning())
            opt = MENU_OPTION_STOP;
              
          menuOption = opt;
          menuState = MENU_STATE_AWAITING_CONFIRM;
          break;
      }
      
      case BUTTON_CLICK_2: {
          /////////////////////////////////////////////
          if (menuState == MENU_STATE_AWAITING_CONFIRM) {
             menuBlink.stop();
             menuState = MENU_STATE_OPTION_ENABLED;
             subMenuOption = getSubMenuOption();
          }
          break;
      }

      case BUTTON_DEFAULT: {
          /////////////////////////////////////////////         
          if (menuState == MENU_STATE_OPTION_ENABLED)
              subMenuOptionCommit.start(SUBMENU_OPTION_COMMIT_INTERVAL);
              
          screenSaver();
          break;
      }
      
      default:
          break;
    }

    if (menuState == MENU_STATE_AWAITING_CONFIRM) {
        menuBlinkState = false;
        menuBlink.start(MENU_BLINK_INTERVAL);
        menuOptionExpiring.start(MENU_OPTION_EXPIRING_INTERVAL);
    }

    lastButtonState = buttonState;
}

//////////////////////////////////////////////////////////////////////////

void renderMenu() {

  String menuLine = "";
  String subMenuLine = "";
  
  switch(menuState) {
    case MENU_STATE_DEFAULT: {
      menuLine = menuItems[menuOption];
      subMenuLine = subMenuItems[subMenuOption];
      break;
    }

    case MENU_STATE_AWAITING_CONFIRM: {
      if (menuBlink.isFinished()) {
        menuBlinkState = !menuBlinkState;
        menuBlink.repeat();
      }

      MenuOption defopt = !masterTimer.isRunning() ? MENU_OPTION_DEFAULT : MENU_OPTION_STOP_DEFAULT;
      menuLine = !menuBlinkState ? menuItems[menuOption] : menuItems[defopt];
      break;
    }

    case MENU_STATE_OPTION_ENABLED: {
      menuLine = menuItems[menuOption];
      subMenuLine = subMenuItems[subMenuOption];
      break;
    }

    default:
      break;
  }

  if (menuLine != lastMenuLine) { 
      lastMenuLine = menuLine;
      lcd.setCursor(0,3);
      lcd.print(menuLine);
  }

  if (subMenuLine != lastSubMenuLine) {
      lastSubMenuLine = subMenuLine;
      lcd.setCursor(0,2);
      lcd.print(subMenuLine);
  }
}

//////////////////////////////////////////////////////////////////////////

void renderTime() {
  unsigned long t = masterTimer.isRunning() ? masterTimer.remaining() : masterTimerLength;
  String displayTime = timeToString(t);
  if (lastTimerDisplay == displayTime)
    return;
  
  lcd.setCursor(6, 1);
  lcd.print(displayTime);

  lastTimerDisplay = displayTime;
}

//////////////////////////////////////////////////////////////////////////

void renderStripStatus() {
     
  String strip1Display = digitalReadOutputPin(LED_STRIP_PIN1) ? "L1" : "  ";
  String strip2Display = digitalReadOutputPin(LED_STRIP_PIN2) ? "L2" : "  ";

  if (strip1Display != lastStrip1Display) {
      lcd.setCursor(14, 1);
      lcd.print(strip1Display);
      lastStrip1Display = strip1Display;
  }

  if (strip2Display != lastStrip2Display) {
      lcd.setCursor(18, 1);
      lcd.print(strip2Display);
      lastStrip2Display = strip2Display;
  }
}

//////////////////////////////////////////////////////////////////////////

void setup() {

  Serial.begin(9600);

  /////////////////////////////////

  pinMode(LED_PIN, OUTPUT);
  pinMode(SWITCH_PIN, INPUT);
  pinMode(LED_STRIP_PIN1, OUTPUT);
  pinMode(LED_STRIP_PIN2, OUTPUT);

  /////////////////////////////////
  
  digitalWrite(LED_BUILTIN, HIGH);
  switchOn(false);

  /////////////////////////////////
  
  lcd.begin(20, 4);
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("-- UV-PCB Scanner --");

  lcd.setCursor(0, 1);
  lcd.print("time:");

  /////////////////////////////////

  screenSaver();
}

void loop () {
  
  handleButton();

  handleMenuOptions();

  renderMenu();

  renderTime();

  renderStripStatus();
 
}
