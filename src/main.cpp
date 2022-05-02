// Drug Timer
// This program objective is to alert patient to take drug
// on time and dose that are programmed
// This code is part of Final Project 2102444 Introduction to Embedded Systems
// semaster2 2021 Department of Electrical Engineering, Chulalongkorn University
// Author:  Siraphop Vespaiboon           6230553021
//          Jirawat Wongyai               6230069721
//          Wasawat Ratthapornsupphawat   6230480921
//          Rungsiman Kulpetjira          6232026021
// Created on 30 April 2022

#include <Arduino.h>
#include <DHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>
#define USE_TIMER_3 true
#include <TimerInterrupt.h>
#include <PinChangeInterrupt.h>

// name of drug to take
const String name[] = {"Paracetamol", "Andrographis", "Dextromethorphan", "Chlorpheniramine", "Fexofenadine", " ", " ", " ", " ", " "};

// dose of each drug
const uint8_t no[] = {2, 1, 1, 2, 2, 0, 0, 0, 0, 0};

// time to take each drug
const uint16_t time[10][3] = { {900, 1500, 0},
                               {800, 1200, 1637},
                               {800, 1637},
                               {800, 1200, 1637},
                               {1200, 0, 0},
                               {0, 0, 0},
                               {0, 0, 0},
                               {0, 0, 0},
                               {0, 0, 0},
                               {0, 0, 0} };

#define dhtPin 2
DHT dht;

LiquidCrystal_I2C lcd(0x27, 16, 2);

RTC_DS3231 RTC;
DateTime now;

volatile uint32_t lastPress = 0;  // stored last pressed millis() for debounce
volatile uint8_t activeRow = 0;   // stored reading Row of keypad
volatile char key = ' ';          // stored last pressed key

uint16_t alarm = 0;               // alarm flag
uint32_t checkAlarmTime = 0;      // store last alarm check millis()

uint32_t setScreenTime = 0;       // store last update screen millis()
uint32_t startTime = 0;
uint32_t updateTime = 0;          // store last update RTC millis()

uint8_t page = 0, scroll = 0;     // variable for store current display page
uint8_t select = 0;               // store drug user select for view
bool start = false;               // flag for first time running

void print2digits(int number);
void displayDate();
void displayTime();
void displayTemp();
void Timer3ISR();
void PCINT_ISR();
void viewDetail(int index);
void checkAlarm();

void setup() 
{
  Serial.begin(9600);

  dht.setup(dhtPin);    // initialize DHT sensor

  Wire.begin();
  lcd.init();           // initialize the lcd 
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Temp:");
  lcd.setCursor(8, 0);
  lcd.print("Humid:");
  lcd.setCursor(0, 1);
  lcd.print("Date:");

  RTC.begin();          // initialize RTC module
  // RTC.adjust(DateTime(__DATE__, __TIME__));    // synce time with computer

  ITimer3.init();   // timer 3 for 7-segment display and keypad scan
  ITimer3.attachInterruptInterval(5, Timer3ISR);  // attach interrupt with interval 5 mS

  DDRA = 0xFF;    // set PORTA output for 7 segment pin
  DDRC = 0xF0;    // set PORTC output for 7 segment digit

  DDRH  = 0x78;   // set PORTH output 4 pin for keypad
  DDRB  = 0x00;   // input pullup for keypad
  PORTB = 0xF0;
  attachPCINT(digitalPinToPCINT(10), PCINT_ISR, FALLING);   // attach Pin Change Interrupt
  attachPCINT(digitalPinToPCINT(11), PCINT_ISR, FALLING);   // for each column of key pad
  attachPCINT(digitalPinToPCINT(12), PCINT_ISR, FALLING);   // (each pin are set to input
  attachPCINT(digitalPinToPCINT(13), PCINT_ISR, FALLING);   //  pullup)
}

void loop() 
{
  if (page == 0 && key == '1')    // change screen from home screen
  {                               // to drug list menu
    key = ' ';
    start = true;
    page = 1; scroll = 1;
    lcd.clear();
    lcd.setCursor(0 ,0);  lcd.print("Drug Lists");
    delay(2000);
  }

  if (page == 1)
  {
    // if user pressed up or down key or it's the first time scroll the menu
    if (key == 'D' || key == 'U' || start == true)        
    {
      if (key == 'D' && scroll <= 7)        scroll += 2;
      else if (key == 'U' && scroll >= 2)   scroll -= 2;
      key = ' ';
      lcd.clear();                            // display drug list
      lcd.setCursor(0, 0);
      lcd.print(scroll);  lcd.print(".");
      lcd.print(name[scroll - 1]);

      lcd.setCursor(0, 1);
      lcd.print(scroll + 1);  lcd.print(".");
      lcd.print(name[scroll]);
      start = false;
    }
    // if user press any number on keypad display detail of drug
    else if ((int)key - 48 > 0 && (int)key - 48 < 10)
    {
      select = (int)key - 48;
      key = ' ';
      viewDetail(select);
    }
  }

  if (page == 2)
  {
    viewDetail(select);
  }
  
  if (page != 0 && key == 'H')   // if press H return to home screen
  {
    key = ' ';
    page = 0;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Temp:");
    lcd.setCursor(8, 0);
    lcd.print("Humid:");
    lcd.setCursor(0, 1);
    lcd.print("Date:");
    displayDate();
    displayTemp();
    setScreenTime = millis();
  }
  // update date, temperatue and humidity every 2s while in home screen
  if (millis() - setScreenTime > 2000 && page == 0)
  {
    displayDate();
    displayTemp();
    setScreenTime = millis();
  }
  
  // update time on 7-segment every 1s
  if ((millis() - updateTime) > 1000)
  {
    displayTime();
    updateTime = millis();
  }
  
  // check alarm every 1 min
  if ((millis() - checkAlarmTime) > 60000)
  {
    displayTime();
    checkAlarm();
    if (alarm != 0)
    {
      key = ' ';
      while (key != 'H')
      {
        // alert
        if (key != 'K') tone(3, 1000);
        for (uint8_t i = 0; i < 16; i++)  // read alarm flag
        {

          if (((1 << i) & alarm))
          { 
            page = 3;
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print(i+1);   lcd.print(".");
            lcd.print(name[i]);                   // display drug name
            lcd.setCursor(0, 1);
            lcd.print("#");   lcd.print(no[i]);   // display number of
            startTime = millis();
          }
          while((millis() - startTime) < 1000)
          {
            if      (key == 'K')    noTone(3);  
            else if (key == 'H')  { noTone(3);  break; }
          }
          if (key == 'H')   break;
        }
      }
      alarm = 0;
    }
    checkAlarmTime = millis();
  }
}

void print2digits(int number)
{
  if (number >= 0 && number < 10)
    lcd.print('0');
  lcd.print(number, DEC);
} 

// update date on LCD
void displayDate()
{
  now = RTC.now();

  lcd.setCursor(6, 1);
  print2digits(now.day());
  lcd.print('/');
  print2digits(now.month());
  lcd.print('/');
  lcd.print(now.year(), DEC);
}

/*
store segment digit for display on 7-segment
and decimal place state for blink every second
*/
volatile uint8_t segmentDigit[] = {0,0,0,0};
volatile bool segmentDP = LOW;

// update time on 7-segment
void displayTime()
{
  now = RTC.now();
  // 7-segment digit
  const uint8_t segment[] = {0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f};
  segmentDigit[0] = segment[now.minute() % 10];
  segmentDigit[1] = segment[now.minute() /10];
  segmentDigit[2] = segment[now.hour() % 10];
  segmentDigit[3] = segment[now.hour() / 10];
  segmentDP = (now.second() % 2);
}

// display temperature and humidity on LCD
void displayTemp()
{
  float temp = dht.getTemperature();  // read temperature from DHT sensor
  float humid = dht.getHumidity();    // read humidity from DHT sensor

  lcd.setCursor(5, 0);                // display temperature and humidity on LCD
  lcd.print(int(round(temp)));
  lcd.setCursor(14, 0);
  lcd.print(int(round(humid)));
}

void Timer3ISR()  // update 7-segment & scan keypad
{
  // update 7 segment
  static uint8_t digit = 3;

  if (digit == 3)   digit =  0;       // Loop display through 4 digit
  else              digit++;

  PORTC = (1 << (digit + 4)) & 0xF0;  // select digit to display
  PORTA = ~segmentDigit[digit];       // common anode 7-segment

  if (digit == 2)                     // blink digit 2 DP
  {
    if (segmentDP == 1)   PORTA |= 0x80;  // turn on  digit 2 DP
    else                  PORTA &= 0x7F;  // turn off digit 2 DP
  }

  // scan key pad
  if (activeRow == 3)   activeRow = 0;      // Loop scan through 4 row
  else                  activeRow++;        // of keypad

  PORTH = (~(1 << (activeRow + 3))) & 0x78; // set each Row to LOW
}

/*
  Pin Change Interrupt Subroutine. This Function will be called
  Every time Keypad button are pressed for reading pressed key
*/
void PCINT_ISR()
{
  char botton[4][4] = { {'1', '2', '3', 'U'},
                      {'4', '5', '6', 'D'},
                      {'7', '8', '9', 'K'},
                      {'-', '0', '-', 'H'} };

  uint8_t col = 1;    // store column of keypad which pressed
  uint8_t colPress = ((~PINB) >> 4) & 0x0F; // read keypad column
  if (millis() - lastPress > 300)           // debounce keypad switch
  {
    for (uint8_t i = 0; i < 4; i++)         // convert keypad column that
    {                                       // are pressed to column index
      if (col == colPress)  { col = i; break; }
      col = col << 1;
    }
    key = botton[activeRow][col];           // translate botton pressed to key
    lastPress = millis();                   // stored last pressed millis() for debounce
    Serial.println(key);                    // print key pressed for debugging
  }
}

/*
  This function will be call contineusly while user are in
  view detail menu to display drugname, dose and time to take
*/
void viewDetail(int index)
{
  static uint8_t i = 0;   // variable to store time display on screen

  if (page != 2)          // these set of text will be print only in
  {                       // first time when screen change to this page
    page = 2;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(index);  lcd.print(".");
    lcd.print(name[index-1]);
    lcd.setCursor(0, 1);
    lcd.print("#");       lcd.print(no[index-1]);
    lcd.setCursor(3, 1);
    lcd.print("Time ");
    startTime = millis();
    i = 0;
  }
  
  // Display times to take this drug
  if (time[index-1][i] != 0 && (millis() - startTime) > 1000)
  {
    lcd.setCursor(8, 1);
    lcd.print(i + 1);   lcd.print(". ");
    lcd.setCursor(11, 1);
    print2digits(time[index-1][i] / 100);
    lcd.print(":");
    print2digits(time[index-1][i] % 100); 
    if (++i > 2)  i = 0;
    startTime = millis();
  }
}

// check if it's time to take any drug
void checkAlarm()
{
  if (alarm == 0)
  {
    now = RTC.now();

    for (uint8_t i = 0; i < 10; i++)
    {
      for (uint8_t j = 0; j < 3; j++)
      {
        if (time[i][j] == 0) {}
        else if ( (time[i][j] / 100) == now.hour() && 
                  (time[i][j] % 100) == now.minute())
        {
          alarm |= (1 << i);    // set alarm flag
        }
      }
    }

    if (alarm != 0)
    {
      Serial.println("Alarm!!!");
      Serial.println(alarm, BIN);
    }
  }
}
