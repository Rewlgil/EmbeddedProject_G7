#include <Wire.h>
#include <SPI.h>  // not used here, but needed to prevent a RTClib compile error
#include <RTClib.h>

RTC_DS3231 RTC;

void setup () {
    Serial.begin(9600);
    Wire.begin();
    RTC.begin();

  RTC.adjust(DateTime(__DATE__, __TIME__));
  if (! RTC.isrunning()) {
    Serial.println("RTC is NOT running!");
   RTC.adjust(DateTime(__DATE__, __TIME__));  //ตั้งดึงเวลาจากคอมพิวเตอร์มา Save ลงใน Module RTC
  }

  DateTime now = RTC.now();
  RTC.setAlarm1Simple(21, 58);  //เป็นการตั้งเวลาให้ทำงานเวลา 21.58 น.
  RTC.turnOnAlarm(1);
  if (RTC.checkAlarmEnabled(1)) {
    Serial.println("Alarm Enabled");
  }
}

void loop () {
    DateTime now = RTC.now();
    Serial.print(now.year(), DEC);
    Serial.print('/');
    Serial.print(now.month(), DEC);
    Serial.print('/');
    Serial.print(now.day(), DEC);
    Serial.print(' ');
    Serial.print(now.hour(), DEC);
    Serial.print(':');
    Serial.print(now.minute(), DEC);
    Serial.print(':');
    Serial.print(now.second(), DEC);
    Serial.println();

    if (RTC.checkIfAlarm(1)) {  //ในส่วนนี้เมื่อถึงเวลาที่ตั้งไว้ให้ทำงานอะไร สามารถตั้งการทำงานได้เลย
      Serial.println("Alarm Triggered");
    }

    Serial.print("Tempeature = ");
    Serial.print(RTC.getTemperature()); // สามารถดึงอุณหภูมิของ RTC มาแสดงได้อีกด้วย
    Serial.println(" C");

    Serial.println();
    delay(1000);
}