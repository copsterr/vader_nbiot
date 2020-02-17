#include "AIS_NB_BC95.h"
#include "nbiot_credentials.h"    // credentials for connecting to the server
#include "PMS.h"
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <RtcDS1307.h>


/* Function Prototypes ---------- */
void (* resetFunc)(void) = 0;
void resetArduino(void);

/* Private Variables ---------- */
// NB-IoT Vars
AIS_NB_BC95 AISnb;
uint16_t sendingInterval = 10000;
uint32_t prevMillis = 0;

// PMS Vars
PMS pms(Serial1); // init Serial1 for PMS Module
PMS::DATA pmsData;

// LCD Vars
// Set the LCD address to 0x27 for a 16 chars and 2 line display
LiquidCrystal_I2C lcd(0x27, 16, 2);

// RTC Vars
RtcDS1307<TwoWire> Rtc(Wire);


void setup() {
  Serial.begin(9600);
  Serial1.begin(9600);

/* Setup LCD -------------------- */
// initialize the LCD
  lcd.begin();

  // Turn on the blacklight and print a message.
  lcd.backlight();
  lcd.print("Starting NB-IoT...");

  // Start RTC
  Rtc.Begin();
  RtcDateTime now = Rtc.GetDateTime();

/* Setup NB-IoT Shield ---------- */
  AISnb.debug = true; // Enable debugging
  AISnb.setupDevice(SERVER_PORT);
  
  // get device IP
  String devIP = AISnb.getDeviceIP();
  
  // get ping response
  pingRESP pingResp = AISnb.pingIP(SERVER_IP);

  lcd.clear();
  lcd.print("Completed.");
  delay(1000);
  lcd.clear();

}


void loop() {
  if (pms.read(pmsData)) {
    Serial.print("Current Time: ");
    RtcDateTime now = Rtc.GetDateTime();
    printDateTime(now);  Serial.println();

    char timePayload[80] = {0};
    sprintf(timePayload, "%02u/%02u/%04u %02u:%02u:%02u",
            now.Month(),
            now.Day(),
            now.Year(),
            now.Hour(),
            now.Minute(),
            now.Second());

    String payload = "{\"type\":\"PM\",\"timestamp\":" + String(timePayload) + \
                     ",\"PM1.0\":" + String(pmsData.PM_AE_UG_1_0) + \
                     ",\"PM2.5\":" + String(pmsData.PM_AE_UG_2_5) + \
                     ",\"PM10.0\":" + String(pmsData.PM_AE_UG_10_0) +"}";
   
    lcd.clear(); lcd.setCursor(0, 0);
    lcd.print("PM1.0:" + String(pmsData.PM_AE_UG_1_0) + " PM2.5:" + String(pmsData.PM_AE_UG_2_5));
    lcd.setCursor(0, 1);
    lcd.print("PM10.0:" + String(pmsData.PM_AE_UG_10_0));

    UDPSend udp = AISnb.sendUDPmsgStr(SERVER_IP, SERVER_PORT, payload);
    UDPReceive resp = AISnb.waitResponse();
  }

}


/* User Defined Functions ---------- */
void resetArduino(void)
{
  /*
    Function used for resetting Arduino
  */
  Serial.println("*** ---------- Resetting Arduino ---------- ***");
  resetFunc();
}

#define countof(a) (sizeof(a) / sizeof(a[0]))

void printDateTime(const RtcDateTime& dt)
{
    char datestring[20];

    snprintf_P(datestring, 
            countof(datestring),
            PSTR("%02u/%02u/%04u %02u:%02u:%02u"),
            dt.Month(),
            dt.Day(),
            dt.Year(),
            dt.Hour(),
            dt.Minute(),
            dt.Second() );
    Serial.print(datestring);
}
