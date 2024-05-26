#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>
#include "Adafruit_Sensor.h"
#include "Adafruit_AM2320.h"

// CONSTANTS FOR PINS & MISC
const int BUTTON_PIN = 2;
const int SERVO_PIN = 6;

const char LCD_ADDRESS = 0x27;
const float ERROR = -999999;

// CONSTANTS FOR CONFIG
const unsigned long LONG_PRESS_TIME = 1000;
const char* MODES[] = {"Temperature", "Humidity"};
const int MODE_COUNT = 2;
const int LIMITS[] = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
const int LIMIT_COUNT = 10;
const char* OPERATORS[] = {"<", ">"};
const int OPERATOR_COUNT = 2;

// CONSTANTS FOR LCD TEXTS
char TITLE_TEXT[] = " -- BUTTONPUSHER -- ";
String TOOLTIP_TEXT = "hold btn " + String(LONG_PRESS_TIME / 1000) + "s to conf.";
char SELECTED_LIMIT_TEXT[] = "selected lim: ";
char CURRENT_VALUE_TEXT[] = "current val.: ";
char LONG_TOOLTIP_TEXT[] = "long btn: continue  ";
char SHORT_TOOLTIP_TEXT[] = "short btn: cycle val";

// GLOBALS FOR BUTTON HANDLING
unsigned long elapsed_time = 0;
unsigned long previous_elapsed_time = 0;

bool press_started = false;
bool is_long_pressed = false;
bool is_short_pressed = false;

// GLOBALS FOR STATE
int screen_number = 0;
int mode = 1;
int limit = 1;
int op = 1;
float value = ERROR;

bool use_custom_limit = false;
int custom_limit_serial = 0;
int custom_limit = 0;

// DEVICES
Adafruit_AM2320 am2320 = Adafruit_AM2320(); // temp/humidity sensor
Servo myservo; // servo
LiquidCrystal_I2C lcd(LCD_ADDRESS, 20, 4); // lcd

void setup() {
  Serial.begin(9600);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT);
  pinMode(SERVO_PIN, OUTPUT);

  // Curernt ms as elapsed time
  previous_elapsed_time = millis();
  
  // Init temp/humidity sensor
  am2320.begin();

  // Init servo
  myservo.attach(SERVO_PIN);

  // Init lcd
  lcd.init();
  lcd.backlight();
}

void loop() {
  // uncomment for debug printing
  // debugPrint();

  // Read custom value from Serial
  if (Serial.available() > 0) {
    String input_line = Serial.readStringUntil('\n');
    input_line.trim(); // Remove leading/trailing spaces

    // Convert the input to an integer
    int temp_value = input_line.toInt();

    if (temp_value >= -999 && temp_value <= 999) {
      custom_limit = temp_value;
      use_custom_limit = true;
      Serial.print("Accepted int: ");
      Serial.println(custom_limit);
    } else {
      Serial.println("Value out of range. Must be between -999 and 999.");
    }
  }

  // Grab the button value and check press dration
  int btn_value = digitalRead(BUTTON_PIN);
  checkButtonPress(btn_value);

  // Change screen number on long press
  if (is_long_pressed) {
    screen_number += 1;
  }
  
  // Show screen based on screen_number
  if (screen_number == 0) {
    defaultScreen();
  } else if (screen_number == 1) {
    modeConfigScreen();
  } else if (screen_number == 2) {
    limitConfigScreen();
  } else if (screen_number == 3) {
    operatorConfigScreen();
  } else {
    screen_number = 0;
    defaultScreen();
  }

  // Move servo if in default view (screen_number == 0)
  // if evaluate() true -> move to 270, if evaluate() false -> reset to 0
  if (screen_number == 0) {
    bool move_servo = false;
    if (use_custom_limit) {
      move_servo = compare(value, custom_limit);
    } else {
      move_servo = compare(value, LIMITS[limit - 1]);
    }
    if (move_servo) {
      myservo.write(270);
      delay(100);        
    } else {
      myservo.write(0);
      delay(100);
    }
  }

  // Reset button press and add small delay
  is_short_pressed = false;
  is_long_pressed = false;
  delay(10);
}

// The default screen showing: 
// Title, selected mode with operator, selected limit and live sensor readnig for selected mode
void defaultScreen() {

  lcd.setCursor(0,0);
  lcd.print(TITLE_TEXT);
  lcd.setCursor(0,1);
  lcd.print(TOOLTIP_TEXT);
  
  // Print current config info. Examples:
  // "mode: H < limit: 30" (humidity less than 30)
  // "mode: T > limit: 10" (temperature more than 10)
  lcd.setCursor(0,2);
  lcd.print("mode: ");
  lcd.print(MODES[mode - 1][0]);
  lcd.print(" ");
  lcd.print(OPERATORS[op - 1][0]);
  lcd.print(" limit:");
  if (use_custom_limit) {
    printLimit(custom_limit);
  } else {
    printLimit(LIMITS[limit-1]);
  }

  // Read value based on selected mode
  value = ERROR;
  if (mode == 1) {
    value = am2320.readTemperature();
  } else if (mode == 2) {
    value = am2320.readHumidity();
  } 

  // Print current sensor reading for selected mode
  lcd.setCursor(0,3);
  lcd.print(CURRENT_VALUE_TEXT);
  lcd.print(value);
}

// Config screen for changing selected mode
void modeConfigScreen() {
  mode = cycleOnShortPress(mode, MODE_COUNT);
  lcd.setCursor(0,0);
  printConfigInfo("    MODE  CONFIG    ");
  lcd.setCursor(0,3);
  lcd.print("mode:               ");  
  lcd.setCursor(6,3);
  lcd.print(MODES[mode - 1]);  
}

// Config screen for changing selected limit
void limitConfigScreen() {

  if (is_short_pressed) {
    use_custom_limit = false;
  }

  limit = cycleOnShortPress(limit, LIMIT_COUNT);
  lcd.setCursor(0,0);
  printConfigInfo("    LIMIT CONFIG    ");
  lcd.setCursor(0,3);
  lcd.print("limit:              ");
  lcd.setCursor(7, 3);
  lcd.print(LIMITS[limit - 1]);
}

// Config screen for changing selected operator
void operatorConfigScreen() {
  op = cycleOnShortPress(op, OPERATOR_COUNT);
  printConfigInfo("  OPERATOR  CONFIG  ");
  lcd.setCursor(0,3);
  lcd.print("operator:           ");
  lcd.setCursor(10, 3);
  lcd.print(OPERATORS[op - 1]);
}

// Function for cycling array indexes on short press
int cycleOnShortPress(int val, int count) {
  if (is_short_pressed) {
    val++;
    if (val > count) {
      val = 1;
    }
  }
  return val;
}

// Function for compares given values using globally set operator
bool compare(float value, int value2) {
  if (value == ERROR) {
    return false;
  }

  if (op == 1) {
    return (value < value2);
  } else if (op == 2) {
    return (value > value2);
  } else {
    return false;
  }
}

// Calls either buttonDown or buttonUp based on btn_value.
void checkButtonPress(int btn_value) {
  if (btn_value == LOW) {
    buttonDown();
  } else {
    if (press_started) {
      buttonUp();
    }
  }
}

// Acts like an event for button down. 
// Starts or continues tracking the time button has been pressed
void buttonDown() {
  if (press_started == false) {
    elapsed_time = 0;
    press_started = true;
    elapsed_time = 0;
    previous_elapsed_time = millis();
  } else {
    elapsed_time = millis() - previous_elapsed_time;
  }
}

// Acts like an event for button up. 
// Decides if press was either short or long based on time tracked
void buttonUp() {
  press_started = false;
  is_short_pressed = false;
  is_long_pressed = false;
  if (elapsed_time < LONG_PRESS_TIME) {
    is_short_pressed = true;
  } else {
    is_long_pressed = true;
  }
}

// Prints the limit with needed whitespace around it.
void printLimit(int val) {
  if (val >= 0) {
    lcd.print(" ");
  }
  lcd.print(val);
  if (val < 100 && val > -100) {
    lcd.print(" ");
  }
  if (val < 10 && val > -10) {
    lcd.print(" ");
  }
}

// Prints given title + tooltips for a config screen
void printConfigInfo(String title) {
  lcd.setCursor(0,0);
  lcd.print(title);
  lcd.setCursor(0,1);
  lcd.print(LONG_TOOLTIP_TEXT);
  lcd.setCursor(0,2);
  lcd.print(SHORT_TOOLTIP_TEXT);
}

// Debug printing of important values
void debugPrint() {
  Serial.println("");
  Serial.print("Value ");
  Serial.print(value);
  Serial.print(", Limit ");
  Serial.print(LIMITS[limit - 1]);
  Serial.print(", Op ");
  Serial.print(OPERATORS[op - 1]);
  Serial.print(" ::: ");
  Serial.print(compare(value, LIMITS[limit - 1]));
}
