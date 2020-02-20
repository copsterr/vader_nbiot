#include "nbiot_credentials.h"    // credentials for connecting to the server
#include "AIS_NB_BC95.h"
#include "PMS.h"
#include "device_config.h"
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <RtcDS1307.h>
#include <TinyGPS++.h>


/* Function Prototypes ---------- */
void (* resetFunc)(void) = 0;
void resetArduino(void);

/* Private Variables ---------- */
// NB-IoT Vars
AIS_NB_BC95 AISnb;
uint32_t prevMillis = 0; // used for dutycycle data transmitting
pingRESP pingResp;

// PMS Vars
PMS pms(Serial1); // init Serial1 for PMS Module
PMS::DATA pmsData;

// LCD Vars
LiquidCrystal_I2C lcd(0x27, 16, 2); // Set the LCD address to 0x27 for a 16 chars and 2 line display

// RTC Vars
RtcDS1307<TwoWire> Rtc(Wire);

// GPS Vars
TinyGPSPlus gps;
String gpsLat_payload = "";
String gpsLng_payload = "";


void setup() {
/* Setup Serial Ports ----------  */
  Serial.begin(VC_BAUD);     // Virtual COM
  Serial1.begin(PMS_BAUD);   // Dust Sensor
  Serial2.begin(GPS_BAUD);   // GPS

#if USE_LCD
/* Setup LCD -------------------- */
  // initialize the LCD
  lcd.begin();

  // Turn on the blacklight and print a message.
  lcd.backlight();
  lcd.print("Starting NB-IoT...");
#endif

/* Setup RTC -------------------- */
  // Start RTC
  Rtc.Begin();
  RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
  Rtc.SetDateTime(compiled);
  RtcDateTime now = Rtc.GetDateTime();

/* Setup NB-IoT Shield ---------- */
  AISnb.debug = NBIOT_DEBUG; // Enable debugging
  
  // Keep connecting until connection succeed
  do {
    AISnb.setupDevice(SERVER_PORT);
    String devIP = AISnb.getDeviceIP(); // get device IP
    pingResp = AISnb.pingIP(SERVER_IP); // get ping response
  } while(pingResp.rtt == ""); // while ping failed


#if USE_LCD
  lcd.clear();
  lcd.print("Completed.");
  delay(1000);
  lcd.clear();
#endif

  prevMillis = millis();
}


void loop() {
  // read GPS data
  while (Serial2.available() > 0)
    gps.encode(Serial2.read());
 
  // read dust data
  if (pms.read(pmsData)) {
    
    // check for NB-IoT connection
    pingResp = AISnb.pingIP(SERVER_IP);
    if (pingResp.rtt != "") {
      if(DEBUG) Serial.println("Connected to NB-IoT");
    } else {
      Serial.println("Connection Failed. Resetting Arduino...");
      delay(1000);
      resetArduino();
    }

    // fill up GPS Payload data
    if (gps.location.isValid())
    {
      gpsLat_payload = String((int32_t)(gps.location.lat()*100000));
      gpsLng_payload = String((int32_t)(gps.location.lng()*100000));
    }

    RtcDateTime now = Rtc.GetDateTime();
    // prints date/time
    if (DEBUG) {
      Serial.print("Current Time: ");
      printDateTime(now);  Serial.println();
    }

    char timePayload[80] = {0};
    sprintf(timePayload, "%02u/%02u/%04u %02u:%02u:%02u",
            now.Month(),
            now.Day(),
            now.Year(),
            now.Hour(),
            now.Minute(),
            now.Second());

    String payload = "{\"dev_id\":" + String(DEV_ID) + \
                     ",\"type\":\"PM\",\"timestamp\":" + String(timePayload) + \
                     ",\"PM1.0\":" + String(pmsData.PM_AE_UG_1_0) + \
                     ",\"PM2.5\":" + String(pmsData.PM_AE_UG_2_5) + \
                     ",\"PM10.0\":" + String(pmsData.PM_AE_UG_10_0) + \
                     ",\"Lat\":" + gpsLat_payload + \
                     ",\"Lng\":" + gpsLng_payload + "}";

#if USE_LCD
    lcd.clear(); lcd.setCursor(0, 0);
    lcd.print("PM1.0:" + String(pmsData.PM_AE_UG_1_0) + " PM2.5:" + String(pmsData.PM_AE_UG_2_5));
    lcd.setCursor(0, 1);
    lcd.print("PM10.0:" + String(pmsData.PM_AE_UG_10_0));
#endif

    // transmitting data with given duty Cycle
    if (millis() - prevMillis > DUTYCYCLE) {
      if (DEBUG) Serial.println("Transmitting Payload to server...");
      prevMillis = millis();
      
      UDPSend udp = AISnb.sendUDPmsgStr(SERVER_IP, SERVER_PORT, payload);
      UDPReceive resp = AISnb.waitResponse();
    }

    // reset payload
    gpsLat_payload = ""; gpsLng_payload = "";
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
  /*
    Prints date and time from RTC module in the Serial Terminal
  */
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

void displayInfo()
{
  Serial.print(F("Location: ")); 
  if (gps.location.isValid())
  {
    Serial.print(gps.location.lat(), 6);
    Serial.print(F(","));
    Serial.print(gps.location.lng(), 6);
  }
  else
  {
    Serial.print(F("INVALID"));
  }

  Serial.print(F("  Date/Time: "));
  if (gps.date.isValid())
  {
    Serial.print(gps.date.month());
    Serial.print(F("/"));
    Serial.print(gps.date.day());
    Serial.print(F("/"));
    Serial.print(gps.date.year());
  }
  else
  {
    Serial.print(F("INVALID"));
  }

  Serial.print(F(" "));
  if (gps.time.isValid())
  {
    if (gps.time.hour() < 10) Serial.print(F("0"));
    Serial.print(gps.time.hour());
    Serial.print(F(":"));
    if (gps.time.minute() < 10) Serial.print(F("0"));
    Serial.print(gps.time.minute());
    Serial.print(F(":"));
    if (gps.time.second() < 10) Serial.print(F("0"));
    Serial.print(gps.time.second());
    Serial.print(F("."));
    if (gps.time.centisecond() < 10) Serial.print(F("0"));
    Serial.print(gps.time.centisecond());
  }
  else
  {
    Serial.print(F("INVALID"));
  }

  Serial.println();
}
