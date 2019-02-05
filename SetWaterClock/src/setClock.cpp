// CONNECTIONS:
// DS1307 SDA --> SDA
// DS1307 SCL --> SCL
// DS1307 VCC --> 5v
// DS1307 GND --> GND

#define countof(a) (sizeof(a) / sizeof(a[0]))

/* for software wire use below
#include <SoftwareWire.h>  // must be included here so that Arduino library object file references work
#include <RtcDS1307.h>

SoftwareWire myWire(SDA, SCL);
RtcDS1307<SoftwareWire> Rtc(myWire);
 for software wire use above */

/* for normal hardware wire use below */
#include <Wire.h> // must be included here so that Arduino library object file references work
#include <RtcDS1307.h>
RtcDS1307<TwoWire> Rtc(Wire);
/* for normal hardware wire use above */

extern void printDateTime(const RtcDateTime& dt);

uint32_t s_water_count = 0;

void setup () 
{
    Serial.begin(/*57600*/9600);

    Serial.print("compiled: ");
    Serial.print(__DATE__);
    Serial.println(__TIME__);

    //--------RTC SETUP ------------
    // if you are using ESP-01 then uncomment the line below to reset the pins to
    // the available pins for SDA, SCL
    // Wire.begin(0, 2); // due to limited pins, use pin 0 and 2 for SDA, SCL
    
    Rtc.Begin();

    RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
    printDateTime(compiled);
    Serial.println();

    if (!Rtc.IsDateTimeValid()) 
    {
        Serial.println("RTC lost confidence in the DateTime!");
        Rtc.SetDateTime(compiled);
    }

    if (!Rtc.GetIsRunning())
    {
        Serial.println("RTC was not actively running, starting now");
        Rtc.SetIsRunning(true);
    }

    RtcDateTime now = Rtc.GetDateTime();
    if (now < compiled) 
    {
        Serial.println("RTC is older than compile time!  (Updating DateTime)");
        Rtc.SetDateTime(compiled);
    }

    // never assume the Rtc was last configured by you, so
    // just clear them to your needed state
    Rtc.SetSquareWavePin(DS1307SquareWaveOut_Low); 

    /* comment out on a second run to see that the info is stored long term */
    // Store something in memory on the RTC
    Rtc.SetMemory(0, (const uint8_t*)&s_water_count, sizeof(s_water_count)); 
    /* end of comment out section */
}

void loop () 
{
    if (!Rtc.IsDateTimeValid()) 
    {
        // Common Cuases:
        //    1) the battery on the device is low or even missing and the power line was disconnected
        Serial.println("RTC lost confidence in the DateTime!");
    }

    RtcDateTime now = Rtc.GetDateTime();

    printDateTime(now);
    Serial.println();

    delay(5000);

    // read data
    // get our data from the address with the given size
    uint8_t gotten = Rtc.GetMemory(0, (uint8_t*)&s_water_count, sizeof(s_water_count));
    Serial.print("data read (");
    Serial.print(s_water_count, DEC);
    Serial.println("\"");
    delay(5000);
}



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
