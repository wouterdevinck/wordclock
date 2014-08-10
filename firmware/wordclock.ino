// Wordclock firmware
// ==================
// November 2013 - August 2014
// by Wouter Devinck

// Dependencies:
//  * Arduino libraries                - http://arduino.cc/
//  * Chronodot library (for DS3231)   - https://github.com/Stephanie-Maks/Arduino-Chronodot
//  * LedControl library (for MAX7219) - http://playground.arduino.cc/Main/LedControl

/* Hardware block diagram:

              +-----------------+                            
              | Real time clock |                            
              | Maxim DS3231    |                            
              +--------+--------+                            
                       |I2C                                  
         +-------------+-------------+                       
         |                           |   +------------------+
         |                           +---+ 8x8 LED matrix 1 |
+---+    |                           |   | Maxim MAX7219    |
|LDR+----+                           |   +---------+--------+
+---+    |      Microcontroller      |             |         
         |      Atmel ATMEGA328      |   +---------+--------+
+------+ |      (with Arduino        |   | 8x8 LED matrix 2 |
|Buzzer+-+       bootloader)         |   | Maxim MAX7219    |
+------+ |                           |   +---------+--------+
         |                           |             |         
         |                           |   +---------+--------+
         +-++----++---------++----++-+   | 8x8 LED matrix 3 |
           ||    ||         ||    ||     | Maxim MAX7219    |
    +------++-+  ||  +------++-+  ||     +---------+--------+
    | Azoteq  |  ||  | Azoteq  |  ||               |         
    | IQS127D |  ||  | IQS127D |  ||     +---------+--------+
    +---------+  ||  +---------+  ||     | 8x8 LED matrix 4 |
                 ||               ||     | Maxim MAX7219    |
          +------++-+      +------++-+   +------------------+
          | Azoteq  |      | Azoteq  |                       
          | IQS127D |      | IQS127D |                       
          +---------+      +---------+                       

(created using http://asciiflow.com/) */


// Includes
#include "LedControl.h" // MAX7219 - LED matrix drivers - http://playground.arduino.cc/Main/LedControl
#include "Chronodot.h"  // DS3231  - Real time clock    - https://github.com/Stephanie-Maks/Arduino-Chronodot
#include <Wire.h>
#include <EEPROM.h> 

// Pins to capacitive touch chips (touch and presence for each of the Azoteq IQS127D chips in the four corners)
const int pinTRB = 9;   // Touch Right Bottom
const int pinTRT = 3;   // Touch Right Top
const int pinTLT = 4;   // Touch Left Top
const int pinTLB = 10;  // Touch Left Bottom
// The presence pins are connected in hardware, but not used in this firmware.
// Leaving the pin numbers in as comments for future reference.
//const int pinPRB = 5; // Presence Right Bottom
//const int pinPRT = 8; // Presence Right Top
//const int pinPLT = 7; // Presence Left Bottom
//const int pinPLB = 6; // Presence Left Top

// Pins to led drivers
const int pinData = 14; // A0 (used as digital pin)
const int pinLoad = 15; // A1 (used as digital pin)
const int pinClock = 16; // A2 (used as digital pin)

// Other pins (buzzer and light sensor)
const int pinBuzzer = 2;
const int pinLDR = 3; // A3 (used as analog pin)

// Constants
const int noBoards = 4; // number of chained LED drivers

// Settings (will be fetched from EEPROM)
int brightness; // Between 0 and 15

// The led controllers (MAX7219)
LedControl lc = LedControl(pinData, pinClock, pinLoad, noBoards);

// The real time clock chip (DS3231)
Chronodot RTC;

// LED grid 
const int bs = 8;
const int bss = 3;
const int boards[2][2] = { {2, 1}, {3, 0} };

// Tasks
const int wait = 10;
const int noTasks = 3;
typedef struct Tasks {
   long unsigned int previous;
   int interval;
   void (*function)();
} Task;
Task tasks[noTasks];

// Serial menu options
boolean mustReadBrightness = false;

// Buffer
boolean prevframe[16][16];

// Words

// Format: { line index, start position index, length }

const int w_it[3] =        { 0,  0,  2 };
const int w_is[3] =        { 0,  3,  2 };
const int w_half[3] =      { 7,  0,  4 }; 
const int w_to[3] =        { 7,  14, 2 };
const int w_past[3] =      { 8,  0,  4 };
const int w_oclock[3] =    { 11, 10, 6 };
const int w_in[3] =        { 12, 0,  2 };
const int w_the[3] =       { 12, 3,  3 };
const int w_afternoon[3] = { 12, 7,  9 };
const int w_noon[3] =      { 12, 12, 4 }; // part of "afternoon"
const int w_midnight[3] =  { 4,  8,  8 };
const int w_morning[3] =   { 13, 0,  7 };
const int w_at[3] =        { 13, 8,  2 };
const int w_night[3] =     { 13, 11, 5 };
const int w_evening[3] =   { 14, 0,  7 };
const int w_and[3] =       { 14, 8,  3 };
const int w_cold[3] =      { 14, 12, 4 };
const int w_cool[3] =      { 15, 0,  4 };
const int w_warm[3] =      { 15, 6,  4 };
const int w_hot[3] =       { 15, 12, 3 };
const int w_el[3] =        { 9,  2,  2 };

const int w_minutes[20][3] = {
  { 0,  13, 3 }, // one
  { 1,  0,  3 }, // two
  { 3,  0,  5 }, // three
  { 2,  12, 4 }, // four
  { 2,  0,  4 }, // five
  { 5,  0,  3 }, // six
  { 6,  0,  5 }, // seven
  { 5,  8,  5 }, // eight
  { 3,  6,  4 }, // nine
  { 1,  4,  3 }, // ten
  { 2,  5,  6 }, // eleven
  { 6,  10, 6 }, // twelve
  { 1,  8,  8 }, // thirteen
  { 4,  0,  8 }, // fourteen
  { 7,  6,  7 }, // quarter
  { 5,  0,  7 }, // sixteen
  { 6,  0,  9 }, // seventeen
  { 5,  8,  8 }, // eighteen
  { 3,  6,  8 }, // nineteen
  { 0,  6,  6 }  // twenty
};

const int w_hours[12][3] = {
  { 8,  5,  3 }, // one
  { 8,  9,  3 }, // two
  { 11, 4,  5 }, // three
  { 9,  7,  4 }, // four
  { 9,  12, 4 }, // five
  { 8,  13, 3 }, // six
  { 10, 0,  5 }, // seven
  { 10, 6,  5 }, // eight
  { 10, 12, 4 }, // nine
  { 11, 0,  3 }, // ten
  { 10, 1,  4 }, // "even"
  { 9,  0,  6 }  // twelve
};

// Touch
boolean tlt;
boolean trt;
boolean tlb;
boolean trb;

void setup() {
  
  // Debug info
  Serial.begin(9600);
  Serial.println("[INFO] Wordclock is booting...");
  
  // Read settings from EEPROM
  Serial.println("[INFO] 1. Read settings");
  brightness = EEPROM.read(0);
  
  // Initiate the LED drivers
  Serial.println("[INFO] 2. LED drivers");
  for(int i = 0; i < noBoards; ++i) {
    lc.shutdown(i, false);
    lc.clearDisplay(i);
  }
  setBrightness(brightness);

  // Initiate the Real Time Clock
  Serial.println("[INFO] 3. Real time clock");
  Wire.begin();
  RTC.begin();
  //if (! RTC.isrunning()) {
  //  Serial.println("[WARNING] RTC is NOT running!");
  //  RTC.adjust(DateTime(__DATE__, __TIME__));
  //}
  
  // Initiate the capacitive touch inputs
  Serial.println("[INFO] 4. Capacitive touch");
  pinMode(pinTRB, INPUT);
  pinMode(pinTRT, INPUT);
  pinMode(pinTLT, INPUT);
  pinMode(pinTLB, INPUT);
  //pinMode(pinPRB, INPUT);
  //pinMode(pinPRT, INPUT);
  //pinMode(pinPLT, INPUT);
  //pinMode(pinPLB, INPUT);
  
  // Tasks
  Serial.println("[INFO] 5. Tasks");
  loadTasks();
  
  // Debug info
  Serial.println("[INFO] Wordclock done booting. Hellooooo!");
  printMenu();
  
}

void loadTasks() {
  
  // Listen for input on the serial interface
  tasks[0].previous = 0;
  tasks[0].interval = 100;
  tasks[0].function = serialMenu;
  
  // Show time
  tasks[1].previous = 0;
  tasks[1].interval = 1000;
  tasks[1].function = showTime;

  // Read the touch inputs
  tasks[2].previous = 0;
  tasks[2].interval = 100;
  tasks[2].function = readTouch;
  
}

void loop() {
  unsigned long time = millis();
  for(int i = 0; i < noTasks; i++) {
    Task task = tasks[i];
    if (time - task.previous > task.interval) {
      tasks[i].previous = time;
      task.function();
    }
  }  
  delay(wait);
}

void serialMenu() {
  if (Serial.available() > 0) {
    if(mustReadBrightness) {
      int val = Serial.parseInt();
      if(val < 0 || val > 15) {
        Serial.println("[ERROR] Brightness must be between 0 and 15");
      } else {
        Serial.print("Brightness set to ");
        Serial.println(val, DEC);
        setBrightness(val);
        brightness = val;
        EEPROM.write(0, val);
      }
      mustReadBrightness = false;
      printMenu();
    } else {
      int in = Serial.read();
      if (in == 49) {
        Serial.println("You entered [1]");
        Serial.println("  Enter brightness (0-15)");
        mustReadBrightness = true;
      } else if (in == 50) {
        Serial.println("You entered [2]");
        Serial.print("  Brightness: ");
        Serial.println(brightness, DEC);
        printMenu();
      } else {
        Serial.println("[ERROR] Whut?");
        printMenu();
      }
    }
  }
}

void showTime() {
  
  // Get the time
  DateTime now = RTC.now();  
  int h = now.hour();
  int h2 = h;
  int m = now.minute();
  int t = now.tempC();
  
  // DEBUG
  /*Serial.print("[DEBUG] ");
  Serial.print(h, DEC);
  Serial.print(':');
  Serial.println(m, DEC);*/
  
  // The frame
  boolean frame[16][16];
  for(int r = 0; r < 16; r++) {
    for(int c = 0; c < 16; c++) {
      frame[r][c] = false;
    }
  }
 
  // Show "IT IS"
  addWordToFrame(w_it, frame);
  addWordToFrame(w_is, frame);
  
  // Minutes
  if (m == 0) {
    
    if (h == 0) {
      addWordToFrame(w_midnight, frame);
    } else if (h == 12) {
      addWordToFrame(w_noon, frame);
    } else {
      addWordToFrame(w_oclock, frame);
    }

  } else {
  
    if (m <= 20) {
      addWordToFrame(w_minutes[m - 1], frame);
    } else if (m < 30) {
      addWordToFrame(w_minutes[19], frame); // twenty
      addWordToFrame(w_minutes[m - 21], frame);
    } else if (m == 30) {
      addWordToFrame(w_half, frame);
    } else if (m < 40) {
      addWordToFrame(w_minutes[19], frame); // twenty
      addWordToFrame(w_minutes[60 - m - 21], frame);
    } else {
      addWordToFrame(w_minutes[60 - m - 1], frame);
    }
 
    if(m <= 30) {
      addWordToFrame(w_past, frame);
    } else {
      addWordToFrame(w_to, frame);
      ++h2;
    }
    
  } 
  
  if(!(m ==0 && (h == 0 || h == 12))) {
  
    // Hours
    if(h2 == 0) {
      addWordToFrame(w_hours[11], frame);
    } else if (h2 <= 12) {
      addWordToFrame(w_hours[h2 - 1], frame);
    } else {
      addWordToFrame(w_hours[h2 - 13], frame);
    }
    if(h2 == 11 || h2 == 23) {
      addWordToFrame(w_el, frame);
    }
  
    // Time of day
    if(h < 6) {
      addWordToFrame(w_at, frame);
      addWordToFrame(w_night, frame);
    } else if(h < 12) {
      addWordToFrame(w_in, frame);
      addWordToFrame(w_the, frame);
      addWordToFrame(w_morning, frame);
    } else if(h < 18) {
      addWordToFrame(w_in, frame);
      addWordToFrame(w_the, frame);
      addWordToFrame(w_afternoon, frame);
    } else {
      addWordToFrame(w_at, frame);
      addWordToFrame(w_night, frame);
    }
    
  }
  
  // Temperature
  addWordToFrame(w_and, frame);
  if(t <= 16) {
    addWordToFrame(w_cold, frame);
  } else if (t <= 20) {
    addWordToFrame(w_cool, frame);
  } else if (t <= 30) {
    addWordToFrame(w_warm, frame);
  } else {
    addWordToFrame(w_hot, frame);
  }

  // Update display
  updateDisplay(prevframe, frame);
  
  // Copy current frame to buffer
  for(int r = 0; r < 16; r++) {
    for(int c = 0; c < 16; c++) {
      prevframe[r][c] = frame[r][c];
    }
  }
  
}

void readTouch() {
  boolean lt = debounce(digitalRead(pinTLT) == LOW, &tlt);
  boolean rt = debounce(digitalRead(pinTRT) == LOW, &trt);
  boolean lb = debounce(digitalRead(pinTLB) == LOW, &tlb);
  boolean rb = debounce(digitalRead(pinTRB) == LOW, &trb);
  if(lt || rt || lb || rb) {
    tone(pinBuzzer, 500, 100);
  }
}

boolean debounce(boolean value, boolean* store) {
  if(value) {
    if(*store) {
      value = false;
    } else {
      *store = true;
    }
  } else {
    *store = false;
  }
  return value;
}

void setAllLeds(boolean on) {
  for(int i = 0; i < noBoards; ++i) {
    for(int r = 0; r < 8; ++r) {
      lc.setRow(i, r, B11111111);
    }
  }
}

void setLed(int row, int col, boolean on) {
  int t = row >> bss;
  int l = col >> bss;
  int board = boards[t][l];
  int r = row % bs;
  int c = col % bs;
  if(t == 0) {
    if(l == 0) {
      r = bs - 1 - r;
      c = (c + 1) % bs;
    } else {
      int r1 = r;
      r = c;
      c = (r1 + 1) % bs;
    }
  } else {
    if(l == 0) {
      int r1 = r;
      r = bs - 1 - c;
      c = (bs - r1) % bs;
    } else {
      c = (bs - c) % bs;
    }
  }
  lc.setLed(board, r, c, on);
}

void updateDisplay(boolean previousframe[16][16], boolean frame[16][16]) {
  for(int r = 0; r < 16; ++r) {
    for(int c = 0; c < 16; ++c) {
      if(prevframe[r][c] && !frame[r][c]) {
        setLed(r, c, false);
      } else if(!prevframe[r][c] && frame[r][c]) {
        setLed(r, c, true);
      }
    }
  }
}

void addWordToFrame(const int theword[3], boolean frame[16][16]){
  for(int i = 0; i < theword[2]; ++i) {
    frame[theword[0]][theword[1] + i] = true;
  }
}

void setBrightness(int value) {
  for(int i = 0; i < noBoards; ++i) {
    lc.setIntensity(i, value);
  }
}

void printMenu() {
  Serial.println("");
  Serial.println("Menu");
  Serial.println("----");
  Serial.println("  1. Set brightness");
  Serial.println("  2. Read brightness");
  Serial.println("");
}
