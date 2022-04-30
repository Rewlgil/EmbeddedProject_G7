#include <Arduino.h>
#include <DHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>
#define USE_TIMER_3 true
#include <TimerInterrupt.h>
#include <PinChangeInterrupt.h>

const String name[] = {"Paracetamol", "Andrographis", "Dextromethorphan", "Chlorpheniramine", "Fexofenadine", "Rew", "Siraphop", "Vespaiboon", "Focus", "Jirawat"};
const uint8_t no[] = {4, 5, 6, 7, 8, 9, 2, 3, 1, 7};
const uint16_t time[10][3] = { {900, 1500, 0}, 
                               {800, 1200, 1800},
                               {800, 1800},
                               {800, 1200, 1800},
                               {1200, 0, 0},
                               {0, 0, 0},
                               {0, 0, 0},
                               {0, 0, 0},
                               {0, 0, 0},
                               {0, 0, 0} };

#define dhtPin 2
DHT dht;

LiquidCrystal_I2C lcd(0x27, 16, 2);
uint32_t setScreenTime = 0;

RTC_DS3231 RTC;
DateTime now;

volatile uint32_t lastPress = 0;
volatile uint8_t activeRow = 0;
char key = ' ';

void print2digits(int number);
void displayDateTime();
void displayTemp();
void Timer3ISR();
void PCINT_ISR();
void viewDetail(int index);
void editDetail(int index);

void setup() 
{
  Serial.begin(9600);

  dht.setup(dhtPin);

  Wire.begin();
  lcd.init();       // initialize the lcd 
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Temp:");
  lcd.setCursor(8, 0);
  lcd.print("Humid:");
  lcd.setCursor(0, 1);
  lcd.print("Date:");

  RTC.begin();
  RTC.adjust(DateTime(__DATE__, __TIME__)); 

  ITimer3.init();   // timer 3 for 7-segment display and keypad scan
  ITimer3.attachInterruptInterval(5, Timer3ISR);

  DDRA = 0xFF;    // set PORTA output for 7 segment pin
  DDRC = 0xF0;    // set PORTC output for 7 segment digit

  DDRH  = 0x78;   // set PORTH output 4 pin for keypad
  DDRB  = 0x00;   // input pullup for keypad
  PORTB = 0xF0;
  attachPCINT(digitalPinToPCINT(10), PCINT_ISR, FALLING);
  attachPCINT(digitalPinToPCINT(11), PCINT_ISR, FALLING);
  attachPCINT(digitalPinToPCINT(12), PCINT_ISR, FALLING);
  attachPCINT(digitalPinToPCINT(13), PCINT_ISR, FALLING);
}

uint8_t page = 0, scroll = 0;

uint8_t select = 0;
void loop() 
{
  if (page == 0 && key == '1')
  {
    key = ' ';
    page = 1; scroll = 0;
    lcd.clear();
    lcd.setCursor(0 ,0);  lcd.print("Drug Lists");
    delay(2000);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(scroll+1);  lcd.print(".");
    lcd.print(name[scroll]);

    lcd.setCursor(0, 1);
    lcd.print(scroll + 2);  lcd.print(".");
    lcd.print(name[scroll+1]);

    while (page == 1)
    {
      if (key == 'D' && scroll <= 7)      // move down list
      {
        key = ' ';
        scroll += 2;
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(scroll+1);  lcd.print(".");
        lcd.print(name[scroll]);

        lcd.setCursor(0, 1);
        lcd.print(scroll + 2);  lcd.print(".");
        lcd.print(name[scroll+1]);
      }
      else if (key == 'U' && scroll >= 2)
      {
        key = ' ';
        scroll -= 2;
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(scroll+1);  lcd.print(".");
        lcd.print(name[scroll]);

        lcd.setCursor(0, 1);
        lcd.print(scroll + 2);  lcd.print(".");
        lcd.print(name[scroll+1]);
      }
      else if ((int)key - 48 > 0 && (int)key - 48 < 10)
      {
        select = (int)key - 48;
        key = ' ';
      
        viewDetail(select);
          
      }
      else if (key == 'H')
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
      }
    }
  }

  if (millis() - setScreenTime > 2000)
  {
    displayDateTime();
    displayTemp();
    setScreenTime = millis();
  }  
}

void print2digits(int number)
{
  if (number >= 0 && number < 10)
    lcd.print('0');
  lcd.print(number, DEC);
} 

uint8_t segmentDigit[] = {0,0,0,0};
bool segmentDP = LOW;

void displayDateTime()
{
  now = RTC.now();

  lcd.setCursor(6, 1);
  print2digits(now.day());
  lcd.print('/');
  print2digits(now.month());
  lcd.print('/');
  lcd.print(now.year(), DEC);

  const uint8_t segment[] = {0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f};
  segmentDigit[0] = segment[now.minute() % 10];
  segmentDigit[1] = segment[now.minute() /10];
  segmentDigit[2] = segment[now.hour() % 10];
  segmentDigit[3] = segment[now.hour() / 10];
  segmentDP = (now.second() % 2);
}

void displayTemp()
{
  float temp = dht.getTemperature();
  float humid = dht.getHumidity();

  lcd.setCursor(5, 0);
  lcd.print(int(round(temp)));
  lcd.setCursor(14, 0);
  lcd.print(int(round(humid)));
}

void Timer3ISR()  // update 7segment & scan keypad
{
  // update 7 segment
  static uint8_t digit = 3;

  if (digit == 3)   digit =  0;
  else              digit++;

  PORTC = (1 << (digit + 4)) & 0xF0;
  PORTA = ~segmentDigit[digit];

  if (digit == 2)
  {
    if (segmentDP == 1)   PORTA |= 0x80;
    else                  PORTA &= 0x7F;
  }

  // scan key pad
  if (activeRow == 3)   activeRow = 0;
  else                  activeRow++;

  PORTH = (~(1 << (activeRow + 3))) & 0x78;
}

char botton[4][4] = { {'1', '2', '3', 'U'},
                      {'4', '5', '6', 'D'},
                      {'7', '8', '9', 'K'},
                      {'-', '0', '-', 'H'} };

void PCINT_ISR()
{
  uint8_t col = 1, colPress = ((~PINB) >> 4) & 0x0F;
  if (millis() - lastPress > 300)
  {
    for (uint8_t i = 0; i < 4; i++)
    {
      if (col == colPress)  { col = i; break; }
      col = col << 1;
    }
    key = botton[activeRow][col];
    lastPress = millis();
    Serial.println(key);
  }
}

void viewDetail(int index)
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(index);  lcd.print(".");
  lcd.print(name[index-1]);
  lcd.setCursor(0, 1);
  lcd.print("#");       lcd.print(no[index-1]);
  lcd.setCursor(3, 1);
  lcd.print("Time ");
  uint8_t i = 0;
  while (key == ' ')
  {
    do
    {
      if (++i > 2)  i = 0;
      delay(1000);
    }
    while (time[index-1][i] == 0);

    lcd.setCursor(8, 1);
    lcd.print(i + 1);   lcd.print(". ");
    lcd.setCursor(11, 1);
    print2digits(time[index-1][i] / 100);
    lcd.print(":");
    print2digits(time[index-1][i] % 100);
    delay(1000);
  }
}