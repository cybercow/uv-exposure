//////////////////////////////////////////////////////////////////////////

#include <Wire.h>
#include <millisDelay.h>
#include <hd44780.h>
#include <hd44780ioClass/hd44780_I2Cexp.h>

//////////////////////////////////////////////////////////////////////////

hd44780_I2Cexp lcd;

//////////////////////////////////////////////////////////////////////////

typedef enum {
   MACHINE_STATUS_STANDBY
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
  " SET  STOP  CONFIG ",
  " SET <STOP> CONFIG "
};

String subMenuItems[8] = {
  "Start => <Yes> - No ",
  "Start =>  Yes - <No>",
  "Stop => <Yes> - No ",
  "Stop =>  Yes - <No>",
  "LED1 [ ] - LED2 [ ] ",
  "LED1 [x] - LED2 [ ] ",
  "LED1 [ ] - LED2 [x] ",
  "LED1 [x] - LED2 [x] "
};

//////////////////////////////////////////////////////////////////////////

MenuOption menuOption = MENU_OPTION_DEFAULT;
MenuState menuState = MENU_STATE_DEFAULT;
millisDelay menuBlink;
millisDelay menuOptionExpiring;
bool menuBlinkState = false;
const int MENU_BLINK_INTERVAL = 550; // 0.5 seconds
const int MENU_OPTION_EXPIRING_INTERVAL = 10000; // 10 seconds
String lastMenuLine = "";

//////////////////////////////////////////////////////////////////////////

int ledPin = 13;
int switchPin = 12;
unsigned long buttonLastClickTime = 0;
ButtonState buttonState = BUTTON_DEFAULT;
ButtonState lastButtonState = buttonState;
const long doubleClickInterval = 300; 

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

void handleMenuOptions() {
    if (menuOptionExpiring.isFinished() && menuState == MENU_STATE_AWAITING_CONFIRM) {
      menuState = MENU_STATE_DEFAULT;
      menuOption = MENU_OPTION_DEFAULT;
      return;
    }
  
    if (buttonState == lastButtonState)
      return;
      
    switch (buttonState) {
      case BUTTON_CLICK_1: {
          MenuOption opt = MenuOption(menuOption + 1);
          if (opt > MENU_OPTION_CONFIG) 
            opt = MENU_OPTION_SET;
          menuOption = opt;
          menuState = MENU_STATE_AWAITING_CONFIRM;
          break;
      }
      
      case BUTTON_CLICK_2: {
          if (menuState == MENU_STATE_AWAITING_CONFIRM) {
             menuBlink.stop();
             menuState = MENU_STATE_OPTION_ENABLED;
          }
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
  
  switch(menuState) {
    case MENU_STATE_DEFAULT:
      menuLine = menuItems[MENU_OPTION_DEFAULT];
      break;

    case MENU_STATE_AWAITING_CONFIRM:
      if (menuBlink.isFinished()) {
        menuBlinkState = !menuBlinkState;
        menuBlink.repeat();
      }
      menuLine = !menuBlinkState ? menuItems[menuOption] : menuItems[MENU_OPTION_DEFAULT];
      break;

    case MENU_STATE_OPTION_ENABLED:
      menuLine = menuItems[menuOption];
      break;

    default:
      break;
  }

  if (menuLine == lastMenuLine)
    return;
  
  lastMenuLine = menuLine;
  
  lcd.setCursor(0,3);
  lcd.print(menuLine);
}

//////////////////////////////////////////////////////////////////////////

void renderContent()
{
  switch(menuState) {
     case MENU_STATE_OPTION_ENABLED:
        break;
  }
}

void setup() {

  Serial.begin(9600);

  /////////////////////////////////

  pinMode(ledPin, OUTPUT);
  pinMode(switchPin, INPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  /////////////////////////////////
  
  lcd.begin(20, 4);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.begin(20,4); 
  lcd.print("-- UV-PCB Scanner --");

  Serial.println("setup ...");
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
