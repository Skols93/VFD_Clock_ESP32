// Real time digital clock.
// User can set the time with a single button. 
// A short push selects which digits to set (hours, minutes, etc)
// A Long push increments the selection
// Another short push returns to displaying the time as normal
// DS1302 library by Rinky-Dink Electronics, Henning Karlsen. All right reserved
// web: http://www.RinkyDinkElectronics.com/
//
// 

#include <ESP32Time.h>

// Definitions 
//#define TEMPERATURE_PIN  A1 // analog temperature sensor pin
#define BUTTON_PIN  14      // pushbutton pin (this is also analog pin 0, but INTERNAL_PULLUP only works when you refer to this pin as '14')

#define SER_DATA_PIN  17     // serial data pin for VFD-grids shift register zelena
#define SER_LAT_PIN  5      // latch pin for VFD-grids shift register kafena
#define SER_CLK_PIN  18      // clock pin for VFD-grids shift register bela
#define LONG_PUSH_TIME  60  //Time to register a long button press
#define DATE_DELAY 2000     //Delay before date is displayed (loop exectutions)
#define TEMP_DELAY 3000     //Delay before temperature is displayed (loop exectutions)

ESP32Time rtc(7560);  // offset in seconds GMT+1

/*******************************************************************************
 * * The following button-polling routine is written as a macro rather than a
 * function in order to avoid the difficulty of passing a pin address and bit 
 * variables to a function argument.
 * Button is momentary, active low.
 * It recognises a short push or long push depending on how long the button has been
 * held down. This also takes care of debouncing.
 ******************************************************************************/
#define POLL_LONG_BUTTON(button,push_n_hold,short_flag,long_flag)   \
                            if(button){                             \
                                if(push_n_hold < LONG_PUSH_TIME){   \
                                    push_n_hold ++;                 \
                                    if(push_n_hold == LONG_PUSH_TIME){\
                                        long_flag = true;              \
                                        push_n_hold = 0;            \
                                    }                               \
                                }                                   \
                            }                                       \
                            else if(push_n_hold >2 && push_n_hold < LONG_PUSH_TIME){\
                                    short_flag = true;                 \
                                    push_n_hold = 0;                \
                            }                                       \
                            else {                                  \
                                push_n_hold = 0;                    \
                            }                                       \


                            
byte user_selection = 0; //Cycle through user selection for setting the time (0 = set hours, 1 = set minutes, 2 = set seconds, 3 = set date, 4 = set month, 5 = set year)
byte push_n_hold; //A counter to time the length of a button push
boolean long_flag = false; //A long-push button flag
boolean short_flag = false; //A short-push button flag
word loopCounter; //A counter to set the display update rate


/******************************************************************************* 
* Digits are lit by shiting out one byte where each bit corresponds to a grid. 
* 1 = on; 0 = off;
* msb = leftmost digit grid;
* lsb = rightmost digit grid.
*******************************************************************************/

/******************************************************************************* 
* Segments are lit by shiting out one byte where each bit corresponds to a segment:  G-B-F-A-E-C-D-dp 
* --A--
* F   B
* --G--
* E   C
* --D----DP (decimal point)
* 
* 1 = on; 0 = off;
*******************************************************************************/
 byte sevenSeg[12] = {
  B01111110, //'0'
  B01000100, //'1'
  B11011010, //'2'
  B11010110, //'3'
  B11100100, //'4'
  B10110110, //'5'
  B10111110, //'6'
  B01010100, //'7'
  B11111110, //'8'
  B11110110, //'9'
  B11110000, //degrees symbol
  B00111010, //'C'
 };

/******************************************************************************* 
Funtion prototypes
*******************************************************************************/
void updateVFD(int pos, byte num, boolean decPoint);
void displayTemperature();
void clearVFD(void);
void displayTime();
void displayDate();
void pollButton();
void clocksChange();

 
//DS1302 rtc(RTC_RESET_PIN, RTC_DATA_PIN, RTC_SCLK_PIN); // Init the DS1302. Function arguments: RESET, DATA, SCLK,
//Time t; // Init the time-data structure of RTC chip
/*contains:
  uint8_t   hour;
  uint8_t   min;
  uint8_t   sec;
  uint8_t   date;
  uint8_t   mon;
  uint16_t  year;
  uint8_t   dow;
*/
//TM1637Display display(DISPLAY_CLK_PIN, DISPLAY_DIO_PIN); // Init the TM1637 display. Function arguments: CLK, DIO

void setup(){
  Serial.begin(115200);   // Setup Arduino serial connection
  rtc.setTime(30, 24, 15, 28, 10, 2023);  // 17th Jan 2021 15:24:30
//  pinMode(TEMPERATURE_PIN,INPUT);
 
  pinMode(SER_DATA_PIN,OUTPUT);
  pinMode(SER_CLK_PIN,OUTPUT);
  pinMode(SER_LAT_PIN,OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP); //Button Input pin with internal pull-up
//  analogReference(INTERNAL); //Internal 1.1V ADC reference
 
 // rtc.halt(false);    //Set the clock to run-mode
 // rtc.writeProtect(false); //Disable the write protection
  
 
}

void loop(){
      loopCounter++;
      displayTime();
    //  pollButton();
                  
      if(loopCounter == DATE_DELAY){
         displayDate();
        
         Serial.println(" ___");
         Serial.print("It is the year "); //Print stuff for debugging
         Serial.println(rtc.getYear());
         Serial.print("Time: ");
         Serial.print(rtc.getHour());
         Serial.print(" hours, ");
         Serial.print(rtc.getMinute());
         Serial.print(" minutes and ");
         Serial.print(rtc.getSecond());
         Serial.println(" seconds");
         Serial.println(" ___");
      }
  
      if(loopCounter == TEMP_DELAY){
         loopCounter = 0;
       //  displayTemperature();
      }
   
}   //End of main program loop


void updateVFD(int pos, byte num, boolean decPoint){ //This shifts 16 bits out LSB first on the rising edge of the clock, clock idles low. Leftmost byte is position 7-0, rightmost byte is 7-seg digit
    if(pos >= 0 && pos < 9){               //Only accept valid positons on the display
      digitalWrite(SER_CLK_PIN, LOW); //Serial clock for 74HC595
      digitalWrite(SER_DATA_PIN, LOW); //Serial Data for 74HC595
      digitalWrite(SER_LAT_PIN, LOW); //Serial Latch for 74HC595
      num = num + decPoint;                // sets the LSB to turn on the decimal point
      word wordOut = (1 << (pos+8)) + num; // concatenate bytes into a 16-bit word to shift out
      boolean pinState;

        for (byte i=0; i<16; i++){        // once for each bit of data
          digitalWrite(SER_CLK_PIN, LOW);
          if (wordOut & (1<<i)){          // mask the data to see if the bit we want is 1 or 0
            pinState = 1;                 // set to 1 if true
          }
          else{
            pinState = 0; 
          }
                 
          digitalWrite(SER_DATA_PIN, pinState); //Send bit to register
          digitalWrite(SER_CLK_PIN, HIGH);      //register shifts bits on rising edge of clock
          digitalWrite(SER_DATA_PIN, LOW);      //reset the data pin to prevent bleed through
        }
      
        digitalWrite(SER_CLK_PIN, LOW);
        digitalWrite(SER_LAT_PIN, HIGH); //Latch the word to light the VFD
        delay(2); //This delay slows down the multiplexing to get get the brightest display (but too long causes flickering)
      
      clearVFD();
    }
} 

void clearVFD(void){
    digitalWrite(SER_DATA_PIN, LOW);          //clear data and latch pins
    digitalWrite(SER_LAT_PIN, LOW);
        for (byte i=0; i<16; i++){            //once for each bit of data
            digitalWrite(SER_CLK_PIN, LOW);
            digitalWrite(SER_CLK_PIN, HIGH);  //register shifts bits on rising edge of clock
            }
    digitalWrite(SER_CLK_PIN, LOW);
    digitalWrite(SER_LAT_PIN, HIGH);          //Latch the word to update the VFD
}

void displayTemperature(){
 // #ifdef __cplusplus
//   extern "C" {
//   #endif
//   uint8_t temprature_sens_read();
//   #ifdef __cplusplus
//   }
//   #endif

// uint8_t temprature_sens_read();

 
    // word adcValue = (temprature_sens_read() - 32) / 1.8;  //get analog reading
    // adcValue = (temprature_sens_read() - 32) / 1.8;  //read twice to avoid glitches
    // Serial.println(adcValue, DEC);                       //For debugging
    //                  //Subtract MCP9107 zero offset (remember ADC uses 1.1V internal reference)
    // word temperature = adcValue ;/// 2.3;           //Scale to produce a three digit number equal to temperature*10
  
    // byte hundreds = (temperature / 100);
    // byte tens = (temperature / 10) % 10;
    // byte units = (temperature % 100) % 10;

    // for (int i = 0; i<11; i++){    //scroll the display across the VFD
    //   for(word j = 0; j<14; j++){  //j count determines the speed of scrolling
    //     if(hundreds > 0){          //don't display first digit if 0, to avoid 00.0 
    //       updateVFD(i-3, sevenSeg[hundreds], false);
    //     }
    //     updateVFD(i-5, sevenSeg[units], false);
    //     updateVFD(i-6, sevenSeg[10], false);  //degrees symbol
    //     updateVFD(i-7, sevenSeg[11], false);  //'C'
    //     updateVFD(i-4, sevenSeg[tens], true); //With decimal point
    //   }
    // }
    
    // for(word j = 0; j<350; j++){   //Hold the display for a short time
    //     if(hundreds > 0){          //don't display first digit if 0, to avoid 00.0 
    //       updateVFD(7, sevenSeg[hundreds], false);
    //     }
    //     updateVFD(5, sevenSeg[units], false);
    //     updateVFD(4, sevenSeg[10], false);  //degrees symbol
    //     updateVFD(3, sevenSeg[11], false);  //'C'
    //     updateVFD(6, sevenSeg[tens], true); //With decimal point
    // }
}

void displayTime(){
    //t = rtc.getTime();   // Get time from the DS1302
    byte tensHour = rtc.getHour() / 10; //Extract the individual digits
    byte unitsHour = rtc.getHour() % 10;
    byte tensMin = rtc.getMinute() / 10;
    byte unitsMin = rtc.getMinute() % 10;
    byte tensSec = rtc.getSecond() / 10;
    byte unitsSec = rtc.getSecond() % 10;
    
    updateVFD(0, sevenSeg[tensHour], false);  // updateVFD()
    updateVFD(1, sevenSeg[unitsHour], false);
   
    updateVFD(2, sevenSeg[tensMin], false);  
    updateVFD(3, sevenSeg[unitsMin], false);
   
}

void displayDate(){
    //t = rtc.getTime();   // Get time from the DS1302 
    byte tensDate = rtc.getDay() / 10; //Extract the individual digits
    byte unitsDate = rtc.getDay() % 10;
    byte tensMon = rtc.getMonth() / 10;
    byte unitsMon = rtc.getMonth() % 10;
    byte thousandsYear = (rtc.getYear() / 1000);
    byte hundredsYear = (rtc.getYear() /100) % 10;
    byte tensYear = (rtc.getYear() / 10) % 10;
    byte unitsYear = (rtc.getYear() % 100) % 10;

    for (int i = 0; i<11; i++){                       //scroll the display across the VFD
      for(word j = 0; j<14; j++){                      //j count determines the speed of scrolling
        updateVFD(i-3, sevenSeg[tensDate], false); 
        updateVFD(i-4, sevenSeg[unitsDate], true);   //with decimal point 
        updateVFD(i-5, sevenSeg[tensMon], false);  
        updateVFD(i-6, sevenSeg[unitsMon], true);    //with decimal point
        updateVFD(i-7, sevenSeg[thousandsYear], false); 
        updateVFD(i-8, sevenSeg[hundredsYear], false); 
        updateVFD(i-9, sevenSeg[tensYear], false);  
        updateVFD(i-10, sevenSeg[unitsYear], false);
      }
    }
    
    for(word j = 0; j<3000; j++){   //Hold the display for a short time
        updateVFD(0, sevenSeg[tensDate], false); 
        updateVFD(1, sevenSeg[unitsDate], true);   //with decimal point 
        updateVFD(2, sevenSeg[tensMon], false); 
        updateVFD(3, sevenSeg[unitsMon], true);    //with decimal point
        
    }

    for(word j = 0; j<3000; j++){   //Hold the display for a short time
        updateVFD(0, sevenSeg[thousandsYear], false); 
        updateVFD(1, sevenSeg[hundredsYear], false); 
        updateVFD(2, sevenSeg[tensYear], false);  
        updateVFD(3, sevenSeg[unitsYear], false);
    }

}


void pollButton(){
  POLL_LONG_BUTTON(!digitalRead(BUTTON_PIN), push_n_hold, short_flag, long_flag) 
  //The following big if-statement contains the user time-setting stuff.
  //Press the button once to access time-setting mode; the current selection will be displayed.
  //Press and hold the button to increment the display.
  //Press once more to exit time-setting mode.
  //The display cycles through hr, min, sec, date, month, year, each time the mode is accessed.
  if(short_flag){ //if button has been pressed
    short_flag = false; //reset flag
    boolean button_reset = true;//Flag to say button must be released before a short push will again be recognised
    boolean set_new_time = true; //we have entered time-setting mode
    void clearVFD();
    int i[6]; //An array to store new user-set time values
         
    while(set_new_time){
      Serial.println(" ___");
      Serial.print(i[user_selection]); //Print stuff for debugging
      Serial.print("  "); //Print stuff for debugging
      Serial.print(rtc.getYear()); //Print stuff for debugging
       // t = rtc.getTime();// Get data from the DS1302
        i[0] = rtc.getHour(); //Values to be stored in the array
        i[1] = rtc.getMinute();
        i[2] = rtc.getSecond();
        i[3] = rtc.getDay(); 
        i[4] = rtc.getMonth();
        i[5] = rtc.getYear();
        byte tens = i[user_selection] / 10; //Extract the individual digits
        byte units = i[user_selection] % 10;
        byte thousandsYear = (i[user_selection] / 1000);
        byte hundredsYear = (i[user_selection] /100) % 10;
        byte tensYear = (i[user_selection] / 10) % 10;
        byte unitsYear = (i[user_selection] % 100) % 10;

        switch (user_selection) {
           case 0: //Display hours
              updateVFD(7, sevenSeg[tens], false);  
              updateVFD(6, sevenSeg[units], false);
              break;
           case 1: //Display mins
              updateVFD(4, sevenSeg[tens], false);  
              updateVFD(3, sevenSeg[units], false);
              break;
           case 2: //Display seconds
              updateVFD(1, sevenSeg[tens], false);  
              updateVFD(0, sevenSeg[units], false);
              break;
           case 3:  //Display date
              updateVFD(7, sevenSeg[tens], false);  
              updateVFD(6, sevenSeg[units], true);
              break;
           case 4: //Display month
              updateVFD(5, sevenSeg[tens], false);  
              updateVFD(4, sevenSeg[units], true);
              break;
           case 5:  //Display year
              updateVFD(3, sevenSeg[thousandsYear], false); 
              updateVFD(2, sevenSeg[hundredsYear], false); 
              updateVFD(1, sevenSeg[tensYear], false);  
              updateVFD(0, sevenSeg[unitsYear], false);
              break;
       }
            
        POLL_LONG_BUTTON(!digitalRead(BUTTON_PIN),push_n_hold,short_flag,long_flag)
          if(long_flag){ //A long push will increment the time
               
               i[user_selection]++; //Increment the time element
               if(i[0] > 23){ //hours cannot exceed 23
                  i[0] = 0;
               }  
               if(i[1] > 59){ //minutes cannot exceed 59
                  i[1] = 0;
               }
               if(i[2] > 59){ //seconds cannot exceed 59
                  i[2] = 0;
               }
               if(i[3] > 31){ //date cannot exceed 31
                  i[3] = 1;
               }  
               if(i[4] > 12){ //months cannot exceed 12
                  i[4] = 1;
               }                                                     
               if(i[5] > 3000){ //years cannot exceed 3000
                  i[5] = 2018;
               }               
              //  rtc.setTime(i[0], i[1], i[2]); //Send the new time or date to the RTC
              //  rtc.setDate(i[3], i[4], i[5]);
               long_flag = false; //Long button push has been duly serviced
               button_reset = true;//Don't accept a short push until the button has actually been released
          }
  
          if(short_flag){
              if(button_reset){//If button has been pressed for a long time previously and has just now been released, 
                  button_reset = false; //any short push glitch will be ignored.
              }
              else{//Otherwise it is a genuine short push
                  set_new_time = false;//exit time-setting mode
              }
            short_flag = false; //Short button push has been duly serviced
          }
      }
    //User has finished setting the time      
    user_selection ++; //Decrement the user selection by 2 ready for next request
      if(user_selection > 5){//if user has cycled through all options,
        user_selection = 0; //reset user selection
      }
  }
}

//  void clocksChange(){
//   if(t.mon == 3 && t.date > 24 && t.dow == 7){            //If it is the last Sunday in March
//         if(t.hour == 1){                                  //If it is 1am
//           rtc.setTime(2, t.min, t.sec);                   //Time goes forward by one hour
//           rtc.poke(0, 0x00);                              //Store a byte in RTC RAM location 0. Set to 0x00 for BST (British summer time).
//         }
//       }
      
//       else if(t.mon == 10 && t.date > 24 && t.dow == 7){  //If it is the last Sunday in October
//         if(t.hour == 2 && rtc.peek(0) == 0x00){           //If it is 2am and the byte stored in RTC RAM location 0 is zero (British Summer Time)
//           rtc.setTime(1, t.min, t.sec);                   //Time goes back by one hour
//           rtc.poke(0, 0xFF);                              //Overwrite stored byte; it is now GMT (winter time). This stops the time being set back again at 2am.
//         }
//       }
//  }

        
    
  


   