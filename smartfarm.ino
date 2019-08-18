// This #include statement was automatically added by the Particle IDE.
#include <LiquidCrystal_I2C_Spark.h>
#include <SparkJson.h>
#include <ThingSpeak.h>
#include "Adafruit_DHT.h"
#include "Particle.h"

#define DHTPIN 2            // what pin we're connected to
#define DHTTYPE DHT11		// DHT 11 
#define MOISTURE_PIN A2
#define FIRE_PIN D5
#define LDR_PIN A4

LiquidCrystal_I2C *lcd;

DHT dht(DHTPIN, DHTTYPE);
TCPClient client;

// forward declarations
void publishData();
void read_Fire();
void getDataHandler(const char *topic, const char *data);
void myGoogleSheetsHandler(const char *event, const char *data);

const unsigned long myChannelNumber = 824757 ;
const char *myWriteAPIKey = "4TIQ6CSI2RODFI1C";
const char *PUBLISH_EVENT_NAME = "Firebase_put";
const char *CHECK_EVENT_NAME = "Firebase_Read";

float temperature = 0.0;
float humidity = 0.0;
float heat_Index = 0.0;
float dew_Point = 0.0;
float light = 0.0;
float moisture = 0.0;
float voltage = 0.0;
float moisture_threshold = 0;
float light_threshold = 0;
String message="";
int messageCode = 0;
int alert=0;

unsigned long interval = 6000;
unsigned long previousMillis = 0;
unsigned long previousMillis2 = 0;
volatile int state = 0 ;

void setup() 
{
	Serial.begin(9600);

    lcd = new LiquidCrystal_I2C(0x27, 16, 2);
    lcd->init();
    lcd->backlight();
    lcd->clear();
    lcd->print("*** Welcome ***");
  	Particle.subscribe("hook-response/Firebase_Read", getDataHandler, MY_DEVICES);
  	Particle.subscribe("hook-response/CropRates", myGoogleSheetsHandler, MY_DEVICES);
	Serial.println("DHT11 test!");
    ThingSpeak.begin(client);
	dht.begin();
	
    pinMode(MOISTURE_PIN, INPUT);
    pinMode(D7,OUTPUT);
    pinMode(FIRE_PIN, INPUT);
    pinMode(LDR_PIN, INPUT);
    
    attachInterrupt(FIRE_PIN, read_Fire, CHANGE);        // an interrupt to trigger fire state change 
    
    readDht();                                       
    read_Moisture();
    read_Light();
    checkCritical();
    previousMillis = millis();
    previousMillis2 = millis();
   
}

void loop() 
{
    Serial.println("Not the time yet"); //
 
    if(millis() - previousMillis >= interval)        // we do not want climate data every second, I am updating values after every 6 seconds
    {
        Serial.println("This is the time");
        writeThingSpeak();
        publishData();
        readDht();                                       
       read_Moisture();
       read_Light();
       previousMillis = millis();
    }
    if (millis() - previousMillis2 >= 600000) //updating values every 10 minutes
    {
    lcd->clear();
		previousMillis2 = millis();
		Particle.publish(CHECK_EVENT_NAME, "", PRIVATE);
		Particle.publish("CropRates","", PRIVATE);
		checkCritical();
		lcd->setCursor(0,0);
		lcd->print(message);
    }
}

void getDataHandler(const char *topic, const char *data) 
{
	StaticJsonBuffer<255> jsonBuffer;
	char *mutableCopy = strdup(data);
	JsonObject& root = jsonBuffer.parseObject(mutableCopy);
	free(mutableCopy);
	// Because of the way the webhooks work, all data, including numbers, are represented as
	// strings, so we need to convert them back to their native data type here
	 moisture_threshold = atof(root["lightTh"]);
	 light_threshold = atof(root["waterTh"]);
	 alert= atoi(root["alert"]); //check whether there is an alert from mobile app or not 1 if true else 0
	Serial.printlnf("light=%.2f water=%.2f alert=%d", light_threshold,moisture_threshold, alert);
}

void writeThingSpeak()
{
    int volt_percent=map((int)voltage,0,5,0,100);
    
    ThingSpeak.setField(1, (float)temperature);
    ThingSpeak.setField(2, (float)humidity);
    ThingSpeak.setField(3, (float)dew_Point);
    ThingSpeak.setField(4, (float)heat_Index);
    ThingSpeak.setField(5, (float)light);
    ThingSpeak.setField(6, (float)moisture);
    ThingSpeak.setField(7, (float)volt_percent);
    
    // Write the fields that you've set all at once.
    ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
    Serial.println("Thingspeak");
    // Give time for the message to reach ThingSpeak
    delay(3000);
}


void readDht()
{
    // Wait a few seconds between measurements.
	delay(2000);
   // Reading temperature or humidity takes about 250 milliseconds!
   // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
	float h = dht.getHumidity();
    float t = dht.getTempCelcius();                                  // Read temperature as Celsius
	float f = dht.getTempFarenheit();                                // Read temperature as Farenheit
  
	if (isnan(h) || isnan(t) || isnan(f))                            // Check if any reads failed and exit early (to try again).
	{
		Serial.println("Failed to read from DHT sensor!");
		return;
	}

	float hi = dht.getHeatIndex();                                   // Compute heat index
	float dp = dht.getDewPoint();
	float k = dht.getTempKelvin();
	
	humidity = h;
	temperature = t;
	dew_Point = dp;
	heat_Index =hi;
// just to check values coming are correct or not
	Serial.print("Humid: "); 
	Serial.print(h);
	Serial.print("% - ");
	Serial.print("Temp: "); 
	Serial.print(t);
	Serial.print("*C ");
	Serial.print(f);
	Serial.print("*F ");
	Serial.print(k);
	Serial.print("*K - ");
	Serial.print("DewP: ");
	Serial.print(dp);
	Serial.print("*C - ");
	Serial.print("HeatI: ");
	Serial.print(hi);
	Serial.println("*C");
	Serial.println(Time.timeStr());

}

void read_Light()
{
    int lightTemp = analogRead(MOISTURE_PIN);
    Serial.println(lightTemp);
    light = (100 - ((lightTemp/4095)*100));
    Serial.print("light % ");
    Serial.print(light);
    delay(1000);
}

void read_Moisture()
{
  int moisture_analog = analogRead(MOISTURE_PIN); // read capacitive sensor
  Serial.println(moisture_analog);
  moisture = (100 - ( (moisture_analog/4095.00) * 100 ) );
    Serial.print("moisture % ");
    Serial.print(moisture);
    delay(1000);
}


void publishData() 
{
	char buf[256];
	snprintf(buf, sizeof(buf), "{\"temp\":%.2f,\"humid\":%.2f,\"dewpoint\":%.2f,\"heatindex\":%.2f,\"moist\":%.2f,\"volt\":%.2f,\"light\":%.2f,\"message\":%d}",
	temperature, humidity, dew_Point, heat_Index, moisture, voltage, light, messageCode);
	Serial.printlnf("publishing %s", buf);
	Particle.publish(PUBLISH_EVENT_NAME, buf, PRIVATE);
	delay(3000);
}

void read_Fire()
{
  state = digitalRead(FIRE_PIN);
  digitalWrite(D7, state);
}

void checkCritical()
{ // set your own custom message to keep a check on farmer's health owing to climatic conditions or track farm health.
    if(alert==1)
    message = "emergency";
    else if(light_threshold!=0&&moisture_threshold!=0)
    {
        if(light<light_threshold||moisture<moisture_threshold)
        {
             if(moisture<moisture_threshold)
             {
             message="Water LOW";
             messageCode=1;
             }
             else if(light<light_threshold)
             {
             message="Light LOW";
             messageCode=2;
             }
             else
             {
             message="Water Light LOW";
             messageCode=3;
             }
        }
       
    }
    else if(state==1 || state==HIGH)
    {
        message="Alert Fire";
        messageCode=4;
    }
    else if(heat_Index>27&&heat_Index<32)
    {
    message="! Heat Cramps";
    messageCode=5;
    }
    else if(heat_Index>32&&heat_Index<41)
    {
    message="! Heat Stroke";
    messageCode=6;
    }
    else if(heat_Index>41||heat_Index>54)
    {
    message="!Extreme danger";
    messageCode=7;
    }
    else if(dew_Point<10)
    {
    message="Skin irritation";
    messageCode=8;
    }
      else if(dew_Point>26)
   {
   message="deadly for asthma patients";
   messagCode=9;
   }
    else if(voltage<1)
    {
    message="battery low";
    messageCode=9;
    }
    else
    {
        message="Normal";
        messageCode=0;
    }
}
void myGoogleSheetsHandler(const char *event, const char *data) {
  // Handle the integration response and print the data to debug console 
   Serial.println( String(data) );
    StaticJsonBuffer<256> jsonBuffer;
	char *mutableCopy = strdup(data);
	JsonObject& root = jsonBuffer.parseObject(mutableCopy);
	free(mutableCopy);
	Serial.printlnf("data: %s", data);
}

void battery()
{
    voltage = analogRead(BATT) * 0.0011224;
    voltage = map(voltage,0.0,5.0,0.0,100.0);
}

