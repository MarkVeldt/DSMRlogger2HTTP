/*
***************************************************************************  
**  Program  : DSMRlogger2HTTP
**  Version  : v5.0
**
**  Copyright (c) 2018 Willem Aandewiel
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
  Arduino-IDE settings for ESP01 (black):

    - Board: "Generic ESP8266 Module"
    - Flash mode: "DIO" / "DOUT"
    - Flash size: "1M (128K SPIFFS)"
    - Debug port: "Disabled"
    - Debug Level: "None"
    - IwIP Variant: "v2 Lower Memory"
    - Reset Method: "nodemcu"
    - Crystal Frequency: "26 MHz" (otherwise Serial output is garbidge)
    - Flash Frequency: "40MHz"
    - CPU Frequency: "80 MHz"
    - Buildin Led: "1"  // GPIO01 - Pin 2
    - Upload Speed: "115200"
    - Erase Flash: "Only Sketch"
    - Port: "ESP2RFlink at <-- IP address -->"
*/
#include <ESP8266WiFi.h>        // version 1.0.0
#include <ESP8266WebServer.h>   // Version 1.0.0
#include <WiFiManager.h>        // version 0.14.0 https://github.com/tzapu/WiFiManager
#include <ArduinoOTA.h>         // Version 1.0.0
#include <TelnetStream.h>       // Version 0.0.1
#include "FTPserver.h"          // Version "FTP-2017-10-18"
#include <FS.h>
#include <TimeLib.h>
#include <dsmr.h>               // Version 0.1.0

//#define HASS_NO_METER       // define if No Meter is attached

#ifdef ARDUINO_ESP8266_NODEMCU
  #define VCC_ENABLE   14   // D3 = GPIO0, D5 = GPIO14, D6 = GPIO12
  #ifndef HASS_NO_METER
    #define HOSTNAME     "NODEMCU-DSMR"
  #else
    #define HOSTNAME     "TEST-DSMR"
  #endif
#endif
#ifdef ARDUINO_ESP8266_GENERIC
  #ifndef HASS_NO_METER
    #define HOSTNAME     "ESP01-DSMR"
  #else
    #define HOSTNAME     "TEST-DSMR"
  #endif
  //#define VCC_ENABLE    0   // GPIO02 Pin 5
  //#define VCC_ENABLE    1   // TxD -> GPIO01 Pin 1
#endif
#define HOURS_FILE        "/hours.csv"
#define WEEKDAY_FILE      "/weekDay.csv"
#define MONTHS_FILE       "/months.csv"
#define MONTHS_CSV_HEADER "YYMM;   Energy Del;   Energy Ret;    Gas Del;\n"
#define HOURS_CSV_HEADER  "HR; Energy Del; Energy Ret;    Gas Del;\n"
#define LOG_FILE          "/logger.txt"
#define LOG_FILE_R        "/loggerR.txt"
//#define MAXGOOGLE       25
#define NUMLASTLOG        3  
#define LED_ON            LOW
#define LED_OFF           HIGH

typedef struct {
    uint16_t  Label;
    float     EnergyDelivered;
    float     EnergyReturned;
    float     GasDelivered;
} dataStruct;

static    dataStruct hoursDat[10];     // 0 + 1-8
static    dataStruct weekDayDat[9];   // 0 - 6 (0=sunday)
static    dataStruct monthsDat[26];   // 0 + year1 1 t/m 12 + year2 1 t/m 12

/**
struct FSInfo {
    size_t totalBytes;
    size_t usedBytes;
    size_t blockSize;
    size_t pageSize;
    size_t maxOpenFiles;
    size_t maxPathLength;
};
**/
static FSInfo SPIFFSinfo;

// Set up to read from the Serial port, and use D5 as the
// request pin. 
#ifdef VCC_ENABLE
  P1Reader reader(&Serial, VCC_ENABLE);
#else
  P1Reader reader(&Serial, 0);
#endif
;

WiFiClient      wifiClient;
ESP8266WebServer server ( 80 );
FtpServer ftpSrv;   //set #define FTP_DEBUG in ESP8266FtpServer.h to see ftp verbose on serial

uint32_t  waitLoop, telegramCount, waitForATOupdate;
char      cMsg[100], fChar[10];
char      APname[50], MAChex[13]; //n1n2n3n4n5n6\0
//byte      mac[6];
float     EnergyDelivered, EnergyReturned;
float     PowerDelivered, PowerDelivered_l1, PowerDelivered_l2, PowerDelivered_l3;
float     PowerReturned,  PowerReturned_l1,  PowerReturned_l2,  PowerReturned_l3;
float     GasDelivered;
String    pTimestamp;
String    Identification, P1_Version, Equipment_Id, Gas_Equipment_Id, Electricity_Tariff;
float     Energy_Delivered_Tariff1, Energy_Delivered_Tariff2, Energy_Returned_Tariff1, Energy_Returned_Tariff2;
float     Voltage_l1, Voltage_l2, Voltage_l3;
float     Current_l1, Current_l2, Current_l3;
float     Power_Delivered_l1, Power_Delivered_l2, Power_Delivered_l3;
float     Power_Returned_l1, Power_Returned_l2, Power_Returned_l3;
int       Gas_Device_Type;

String    lastReset   = "", lastStartup = "";
String    lastLogLine[NUMLASTLOG + 1]; 
bool      debug = true, OTAinProgress = false, Verbose = false, SPIFFSmounted = false;
String    dateTime;
int8_t    thisHour = -1, thisWeekDay = -1, thisMonth = -1, lastMonth, thisYear = 15;
int8_t    tries;
uint32_t  unixTimestamp;
IPAddress ipDNS, ipGateWay, ipSubnet;
uint16_t  WIFIreStartCount;
String    jsonString;

/**
 * Define the DSMRdata we're interested in, as well as the DSMRdatastructure to
 * hold the parsed DSMRdata. This list shows all supported fields, remove
 * any fields you are not using from the below list to make the parsing
 * and printing code smaller.
 * Each template argument below results in a field of the same name.
 */
using MyData = ParsedData<
  /* String */ identification,
  /* String */ p1_version,
  /* String */ timestamp,
  /* String */ equipment_id,
  /* FixedValue */ energy_delivered_tariff1,
  /* FixedValue */ energy_delivered_tariff2,
  /* FixedValue */ energy_returned_tariff1,
  /* FixedValue */ energy_returned_tariff2,
  /* String */ electricity_tariff,
  /* FixedValue */ power_delivered,
  /* FixedValue */ power_returned,
  /* FixedValue */ electricity_threshold,
  /* uint8_t */ electricity_switch_position,
  /* uint32_t */ electricity_failures,
  /* uint32_t */ electricity_long_failures,
  /* String */ electricity_failure_log,
  /* uint32_t */ electricity_sags_l1,
  /* uint32_t */ electricity_sags_l2,
  /* uint32_t */ electricity_sags_l3,
  /* uint32_t */ electricity_swells_l1,
  /* uint32_t */ electricity_swells_l2,
  /* uint32_t */ electricity_swells_l3,
  /* String */ message_short,
  /* String */ message_long,
  /* FixedValue */ voltage_l1,
  /* FixedValue */ voltage_l2,
  /* FixedValue */ voltage_l3,
  /* FixedValue */ current_l1,
  /* FixedValue */ current_l2,
  /* FixedValue */ current_l3,
  /* FixedValue */ power_delivered_l1,
  /* FixedValue */ power_delivered_l2,
  /* FixedValue */ power_delivered_l3,
  /* FixedValue */ power_returned_l1,
  /* FixedValue */ power_returned_l2,
  /* FixedValue */ power_returned_l3,
  /* uint16_t */ gas_device_type,
  /* String */ gas_equipment_id,
  /* uint8_t */ gas_valve_position,
  /* TimestampedFixedValue */ gas_delivered,
  /* uint16_t */ thermal_device_type,
  /* String */ thermal_equipment_id,
  /* uint8_t */ thermal_valve_position,
  /* TimestampedFixedValue */ thermal_delivered,
  /* uint16_t */ water_device_type,
  /* String */ water_equipment_id,
  /* uint8_t */ water_valve_position,
  /* TimestampedFixedValue */ water_delivered,
  /* uint16_t */ slave_device_type,
  /* String */ slave_equipment_id,
  /* uint8_t */ slave_valve_position,
  /* TimestampedFixedValue */ slave_delivered
>;

struct showValues {
  template<typename Item>
  void apply(Item &i) {
    if (i.present()) {
        TelnetStream.print(Item::name);
        TelnetStream.print(F(": "));
        TelnetStream.print(i.val());
        TelnetStream.print(Item::unit());
        TelnetStream.println();
    }
  }
};
 
//===========================================================================================
void displayHoursHist(bool);  // prototype (see handleMenu tab)
void displayDaysHist(bool);   // prototype
void displayMonthsHist(bool); // prototype
//===========================================================================================


//===========================================================================================
String macToStr(const uint8_t* mac) {
//===========================================================================================
  String result;
  for (int i = 0; i < 6; ++i) {
    result += String(mac[i], 16);
    if (i < 5)
      result += ':';
  }
  return result;
} // macToStr()


//===========================================================================================
void hourToSlot(int8_t h, int8_t &slot, int8_t &nextSlot, int8_t &prevSlot) {
//===========================================================================================
  switch(h) {
    case  0:
    case  1:
    case  2:  slot = 1; nextSlot = 2; prevSlot = 8; break;
    case  3:
    case  4:
    case  5:  slot = 2; nextSlot = 3; prevSlot = 1;  break;
    case  6:
    case  7:
    case  8:  slot = 3; nextSlot = 4; prevSlot = 2;  break;
    case  9:
    case 10:
    case 11:  slot = 4; nextSlot = 5; prevSlot = 3;  break;
    case 12:
    case 13:
    case 14:  slot = 5; nextSlot = 6; prevSlot = 4;  break;
    case 15:
    case 16:
    case 17:  slot = 6; nextSlot = 7; prevSlot = 5;  break;
    case 18:
    case 19:
    case 20:  slot = 7; nextSlot = 8; prevSlot = 6;  break;
    case 21:
    case 22:
    case 23:  slot = 8; nextSlot = 1; prevSlot = 7;  break;
    default:  slot = 8; nextSlot = 1; prevSlot = 7; 
  }

} // hourToSlot()


//===========================================================================================
void printData() {
//===========================================================================================

    TelnetStream.println("-Totalen----------------------------------------------------------");
    dateTime = buildDateTimeString(pTimestamp);
    sprintf(cMsg, "Datum / Tijd         :  %s", dateTime.c_str());
    TelnetStream.println(cMsg);

    dtostrf(EnergyDelivered, 9, 3, fChar);
    sprintf(cMsg, "Energy Delivered     : %skWh", fChar);
    TelnetStream.println(cMsg);

    dtostrf(EnergyReturned, 9, 3, fChar);
    sprintf(cMsg, "Energy Returned      : %skWh", fChar);
    TelnetStream.println(cMsg);

    dtostrf(PowerDelivered, 8, 1, fChar);
    sprintf(cMsg, "Power Delivered      : %sWatt", fChar);
    TelnetStream.println(cMsg);

    dtostrf(PowerReturned, 8, 1, fChar);
    sprintf(cMsg, "Power Returned       : %sWatt", fChar);
    TelnetStream.println(cMsg);
    
    dtostrf(PowerDelivered_l1, 8, 1, fChar);
    sprintf(cMsg, "Power Delivered (l1) : %sWatt", fChar);
    TelnetStream.println(cMsg);
    
    dtostrf(PowerDelivered_l2, 8, 1, fChar);
    sprintf(cMsg, "Power Delivered (l2) : %sWatt", fChar);
    TelnetStream.println(cMsg);
    
    dtostrf(PowerDelivered_l3, 8, 1, fChar);
    sprintf(cMsg, "Power Delivered (l3) : %sWatt", fChar);
    TelnetStream.println(cMsg);
    
    dtostrf(PowerReturned_l1, 8, 1, fChar);
    sprintf(cMsg, "Power Returned (l1)  : %sWatt", fChar);
    TelnetStream.println(cMsg);
    
    dtostrf(PowerReturned_l2, 8, 1, fChar);
    sprintf(cMsg, "Power Returned (l2)  : %sWatt", fChar);
    TelnetStream.println(cMsg);
    
    dtostrf(PowerReturned_l3, 8, 1, fChar);
    sprintf(cMsg, "Power Returned (l3)  : %sWatt", fChar);
    TelnetStream.println(cMsg);

    dtostrf(GasDelivered, 9, 2, fChar);
    sprintf(cMsg, "Gas Delivered        : %sm3", fChar);
    TelnetStream.println(cMsg);
    TelnetStream.println("==================================================================");
  
} // printData()


//===========================================================================================
void processData(MyData DSMRdata) {
//===========================================================================================
  int8_t slot, nextSlot, prevSlot;
  
#ifndef HASS_NO_METER
    EnergyDelivered     = (float)(  DSMRdata.energy_delivered_tariff1 
                                  + DSMRdata.energy_delivered_tariff2);
    EnergyReturned      = (float)(  DSMRdata.energy_returned_tariff1 
                                  + DSMRdata.energy_returned_tariff2);
    PowerDelivered      = (float)((PowerDelivered    + (DSMRdata.power_delivered    * 1000.0)) / 2.0); 
    PowerDelivered_l1   = (float)((PowerDelivered_l1 + (DSMRdata.power_delivered_l1 * 1000.0)) / 2.0); 
    PowerDelivered_l2   = (float)((PowerDelivered_l2 + (DSMRdata.power_delivered_l2 * 1000.0)) / 2.0); 
    PowerDelivered_l3   = (float)((PowerDelivered_l3 + (DSMRdata.power_delivered_l3 * 1000.0)) / 2.0); 
    PowerReturned       = (float)((PowerReturned     + (DSMRdata.power_returned     * 1000.0)) / 2.0); 
    PowerReturned_l1    = (float)((PowerReturned_l1  + (DSMRdata.power_returned_l1  * 1000.0)) / 2.0); 
    PowerReturned_l2    = (float)((PowerReturned_l2  + (DSMRdata.power_returned_l2  * 1000.0)) / 2.0); 
    PowerReturned_l3    = (float)((PowerReturned_l3  + (DSMRdata.power_returned_l3  * 1000.0)) / 2.0);
    GasDelivered        = (float)DSMRdata.gas_delivered;
    pTimestamp          = DSMRdata.timestamp;

    Identification            = DSMRdata.identification;
    P1_Version                = DSMRdata.p1_version;
    Equipment_Id              = DSMRdata.equipment_id;
    Energy_Delivered_Tariff1  = (float)DSMRdata.energy_delivered_tariff1;
    Energy_Delivered_Tariff2  = (float)DSMRdata.energy_delivered_tariff2;
    Energy_Returned_Tariff1   = (float)DSMRdata.energy_returned_tariff1;
    Energy_Returned_Tariff2   = (float)DSMRdata.energy_returned_tariff2;
    Electricity_Tariff        = DSMRdata.electricity_tariff;
    Voltage_l1                = (float)DSMRdata.voltage_l1;
    Voltage_l2                = (float)DSMRdata.voltage_l2;
    Voltage_l3                = (float)DSMRdata.voltage_l3;
    Current_l1                = (float)DSMRdata.current_l1;
    Current_l2                = (float)DSMRdata.current_l2;
    Current_l3                = (float)DSMRdata.current_l3;
    Power_Delivered_l1        = DSMRdata.power_delivered_l1;
    Power_Delivered_l2        = DSMRdata.power_delivered_l2;
    Power_Delivered_l3        = DSMRdata.power_delivered_l3;
    Power_Returned_l1         = DSMRdata.power_returned_l1;
    Power_Returned_l2         = DSMRdata.power_returned_l2;
    Power_Returned_l3         = DSMRdata.power_returned_l3;
    Gas_Device_Type           = DSMRdata.gas_device_type;
    Gas_Equipment_Id          = DSMRdata.gas_equipment_id;
#endif
    unixTimestamp       = epoch(pTimestamp);
    if ((lastStartup == "") && (pTimestamp.length() >= 10)) {
      lastStartup = "lastStartup: <b>" + buildDateTimeString(pTimestamp) + "</b>,  Restart Reason: <b>" + lastReset + "</b> ";
    }

    if (DSMRdata.power_delivered == 0.0) {
      PowerDelivered = PowerDelivered_l1 + PowerDelivered_l2 + PowerDelivered_l3;
    }
    if (DSMRdata.power_returned == 0.0) {
      PowerReturned = PowerReturned_l1 + PowerReturned_l2 + PowerReturned_l3;
    }

//================= handle Hour change ======================================================
    if (thisHour != HourFromTimestamp(pTimestamp)) {
      thisHour = HourFromTimestamp(pTimestamp);
      if (   (EnergyDelivered == 0.0) 
          || (EnergyReturned  == 0.0)
          || (GasDelivered    == 0.0)) {
            TelnetStream.println("Last Read hourData is zero (skip)");
            writeLogFile("Last Read hourData is zero (skip)");
            thisHour = -2;
      } else {
        hourToSlot(thisHour, slot, nextSlot, prevSlot);
//      slot = thisHour;
//      if (slot < 0) slot = 23;
        TelnetStream.printf("Saving data for thisHour[%02d] in slot[%02d] (nextSlot[%02d])\n", thisHour, slot, nextSlot);
        Serial.printf("Saving data for thisHour[%02d] in slot[%02d] (nextSlot[%02d])\n", thisHour, slot, nextSlot);
        hoursDat[slot].EnergyDelivered = EnergyDelivered;
        hoursDat[slot].EnergyReturned  = EnergyReturned;
        hoursDat[slot].GasDelivered    = GasDelivered;
        if(!saveHourData(slot)) {
          TelnetStream.println("Error writing hourData ..(zero value)");
          writeLogFile("Error writing hourData ..(zero value)");
          delay(500);
        }
      }

    } // if (thisHour != HourFromTimestamp(pTimestamp)) 

    hourToSlot(thisHour, slot, nextSlot, prevSlot);

    if (Verbose) {
      TelnetStream.printf("Put data for Hour[%02d] in nextSlot[%02d]\n", thisHour, nextSlot);
    }
    hoursDat[slot].EnergyDelivered = EnergyDelivered;
    hoursDat[slot].EnergyReturned  = EnergyReturned;
    hoursDat[slot].GasDelivered    = GasDelivered;
     
//================= handle Day change ======================================================
    if (thisWeekDay != weekday(unixTimestamp)) {
      // weekday() from unixTimestamp is from 1 (sunday) to 7 (saterday)
      if (thisWeekDay != -1) rotateLogFile("Daily rotate");
      thisWeekDay = weekday(unixTimestamp);
      // in our weekDayDat[] table we have to subtract "1" to get 0 (sunday) to 6 (saterday)
      slot = thisWeekDay - 1;
      if (slot < 0) slot = 6;
      TelnetStream.printf("Saving data for WeekDay[%02d] in slot[%02d]\n", thisWeekDay, slot);
      Serial.printf("Saving data for WeekDay[%02d] in slot[%02d]\n", thisWeekDay, slot);
      weekDayDat[slot].EnergyDelivered = EnergyDelivered;
      weekDayDat[slot].EnergyReturned  = EnergyReturned;
      weekDayDat[slot].GasDelivered    = GasDelivered;
      saveWeekDayData(); 
    }
    slot = weekday(unixTimestamp);
    // in our weekDayDat[] table we have to subtract "1" to get 0 (sunday) to 6 (saterday)
    slot -= 1;
    if (slot < 0) slot = 6;
    weekDayDat[slot].EnergyDelivered = EnergyDelivered;
    weekDayDat[slot].EnergyReturned  = EnergyReturned;
    weekDayDat[slot].GasDelivered    = GasDelivered;

//================= handle Month change ======================================================
    if (thisMonth != MonthFromTimestamp(pTimestamp)) {
      thisMonth = MonthFromTimestamp(pTimestamp);
      thisYear  = YearFromTimestamp(pTimestamp);
      if (Verbose) TelnetStream.printf("processData(): thisYear[%02d] => thisMonth[%02d]\r\n", thisYear, thisMonth);
      TelnetStream.flush();
      if (   (EnergyDelivered == 0.0) 
          || (EnergyReturned  == 0.0)
          || (GasDelivered    == 0.0)) {
            TelnetStream.println("Last Read monthData is zero (skip)");
            writeLogFile("Last Read monthData is zero (skip)");
            thisMonth = -2;
      } else {
        lastMonth = getLastMonth();
        if (lastMonth != thisMonth) {
          if (Verbose) TelnetStream.printf("processData(): lastMonth[%02d]; thisYear[%02d] => thisMonth[%02d]\r\n"
                                                          ,lastMonth,       thisYear,         thisMonth);
          TelnetStream.println("Move thisMonth one slot up");
          TelnetStream.flush();
          Serial.println("Move thisMonth one slot up");
          writeLogFile("Move thisMonth one slot up!");
          shiftDownMonthData(thisYear, thisMonth);
        }
        TelnetStream.printf("Saving data for thisMonth[%02d-%02d] in slot[01]\n", thisYear, thisMonth);
        Serial.printf("Saving data for thisMonth[%02d-%02d] in slot[01]\n", thisYear, thisMonth);
        sprintf(cMsg, "%02d%02d", thisYear, thisMonth);
        monthsDat[1].Label           = String(cMsg).toInt();
        monthsDat[1].EnergyDelivered = EnergyDelivered;
        monthsDat[1].EnergyReturned  = EnergyReturned;
        monthsDat[1].GasDelivered    = GasDelivered;
        if(!saveMonthData(thisYear, thisMonth)) {
          TelnetStream.println("Error writing monthData ..(zero value)");
          writeLogFile("Error writing monthData ..(zero value)");
          delay(500);
        }
      }

    } // if (thisMonth != MonthFromTimestamp(pTimestamp)) 

    if (Verbose) {
      TelnetStream.printf("Put data for Month[%02d-%02d] in Slot[01]\n", thisYear, thisMonth);
    }
    monthsDat[1].EnergyDelivered = EnergyDelivered;
    monthsDat[1].EnergyReturned  = EnergyReturned;
    monthsDat[1].GasDelivered    = GasDelivered;
   
} // processData()


//===========================================================================================
void setup() {
//===========================================================================================
  Serial.begin(115200, SERIAL_8N1);
  pinMode(BUILTIN_LED, OUTPUT);
  for(int I=0; I<5; I++) {
    digitalWrite(BUILTIN_LED, !digitalRead(BUILTIN_LED));
    delay(2000);
  }
  digitalWrite(BUILTIN_LED, LED_OFF);  // HIGH is OFF
  lastStartup = "";
  lastReset     = ESP.getResetReason();
  
  #ifdef ARDUINO_ESP8266_GENERIC
    debug = false;
  #endif
  
  if (debug) Serial.println("\nBooting....\n");

//================ SPIFFS =========================================
  if (!SPIFFS.begin()) {
    if (debug) Serial.println("SPIFFS Mount failed");   // Serious problem with SPIFFS 
    TelnetStream.println("SPIFFS Mount failed");        // Serious problem with SPIFFS 
    SPIFFSmounted = false;
    
  } else { 
    if (debug) Serial.println("SPIFFS Mount succesfull");
    TelnetStream.println("SPIFFS Mount succesfull");
    SPIFFSmounted = true;
    ftpSrv.begin("esp8266","esp8266");    //username, PASSWORD for ftp.  set ports in ESP8266FtpServer.h  (default 21, 50009 for PASV)
    sprintf(cMsg, "Last reset reason: [%s]", ESP.getResetReason().c_str());
    if (lastReset.length() > 2) {
        writeLogFile(cMsg);
    }
  }
//=============end SPIFFS =========================================

#ifdef VCC_ENABLE
    // This is needed on Pinoccio Scout boards to enable the 3V3 pin.
    pinMode(VCC_ENABLE, OUTPUT);
    digitalWrite(VCC_ENABLE, LOW);
#endif


  digitalWrite(BUILTIN_LED, LED_ON);
  setupWiFi(false);
  digitalWrite(BUILTIN_LED, LED_OFF);

  if (debug) {
    Serial.println ( "" );
    Serial.print ( "Connected to " ); Serial.println (WiFi.SSID());
    Serial.print ( "IP address: " );  Serial.println (WiFi.localIP());
  }
  for (int L=0; L < 10; L++) {
    digitalWrite(BUILTIN_LED, !digitalRead(BUILTIN_LED));
    delay(200);
  }
  digitalWrite(BUILTIN_LED, LED_OFF);

  TelnetStream.begin();
  TelnetStream.flush();
  if (debug) Serial.println("\nTelnet server started ..");
  delay(500);

  //============= configure OTA (minimal) ====================
  ArduinoOTA.setHostname(HOSTNAME);   // defaults to esp8266-[ChipID]
  ArduinoOTA.onStart([]() {
    writeLogFile("Start OTA update ...");
    Serial.swap();  // stop receiving data from TxR
    OTAinProgress = true;
    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    SPIFFS.end();
    SPIFFSmounted = false;
  });
  ArduinoOTA.begin();
  //==============OTA end=====================================

  if (!readHourData()) {
    writeLogFile("Error readHourData() ..! ");
  }
  if (!readWeekDayData()) {
    writeLogFile("Error readWeekDayData() ..! ");
  }
  readMonthData();

#ifdef HASS_NO_METER
  for (int s=1; s<=8; s++) {
    hoursDat[s].Label           = s;
    hoursDat[s].EnergyDelivered   = 0.0;
    hoursDat[s].EnergyReturned    = 0.0;
    hoursDat[s].GasDelivered      = 0.0;
  }
  for (int s=0; s<=6; s++) {
    weekDayDat[s].Label           = s;
    weekDayDat[s].EnergyDelivered = 0.0;
    weekDayDat[s].EnergyReturned  = 0.0;
    weekDayDat[s].GasDelivered    = 0.0;
  }
  for (int s=24; s>=1; s--) {
    sprintf(cMsg, "18%02d", s);
    monthsDat[s].Label           = String(cMsg).toInt();
    monthsDat[s].EnergyDelivered = 0.0;
    monthsDat[s].EnergyReturned  = 0.0;
    monthsDat[s].GasDelivered    = 0.0;
  }
  randomSeed(analogRead(0));
#endif
  telegramCount   =  0;

    
  server.on("/getMeterInfo.json", sendDataMeterInfo);
  server.on("/getActual.json", sendDataActual);
  server.on("/getTableWeek.json", sendTableWeek);
  server.on("/getTableHours.json", sendTableHours);
  server.on("/getTableMonths.json", sendTableMonths);

  server.serveStatic("/js", SPIFFS, "/js");
  server.serveStatic("/css", SPIFFS, "/css");
  server.serveStatic("/img", SPIFFS, "/img");
  server.serveStatic("/", SPIFFS, "/index.html");

  server.begin();
  if (debug) Serial.println( "HTTP server started" );
  TelnetStream.println( "HTTP server started" );
  Serial.flush();
  TelnetStream.flush();

#ifdef VCC_ENABLE
  digitalWrite(VCC_ENABLE, LOW);
  delay(200);
#endif
  for(int l=0; l < NUMLASTLOG; l++) {
    lastLogLine[l] = String(l);
  }
  
  if (debug) Serial.println("\nEnable reader..");
  Serial.flush();
  TelnetStream.println("\nEnable reader..");
  TelnetStream.flush();
  delay(100);
  reader.enable(true);

  waitLoop = millis() + 5000;
  
} // setup()


//===========================================================================================
void loop () {
//===========================================================================================
  ArduinoOTA.handle();
  server.handleClient();
  ftpSrv.handleFTP();        //make sure in loop you call handleFTP()!!  
  handleKeyInput();
  
  reader.loop();

  if (!OTAinProgress) {
    if (millis() > waitLoop) {
      waitLoop = millis() + 10000;  // tien seconden?

      reader.enable(true);
#ifdef ARDUINO_ESP8266_GENERIC
      digitalWrite(BUILTIN_LED, LED_ON);
#else
      digitalWrite(BUILTIN_LED, !digitalRead(BUILTIN_LED));
#endif
    } 
  } else {  // waiting for ATO-update (or in progress) ...
      if (millis() > waitForATOupdate) {
        OTAinProgress     = false;
        waitForATOupdate  = millis();
      }
  }
  
#ifdef HASS_NO_METER
  static  MyData    DSMRdata;
  static uint8_t sMinute = 1, sHour = 20, sDay = 27, sMonth = -1, sYear;
  if (sMonth < 0) sMonth = getLastMonth();
  EnergyDelivered  += (float)(random(1, 50) / 15.0);
  EnergyReturned   += (float)(random(1, 40) / 55.0);
  GasDelivered     += (float)(random(1, 30) / 100.0);
  sMinute += 57;
  if (sMinute >= 60) {
    sMinute -= 59;
    sHour++;
  }
  if (sHour >= 24) {  // 0 .. 23
    sHour = 0;
    sDay += 9;
  }
  if (sDay >= 30) {
    sDay = (sDay % 30 ) + 1;
    sMonth++;
  }
  if (sMonth <  1) sMonth = 1;
  if (sMonth > 12) {
    sMonth = 1;
    sYear++;
  }

  telegramCount++;
  sprintf(cMsg, "18%02d%02d%02d%02d15S", sMonth, sDay, sHour, sMinute);
  pTimestamp = String(cMsg);
  if (Verbose) TelnetStream.printf("pTimestamp [%s] sMonth[%02d] sDay[%02d] sHour[%02d] sMinute[%02d]\r\n"
                                  , pTimestamp.c_str(), sMonth,  sDay,      sHour,      sMinute);
  if (!OTAinProgress) {
    digitalWrite(BUILTIN_LED, !digitalRead(BUILTIN_LED));
    processData(DSMRdata);
    delay(1000);
  }

#else
  if (!OTAinProgress) {
    if (reader.available()) {
      ArduinoOTA.handle();

      //-- declaration of DSMRdata must be in 
      //-- if-statement so it will be initialized
      //-- in every itteration (don't know how else)
      MyData    DSMRdata;
      String    DSMRerror;
    
      TelnetStream.println("\n==================================================================");
      TelnetStream.printf("read telegram [%d]\n", ++telegramCount);
      Serial.printf("read telegram [%d]\n", ++telegramCount);

      if (reader.parse(&DSMRdata, &DSMRerror)) {  // Parse succesful, print result
        digitalWrite(BUILTIN_LED, LED_OFF);
        processData(DSMRdata);
        if (Verbose) {
          DSMRdata.applyEach(showValues());
          printData();
        }
      } else {                                    // Parser error, print error
        TelnetStream.printf("Parse error %s\n", DSMRerror.c_str());
      }
    } // if (reader.available()) 
    
  } // if (!OTAinProgress) 
#endif

} // loop()



/***************************************************************************
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to permit
* persons to whom the Software is furnished to do so, subject to the
* following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT
* OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
* THE USE OR OTHER DEALINGS IN THE SOFTWARE.
* 
***************************************************************************/
