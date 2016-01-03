/*------------------------------------------------------------------------------------------------------
Author : Evan Richinson aka MrRetupmoc42
               
Adalogger Solar Panel Power Tracker to SD Card
   Stores Calculated Values from Analog Voltage Input
   Custom RTC Called MTC ( MicroProcessor Time Clock ), Tuned?
   When Charging we Track this Data the Most, Idle Panel Track Every Min ( Track Sun Levels )
   
   Future Idea's
  
   Peek Power with MTC Timestamp ( Peek Power Days )
   Eventually wanting to Have this setup run a 1 Axis Tracking
   
December 30th 2015 : Data Poll, Calculations and Storage
December 31st 2015 : Cleanup of Storage and Started MultiStore
January 1st 2016   : "MTC ( MicroProcessor Time Clock )" and Max Time Values
January 2nd 2016   : Storage Update for MTC, Now MultiStores S,M,H,D and Y and Linerization Added to Calculations
->January 2nd 2016 : Daniel Schimmer -> Inserted before the predicted January 3rd update! ;) - Play around formatting and 'optimization'
January 3rd 2016   : Storage Array for Average Values

Sketch stats before:

Sketch uses 24,296 bytes (84%) of program storage space. Maximum is 28,672 bytes.
Global variables use 1,782 bytes (69%) of dynamic memory, leaving 778 bytes for local variables. Maximum is 2,560 bytes.

Sketch stats now:

Sketch uses 17,480 bytes (60%) of program storage space. Maximum is 28,672 bytes.
Global variables use 934 bytes (36%) of dynamic memory, leaving 1,626 bytes for local variables. Maximum is 2,560 bytes.

Still playing.....

-------------------------------------------------------------------------------------------------------*/
#include "PetitFS.h"
#include <SPI.h>

// Petite FS
FATFS fs;

//Pin Setup and Calculation Data
#define StatusLed 13
#define VBatLipo A9
#define VBatLeadAcid A2
#define VSolar A0
#define ISolar A1
  
String dataString = "";
const String Split = "/";
const String End = ".";

float MeasuredVBatLipo;
float MeasuredVBatLeadAcid;
float MeasureVSolarPanel;
float MeasureISolarPanel;
float MeasureWSolarPanel;

float UnderVBatLipo = 3.300;               //Set Under Voltage Lipo
float UnderVBatLeadAcid = 10.500;          //Set Under Voltage Lead Acid

bool LipoUnderChargedVoltage = false;      //
bool LeadAcidUnderChargedVoltage = false;  //
bool PanelUnderChargeVoltage = false;      //Cloudy or Night Time?
bool PanelAtChargeVoltage = false;         //Charging the Battery
bool PanelOverChargeVoltage = false;       //Sunny But Don't Need to Charge

const bool Debug = true;
const int VoltageOffset = 1;

//This is the "MTC ( MicroProcessor Time Clock )" Setup
const int CalcTimeOffset = 59; //mS, +50mS for LED Flash, The Offset to Correct for a Second of Time

const int SecondCount_Max = 60;  //60 Sec per Min
const int MinuteCount_Max = 60; //60 Min per Hour
const int HourCount_Max = 24;    //24 Hours per Day
const int DayCount_Max = 365;    //365 Days per Year
const int YearCount_Max = 10;    //10 Years worth of Logs....

uint32_t time_in_seconds = 0;

//---------------------------------------------------------------------------------------------------------------------------------------

void setup() {
  pinMode(StatusLed, OUTPUT);
  Serial.begin(9600);

  digitalWrite(StatusLed, HIGH);
  delay(5000); //while (!Serial) { }
  digitalWrite(StatusLed, LOW);

  debug("Full Mode Initialiazation");
    
  if (pf_mount(&fs)) {
    debug("Card failed, or not present");
    return;
  }

  debug("Card Save Enabled.");
  
  digitalWrite(StatusLed, HIGH);
  delay(2000);
  digitalWrite(StatusLed, LOW);
  delay(2000);
}

//-------------------------------------------------------------------------------------------------------------------------------------

void loop() {
  // get time's..instead of tracking in global vars
  uint16_t years = time_in_seconds / 60 / 60 / 24 / 365;
  uint16_t days = (time_in_seconds / 60 / 60 / 24) % 365;
  uint16_t hours = (time_in_seconds / 60 / 60) % 24;
  uint16_t minutes = (time_in_seconds / 60) % 60;
  uint16_t seconds = time_in_seconds % 60;

  //Start Indication of a Poll
  
  digitalWrite(StatusLed, HIGH);
  delay(50);
  
  //Grab Values from Analog Input
  MeasuredVBatLipo = analogRead(VBatLipo);
  MeasuredVBatLeadAcid = analogRead(VBatLeadAcid);
  MeasureVSolarPanel = analogRead(VSolar);
  MeasureISolarPanel = analogRead(ISolar);

  
  //Calculate to Correct Values
  //Internal Resistor Divider Setup 2x
  MeasuredVBatLipo *= 2; //Resistor Divider Inside is 2x
  MeasuredVBatLipo *= 3.3; //WORKS Convert to Voltage of Board
  MeasuredVBatLipo /= 1024; //WORKS Convert the Counts per Voltage
  MeasuredVBatLipo *= 1.01; //% Error Offset


  //External Resistor Divider Setup 4x
  MeasuredVBatLeadAcid *= 4; //Resistor Divider Inside is 4x
  MeasuredVBatLeadAcid *= 3.3; //WORKS Convert to Voltage of Board
  MeasuredVBatLeadAcid /= 1024; //WORKS Convert the Counts per Voltage
  
  //Correction
  //Measured > Readout
  //10.64v --> 10.65v = % Error @ 4x
  //11.81v --> 11.83v = % Error @ 4x
  //12.49v --> 12.53v = % Error @ 4x
  //13.16v --> 13.06v = % Error @ 4x
  //13.55v --> 13.06v = % Error @ 4x
  //14.49v --> 6.v = % Error @ 4x
  //MeasuredVBatLeadAcid *= 1; //
  //MeasuredVBatLeadAcid += 0; //
  MeasuredVBatLeadAcid *= 0.99; //% Error Offset

  //External Autopilot 50V 180A Version
  //50V/180A = 63.69mV / Volt, 18.30mV / Amp.
  //3.3v / 1024 = 3.22mV per Step


  //Solar Panel Voltage ( With Lineraization )
  //63.69mV / Volt / 3.22 = 19.7795
  MeasureVSolarPanel *= 15.8; //Set for Somewhat Real Data was 0.0754 in Wattmeter 
  MeasureVSolarPanel *= 3.3; //WORKS Convert to Voltage of Board 
  MeasureVSolarPanel /= 1024; //WORKS Convert the Counts per Voltage
  
  //Correction
  //Measured > Readout
  //12.37v --> 12.9v = 3% Error @ 15.8x
  //12.42v --> 12.64v = % Error @ 15.8x
  //MeasureVSolarPanel *= 1;    //
  //MeasureVSolarPanel += 0;    //
  MeasureVSolarPanel *= 1.03; //% Error Offset


  //Solar Panel Current ( With Lineraization )
  //18.30mV / Amp / 3.22 = 5.6832
  MeasureISolarPanel *= 5; //Set for RAW Data was 0.67 in Wattmeter
  MeasureISolarPanel *= 3.3; //WORKS Convert to Voltage of Board
  MeasureISolarPanel /= 1024; //WORKS Convert the Counts per Voltage
  
  //Correction
  //Measured > Readout
  //
  //MeasureISolarPanel *= 1; //Set for RAW Data was 0.67 in Wattmeter
  //MeasureISolarPanel += 0; //
  //MeasureISolarPanel *= 1; //% Error Offset


  //Panel Wattage Calulation ( Simple )
  MeasureWSolarPanel = MeasureVSolarPanel * MeasureISolarPanel;

  //Logic Statements ---------------------------------------------------------------------------------------------------------------------

  if (MeasuredVBatLipo <= UnderVBatLipo) {
    LipoUnderChargedVoltage = true;
  } else {
    LipoUnderChargedVoltage = false;
  }

  if (MeasuredVBatLeadAcid <= UnderVBatLeadAcid) {
    LeadAcidUnderChargedVoltage = true;
  } else {
    LeadAcidUnderChargedVoltage = false;
    
    if (MeasureVSolarPanel < (MeasuredVBatLeadAcid - VoltageOffset)) {
      PanelUnderChargeVoltage = true;
      PanelAtChargeVoltage = false;
      PanelOverChargeVoltage = false;
    } else if(MeasureVSolarPanel >= (MeasuredVBatLeadAcid - VoltageOffset) && MeasureVSolarPanel <= (MeasuredVBatLeadAcid + VoltageOffset)) {
      PanelUnderChargeVoltage = false;
      PanelAtChargeVoltage = true;
      PanelOverChargeVoltage = false;
    } else if (MeasureVSolarPanel > (MeasuredVBatLeadAcid - VoltageOffset)) {
      PanelUnderChargeVoltage = false;
      PanelAtChargeVoltage = false;
      PanelOverChargeVoltage = true;
    }
  }

  if (MeasureVSolarPanel <= 0  || MeasureVSolarPanel <= 0 && LeadAcidUnderChargedVoltage) {
    PanelUnderChargeVoltage = false;
    PanelAtChargeVoltage = false;
    PanelOverChargeVoltage = false;
  }
  

  //Serial Out the Values ------------------------------------------------------------------------------------------------------------------
  if (Serial) {
    debug("");
    debug("********************************");
    debug("Lipo Bat Voltage : %f", MeasuredVBatLipo);

    if(LipoUnderChargedVoltage) {
      debug("Lipo Backup is Under Charged, Sleeping");
    } else {
      debug("Lipo Backup is Good to GO");
    }

    debug("-------------------------------");
    debug("Lead Acid Bat Voltage : %f", MeasuredVBatLeadAcid);

    if (LeadAcidUnderChargedVoltage) {
      debug("LeadAcid Backup is Under Charged...");
    } else {
      debug("LeadAcid Backup is Ready");
    }

    debug("-------------------------------");
    debug("Solar Panel Voltage : %f", MeasureVSolarPanel);
    debug("Solar Panel Current : %2f", MeasureISolarPanel);
    debug("Solar Panel Wattage : %2f", MeasureWSolarPanel);
    
    if (!PanelUnderChargeVoltage && !PanelAtChargeVoltage && !PanelOverChargeVoltage) {
      debug("No Solar Panel Found");
    } else if(PanelUnderChargeVoltage) {
      debug("Not Enough Voltage to Charge");
    } else if(PanelAtChargeVoltage) {
      debug("Charging Backup Battery");  
    } else if(PanelOverChargeVoltage) {
      debug("Float / Done Charging / Backup Battery");
    } else {
      debug("Not Charging");
    }
    
    debug("********************************");
  }

  //Datalog Spreadsheet Setup ------------------------------------------------------------------------------------------------------------------

  if(time_in_seconds < 0) { //Startup Print Values?
    dataString = F("Year/Day/Hour/Minute/Second/LipoBat_V/LeadAcidBat_V/SolarPanel_V/SolarPanel_I/SolarPanel_W/Temperature/Humidity.");
  } else { //Normal Operation
  //Storage Array :   Year,                       Day,                       Hour,                       Minute,
    dataString = String(years) + Split + String(days) + Split + String(hours) + Split + String(minutes) + Split + 
    //     Second,                       LipoBat_V,                         LeadAcidBat_V,                         SolarPanel_V,
    String(seconds) + Split + String(MeasuredVBatLipo) + Split + String(MeasuredVBatLeadAcid) + Split + String(MeasureVSolarPanel) + Split +
    //     SolarPanel_I,                        SolarPanel_W,                        Temperature,        Humidity.
    String(MeasureISolarPanel) + Split + String(MeasureWSolarPanel) + Split + String(0) + Split + String(0) + End;
  }

  // XXX: Need to look at these ifs after changing time determination....funny assumption here...
  
  //Datalog the Data that I Captured each Second;
  if (seconds < SecondCount_Max) {
    // Second Tick Capture
    writeLog(F("sec.txt"), dataString);
  }

  if (seconds >= SecondCount_Max) { //Second Tick 60s Meaning 1 Min Capture
    writeLog(F("min.txt"), dataString);
  }

  if (minutes >= MinuteCount_Max) {
    //Min Tick 60m Meaning 1 Hour Capture
    writeLog(F("hour.txt"), dataString);
  }

  if (hours >= HourCount_Max) {
    //Hour Tick 24h Meaning 1 Day Capture
    writeLog(F("day.txt"), dataString);
  }

  if (days >= DayCount_Max) {
    //Day Tick 365d Meaning 1 Year Capture
    writeLog(F("year.txt"), dataString);
  }

  //YearCount >= YearCount_Max //Year Tick 10y Meaning 1 Decade?
  
  //End Data Poll ------------------------------------------------------------------------------------------
  digitalWrite(StatusLed, LOW);
  
  if (PanelAtChargeVoltage && !LipoUnderChargedVoltage) {
    delay(1000 - CalcTimeOffset); //Should be Charging, Watch this Data Closely
    time_in_seconds += 1;
  }
  else if (PanelOverChargeVoltage && !LipoUnderChargedVoltage) {
    delay(5000 - CalcTimeOffset); //Whats the Sun Levels Like?
    time_in_seconds += 5;
  }
  else if (PanelUnderChargeVoltage && !LipoUnderChargedVoltage) {
    delay(15000 - CalcTimeOffset); //Still want to know what the sun levels are like when low
    time_in_seconds += 14;
  }
  else if (!PanelUnderChargeVoltage && !PanelAtChargeVoltage && !PanelOverChargeVoltage && !LipoUnderChargedVoltage) {
    delay(30000 - CalcTimeOffset); //5min, 1s Datalog Feed if not Charging and Above 0V ( Cloudy / Night Mode )
    time_in_seconds += 30;
  }
  else {
    delay(60000 - CalcTimeOffset);  //I Dunno what the stare of the System is...
    time_in_seconds += 60;
  }
}

void writeLog(const __FlashStringHelper *fsh, String dataString) {
  //File datafile = SD.open(fileName, FILE_WRITE);
  char fileName[32];

  // XXX: find a safe..err smarter...er intell....err non-potential-boom-boom way of doing this..BOOM
  strcpy_P(fileName, (PGM_P)fsh);

  int result = pf_open(fileName);
  
  if (result == FR_OK) {
      //datafile.println(dataString);
      pf_write(dataString.c_str(), 0, 0);

      debug("Updated %s", fileName);
    

    if (Debug) {
      debug(dataString.c_str());
    }
  } else {
    debug("Error Opening %s, %d", fileName, result);
  }
}

// handles nastyness of logging with serial, needs work.
int debug(const char *str, ...) //Variadic Function
{
  int i, count=0, j=0, flag=0;
  char temp[16+1];
  for(i=0; str[i]!='\0';i++)  if(str[i]=='%')  count++; //Evaluate number of arguments required to be printed
  
  va_list argv;
  va_start(argv, count);
  for(i=0,j=0; str[i]!='\0';i++) //Iterate over formatting string
  {
    if(str[i]=='%')
    {
      //Clear buffer
      temp[j] = '\0'; 
      Serial.print(temp);
      j=0;
      temp[0] = '\0';
      
      //Process argument
      switch(str[++i])
      {
        case 'd': Serial.print(va_arg(argv, int));
                  break;
        case 'l': Serial.print(va_arg(argv, long));
                  break;
        case 'f': Serial.print(va_arg(argv, double));
                  break;
        case 'c': Serial.print((char)va_arg(argv, int));
                  break;
        case 's': Serial.print(va_arg(argv, char *));
                  break;
        default:  ;
      };
    }
    else 
    {
      //Add to buffer
      temp[j] = str[i];
      j = (j+1)%16;
      if(j==0)  //If buffer is full, empty buffer.
      {
        temp[16] = '\0';
        Serial.print(temp);
        temp[0]='\0';
      }
    }
  };
  
  Serial.println(); //Print trailing newline
  return count + 1; //Return number of arguments detected
}

