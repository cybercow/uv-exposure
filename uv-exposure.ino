/////////////////////////////////////////////////////////////////////////////////
//
//   UV exposure box controller for making PCB's @ home
//   working on ARDUINO MEGA2560 + LCD 20x4 (2004)
//   inspired by this: https://www.youtube.com/watch?v=ZAlxNNI-BVM
//   ... but different principle of operation:
//   - minimalistic but functional UI on 4 rows 20 characters LCD
//   - only one command (push) button (no rotary encoder or microswitches)
//     use double clicks with continuous intuitive clicking for navigation
//   - two indipendent single timed channels, combine as you wish like for eg.:
//     use 1st. channel for UV-LED strip for UV exposure of PCB
//     use 2nd. channel for regular white LED strip and PCB/film inspection
//     you got the idea, combine as you wish ...
//   - use two relays for powering both channels - or use a MOSFET switch as
//     did the author from the YT video bellow ...
//   - take care of LED/PSU power to not exceed combined power of Arduino + 
//     accessories + combined LED strips, i'm using 12V / 5A LED driver       
//   - this is work in progress!
//
//  (c)2019 by cybercow222
//
//   v 0.1a
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
   MACHINE_STATUS_STANDBY,
   MACHINE_STATUS_RUNNING
} MachineState;

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

String subMenuItems[9] = {
  "                    ",
  "start:  yes  - <no> ",
  "start: <yes> -  no  ",
  "stop:  yes  - <no>  ",
  "stop: <yes> -  no   ",
  "led1 [ ] - led2 [ ] ",
  "led1 [x] - led2 [ ] ",
  "led1 [ ] - led2 [x] ",
  "led1 [x] - led2 [x] "
};

//////////////////////////////////////////////////////////////////////////

MenuOption menuOption = MENU_OPTION_DEFAULT;
MenuState menuState = MENU_STATE_DEFAULT;
SubMenuOption subMenuOption = SUBMENU_OPTION_DEFAULT;

millisDelay menuBlink;
millisDelay menuOptionExpiring;
millisDelay subMenuOptionCommit;
millisDelay masterTimer;

bool menuBlinkState = false;
const int MENU_BLINK_INTERVAL = 400; // 0.4 seconds
const int MENU_OPTION_EXPIRING_INTERVAL = 10000; // 10 seconds
const int SUBMENU_OPTION_COMMIT_INTERVAL = 1250; // 1.5 seconds
const long MASTER_TIMER_DEFAULT = 600000; // 10 minutes
unsigned long masterTimerLength = MASTER_TIMER_DEFAULT;
int masterTimerIncrement = 30000; // 30 seconds

String lastMenuLine = "";
String lastSubMenuLine = "";
String lastTimerDisplay = "";

//////////////////////////////////////////////////////////////////////////

int ledPin = 13;
int switchPin = 12;
int ledStripPin1 = 8;
int ledStripPin2 = 9;

unsigned long buttonLastClickTime = 0;
ButtonState buttonState = BUTTON_DEFAULT;
ButtonState lastButtonState = buttonState;
const long doubleClickInterval = 300; // 0.3 seconds

//////////////////////////////////////////////////////////////////////////

bool ledStripState1 = false;
bool ledStripState2 = false;

//////////////////////////////////////////////////////////////////////////

char* timeToString(unsigned long ms) {
  static char str[12];
  unsigned long t = ms / 1000;
  //long h = t / 3600;
  t = t % 3600;
  int m = t / 60;
  int s = t % 60;
  sprintf(str, "%02d:%02d", m, s);
  return str;
}

//////////////////////////////////////////////////////////////////////////

void handleButton() { 
  int readSwitch = digitalRead(switchPin);
  unsigned long currentMillis = millis();
  
  if (readSwitch == HIGH && buttonState == BUTTON_DEFAULT) {
     buttonLastClickTime = currentMillis;
     buttonState = BUTTON_DEFAULT_HI;
     //Serial.println("BUTTON_DEFAULT_HI");
     return;
  }

  if (readSwitch == LOW && buttonState == BUTTON_DEFAULT_HI && currentMillis - buttonLastClickTime <= doubleClickInterval) {
     buttonState = BUTTON_CLICK_2_PENDING;
     //Serial.println("BUTTON_CLICK_2_PENDING");
     return;
  }

  if (readSwitch == HIGH && buttonState == BUTTON_CLICK_2_PENDING && currentMillis - buttonLastClickTime <= doubleClickInterval) {
     buttonState = BUTTON_CLICK_2;
     //Serial.println("BUTTON_CLICK_2");
     return;
  }

  if (readSwitch == LOW && (buttonState == BUTTON_CLICK_2_PENDING) && currentMillis - buttonLastClickTime > doubleClickInterval) {
     buttonState = BUTTON_CLICK_1;
     //Serial.println("BUTTON_CLICK_1");
     return;
  }
  
  if (readSwitch == LOW && buttonState != BUTTON_DEFAULT && currentMillis - buttonLastClickTime > doubleClickInterval + 50) {
     buttonState = BUTTON_DEFAULT;
     //Serial.println("BUTTON_DEFAULT");
     return;
  }
}

//////////////////////////////////////////////////////////////////////////

SubMenuOption getSubMenuOption(bool next = false) {
  SubMenuOption res = SUBMENU_OPTION_DEFAULT;
  
  switch(menuOption) {
     case MENU_OPTION_SET: {
      /////////////////////////////////////////////
      Serial.println("set");
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

void handleMenuOptions() {
    if (subMenuOptionCommit.isFinished() && menuState == MENU_STATE_OPTION_ENABLED) {
      
      if (subMenuOption == SUBMENU_OPTION_START_YES)
         masterTimer.start(masterTimerLength);

      if (subMenuOption == SUBMENU_OPTION_STOP_YES)
         masterTimer.stop();
         
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
      case BUTTON_CLICK_1: {
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
          if (menuState == MENU_STATE_OPTION_ENABLED)
              subMenuOptionCommit.start(SUBMENU_OPTION_COMMIT_INTERVAL);
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

void renderContent()
{
  unsigned long t = masterTimer.isRunning() ? masterTimer.remaining() : masterTimerLength;
  String displayTime = timeToString(t);
  
  if (lastTimerDisplay == displayTime)
    return;
  
  lcd.setCursor(0, 1);
  lcd.print("time:");
  lcd.setCursor(6, 1);
  lcd.print(displayTime);

  lastTimerDisplay = displayTime;
}

void setup() {

  Serial.begin(9600);

  /////////////////////////////////

  pinMode(ledPin, OUTPUT);
  pinMode(switchPin, INPUT);
  pinMode(ledStripPin1, OUTPUT);
  pinMode(ledStripPin2, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  /////////////////////////////////
  
  lcd.begin(20, 4);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("-- UV-PCB Scanner --");
}

void loop () {
  /////////////////////////////////
  
  handleButton();

  /////////////////////////////////

  handleMenuOptions();
  
  /////////////////////////////////
  
  renderMenu();
  
  /////////////////////////////////

  renderContent();

  /////////////////////////////////
}

/*
 * 
 *In addition to the start(delay), isFinished() and repeat() functions illustrated above, the millisDelay library also has stop() to stop the delay timing out, 
 *isRunning() to check if it has not already timed out and has not been stopped, 
 *restart() to restart the delay from now, using the same delay interval, finish() to force the delay to expire early, 
 *remaining() to return the number of milliseconds until the delay is finished and delay() to return the delay value that was passed to start()
 * 
 */
