#include <Arduino.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <Wire.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <string>
#include <IRrecv.h>
#include <IRac.h>
#include <IRtext.h>
#include <IRutils.h>

#include <OneWire.h>
#include <DallasTemperature.h>

#include "settings.h"
#include "wifiConnection.h"

#include <ir_LG.h>  //  replace library based on your AC unit model, check https://github.com/crankyoldgit/IRremoteESP8266

#define STATUS_LED_GPIO 2

WiFiClient espClient;
PubSubClient client(espClient);
StaticJsonDocument<1200> state;

long refreshPrevMillis = 0;

int statusLedState = LOW;             // statusLedState used to set the LED
unsigned long statusLedPrevMillis = 0;        // will store last time LED was updated


// #############################
// ######## TEMP SENSOR ########
// #############################

// GPIO where the DS18B20 is connected to
const int oneWireBus = 4;     

// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(oneWireBus);

// Pass our oneWire reference to Dallas Temperature sensor 
DallasTemperature sensors(&oneWire);

unsigned long sensorPrevMillis = 0;



// #############################
// ########## IR READ ##########
// #############################

const uint16_t kRecvPin = 15; 


// As this program is a special purpose capture/decoder, let us use a larger
// than normal buffer so we can handle Air Conditioner remote codes.
const uint16_t kCaptureBufferSize = 1024;


// kTimeout is the Nr. of milli-Seconds of no-more-data before we consider a
// message ended.
// This parameter is an interesting trade-off. The longer the timeout, the more
// complex a message it can capture. e.g. Some device protocols will send
// multiple message packets in quick succession, like Air Conditioner remotes.
// Air Coniditioner protocols often have a considerable gap (20-40+ms) between
// packets.
// The downside of a large timeout value is a lot of less complex protocols
// send multiple messages when the remote's button is held down. The gap between
// them is often also around 20+ms. This can result in the raw data be 2-3+
// times larger than needed as it has captured 2-3+ messages in a single
// capture. Setting a low timeout value can resolve this.
// So, choosing the best kTimeout value for your use particular case is
// quite nuanced. Good luck and happy hunting.
// NOTE: Don't exceed kMaxTimeoutMs. Typically 130ms.
#if DECODE_AC
// Some A/C units have gaps in their protocols of ~40ms. e.g. Kelvinator
// A value this large may swallow repeats of some protocols
const uint8_t kTimeout = 50;
#else   // DECODE_AC
// Suits most messages, while not swallowing many repeats.
const uint8_t kTimeout = 15;
#endif  // DECODE_AC
// Alternatives:
// const uint8_t kTimeout = 90;
// Suits messages with big gaps like XMP-1 & some aircon units, but can
// accidentally swallow repeated messages in the rawData[] output.
//
// const uint8_t kTimeout = kMaxTimeoutMs;
// This will set it to our currently allowed maximum.
// Values this high are problematic because it is roughly the typical boundary
// where most messages repeat.
// e.g. It will stop decoding a message and start sending it to serial at
//      precisely the time when the next message is likely to be transmitted,
//      and may miss it.

// Set the smallest sized "UNKNOWN" message packets we actually care about.
// This value helps reduce the false-positive detection rate of IR background
// noise as real messages. The chances of background IR noise getting detected
// as a message increases with the length of the kTimeout value. (See above)
// The downside of setting this message too large is you can miss some valid
// short messages for protocols that this library doesn't yet decode.
//
// Set higher if you get lots of random short UNKNOWN messages when nothing
// should be sending a message.
// Set lower if you are sure your setup is working, but it doesn't see messages
// from your device. (e.g. Other IR remotes work.)
// NOTE: Set this value very high to effectively turn off UNKNOWN detection.
const uint16_t kMinUnknownSize = 12;

// How much percentage lee way do we give to incoming signals in order to match
// it?
// e.g. +/- 25% (default) to an expected value of 500 would mean matching a
//      value between 375 & 625 inclusive.
// Note: Default is 25(%). Going to a value >= 50(%) will cause some protocols
//       to no longer match correctly. In normal situations you probably do not
//       need to adjust this value. Typically that's when the library detects
//       your remote's message some of the time, but not all of the time.
const uint8_t kTolerancePercentage = kTolerance;  // kTolerance is normally 25%

// Legacy (No longer supported!)
//
// Change to `true` if you miss/need the old "Raw Timing[]" display.
#define LEGACY_TIMING_INFO false
// ==================== end of TUNEABLE PARAMETERS ====================

// Use turn on the save buffer feature for more complete capture coverage.
IRrecv irrecv(kRecvPin, kCaptureBufferSize, kTimeout, true);
decode_results results;  // Somewhere to store the results



// #############################
// ########## IR SEND ##########
// #############################

#define MODE_AUTO kLgAcAuto
#define MODE_COOL kLgAcCool
#define MODE_DRY kLgAcDry
#define MODE_HEAT kLgAcHeat
#define MODE_FAN kLgAcFan

#define FAN_AUTO kLgAcFanAuto
#define FAN_MIN kLgAcFanMin
#define FAN_MED kLgAcFanMed
#define FAN_HI kLgAcFanMax

// ESP8266 GPIO pin to use for IR blaster.
const uint16_t kIrLed = 16;
// Library initialization, change it according to the imported library file.
IRLgAc ac(kIrLed);

// settings
char deviceName[] = "AC Remote Control";


const std::string tempKey = "temp";
const std::string modeKey = "mode";
const std::string fanKey = "fan";
const std::string currentTempKey = "currentTemp";

void mqttSend(char* topic, char* message) {
    Serial.print("Sending MQTT: ");
    Serial.println(message);
    client.publish(topic, message, true);
}

void broadcastState() {

  if(state[tempKey] == 0) {
    Serial.println("Cancel broadcast due to lack of state data.");
    return;
  }
  Serial.println("Broadcasting state to MQTT..");
 
  char jsonString[256]; // max 256 o no se manda el MQTT!!!!  
  serializeJson(state, jsonString);

  mqttSend("esp32/ac/state", jsonString);
}

void sendStateToIr() {
  Serial.println("Sending IR..");

  irrecv.disableIRIn();

  if(state[modeKey] == "off") {
    ac.off();
  }
  else{
    ac.on();

    ac.setTemp(state[tempKey]);
    // ac.setMode(MODE_HEAT);
    ac.setFan(FAN_HI);

    if(state[modeKey] == "off") {
      ac.off();
    }
    else if(state[modeKey] == "auto") {
      ac.setMode(MODE_AUTO);
    }
    else if(state[modeKey] == "cool") {
      ac.setMode(MODE_COOL);
    }
    else if(state[modeKey] == "heat") {
      ac.setMode(MODE_HEAT);
    }
    else if(state[modeKey] == "dry") {
      ac.setMode(MODE_DRY);
    }
    else if(state[modeKey] == "fan_only") {
      ac.setMode(MODE_FAN);
      ac.setFan(FAN_HI);
    }
  }
    
  ac.send();

  delay(100);
  irrecv.enableIRIn();
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.println(); Serial.println("Receiving MQTT..");
  Serial.print("Topic: "); Serial.println(topic);
  Serial.print("Payload: ");
  
  char payloadString[length+1];
 
  int i=0;
  for (i=0;i<length;i++) {
    Serial.print((char)payload[i]);
    payloadString[i]=(char)payload[i];
  }
  payloadString[i] = 0; // Null termination
  Serial.println();
  
  
  StaticJsonDocument <256> payloadJson;
  deserializeJson(payloadJson,payload);
   
  if (String(topic) == "esp32/ac/set") {

      if(payloadJson.containsKey(tempKey)) {
        const int tempValue = payloadJson[tempKey];
        if (tempValue) {
          Serial.print(">> Updating TEMPERATURE: "); Serial.println(tempValue);
          state[tempKey] = tempValue;
        }
      }

      if(payloadJson.containsKey(modeKey)) {
        const String modeValue = payloadJson[modeKey];
        if (modeValue) {
          if (modeValue.length() > 1) {
            Serial.print(">> Updating MODE: "); Serial.println(modeValue);
            state[modeKey] = modeValue;
          }
        }
      }

      if(payloadJson.containsKey(fanKey)) {
        const String fanValue = payloadJson[fanKey];
        if (fanValue) {
          if (fanValue.length() > 1) {
            Serial.print(">> Updating FAN: "); Serial.println(fanValue);
            state[fanKey] = fanValue;
          }
        }
      }

    sendStateToIr();

    broadcastState();
  }

  if (String(topic) == "esp32/ac/state") {
    if(state[tempKey] == 0) {

      if(payloadJson.containsKey(tempKey)) {
        const int tempValue = payloadJson[tempKey];
        if (tempValue) {
          Serial.print(">> Updating retained TEMPERATURE: "); Serial.println(tempValue);
          state[tempKey] = tempValue;
        }
      }

      if(payloadJson.containsKey(modeKey)) {
        const String modeValue = payloadJson[modeKey];
        if (modeValue) {
          if (modeValue.length() > 1) {
            Serial.print(">> Updating retained MODE: "); Serial.println(modeValue);
            state[modeKey] = modeValue;
          }
        }
      }

      if(payloadJson.containsKey(fanKey)) {
        const String fanValue = payloadJson[fanKey];
        if (fanValue) {
          if (fanValue.length() > 1) {
            Serial.print(">> Updating retained FAN: "); Serial.println(fanValue);
            state[fanKey] = fanValue;
          }
        }
      }

    }
  }

}

void mqttReconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESP32_AC",MQTT_USER,MQTT_PASSWORD)) {
      Serial.println("MQTT connected");

      // Subscribe
      client.subscribe("esp32/ac/set");

      // To get the  broker retained status
      client.subscribe("esp32/ac/state");

    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(10000);
    }
  }
}


void mqttSetup() {
  client.setServer(MQTT_HOST, 1883);
  client.setCallback(mqttCallback);
}

void mqttLoop() {
  if (!client.connected()) {
    mqttReconnect();
  }
  client.loop();
}


void irSetup() {
 ac.begin();

#if DECODE_HASH
  // Ignore messages with less than minimum on or off pulses.
  irrecv.setUnknownThreshold(kMinUnknownSize);
#endif  // DECODE_HASH
  irrecv.setTolerance(kTolerancePercentage);  // Override the default tolerance.
  irrecv.enableIRIn();  // Start the receiver
}


// ########################################################
// MAIN
// ########################################################

void setup() {

  pinMode(STATUS_LED_GPIO, OUTPUT);
  digitalWrite(STATUS_LED_GPIO, HIGH);

  Serial.begin(115200);
  Serial.println("Rodrigo Butta AC Controller");

  wifiSetup();
  mqttSetup();
  irSetup();

  state[tempKey] = 0;
  state[modeKey] = "off";
  state[fanKey] = "";
  state[currentTempKey] = 0;

  Serial.print("PIN STATUS:"); Serial.println(STATUS_LED_GPIO);
  Serial.print("PIN IR TRANSMITTER:"); Serial.println(kIrLed);
  Serial.print("PIN IR RECEIVER:"); Serial.println(kRecvPin);
  Serial.print("PIN TEMP SENSOR:"); Serial.println(oneWireBus);

  // Start the DS18B20 sensor
  sensors.begin();
}

void statusLedLoop() {
  if (statusLedState == LOW) {
    statusLedState = HIGH;
  } else {
    statusLedState = LOW;
  }

  digitalWrite(STATUS_LED_GPIO, statusLedState);
}

void sensorLoop() {
  sensors.requestTemperatures(); 
  float newTemp = sensors.getTempCByIndex(0);

  // float newTempRounded = round(newTemp * 10.0 ) / 10.0;
  int newTempRounded = newTemp;
  if(newTempRounded != state[currentTempKey]) {
    state[currentTempKey] = newTempRounded;
    
    broadcastState();
  }
}

void irReceiverLoop() {
  if (irrecv.decode(&results)) {   
    Serial.println("Receiving IR..");

    // Display the basic output of what we found.
    Serial.print(resultToHumanReadableBasic(&results));
    // Display any extra A/C info if we have it.
    String description = IRAcUtils::resultAcToString(&results);
    if (description.length()) Serial.println(D_STR_MESGDESC ": " + description);


    stdAc::state_t decodedIr, p; 
    IRAcUtils::decodeToState(&results, &decodedIr, &p);

    Serial.print("Protocol: "); Serial.println(decodedIr.protocol);
    Serial.print("Model: "); Serial.println(decodedIr.model);

    if(decodedIr.protocol != decode_type_t::LG2 || decodedIr.model != 3) { // 3 es AKB74955603
      Serial.println("Not the spected remote");
      return;
    }


    String powerStr;
    switch (decodedIr.power) {
      case true:      powerStr = "on"; break;
      case false:     powerStr = "off"; break;
    }
    Serial.print("Power: "); Serial.println(powerStr);

    if(decodedIr.power == true) {
      
      String modeStr;
      switch (decodedIr.mode) {
        case stdAc::opmode_t::kOff:  modeStr = "off"; break;
        case stdAc::opmode_t::kAuto: modeStr = "auto"; break;
        case stdAc::opmode_t::kCool: modeStr = "cool"; break;
        case stdAc::opmode_t::kHeat: modeStr = "heat"; break;
        case stdAc::opmode_t::kDry:  modeStr = "dry"; break;
        case stdAc::opmode_t::kFan:  modeStr = "fan_only"; break;
        default:                     modeStr = "";
      }Serial.print("Mode: "); Serial.println(modeStr);
      
      Serial.print("Temp: "); Serial.println(decodedIr.degrees);

      String fanspeedStr;
      switch (decodedIr.fanspeed) {
        case stdAc::fanspeed_t::kAuto:   fanspeedStr = "auto"; break;
        case stdAc::fanspeed_t::kMax:    fanspeedStr = "max"; break;
        case stdAc::fanspeed_t::kHigh:   fanspeedStr = "high"; break;
        case stdAc::fanspeed_t::kMedium: fanspeedStr = "med"; break;
        case stdAc::fanspeed_t::kLow:    fanspeedStr = "low"; break;
        case stdAc::fanspeed_t::kMin:    fanspeedStr = "min"; break;
        default:                         fanspeedStr = "";
      }
      Serial.print("Fan: "); Serial.println(fanspeedStr);

      state[tempKey] = decodedIr.degrees;
      state[modeKey] = modeStr;
      state[fanKey] = fanspeedStr;

      broadcastState();
    }
    else {

      state[tempKey] = 0;
      state[modeKey] = "off";
      state[fanKey] = "";

      broadcastState();
    }
  }
}

void loop() {
  mqttLoop();

  irReceiverLoop();
  
  long interval = 2000;
  if(!client.connected()) {
    interval = 300;
  }

  unsigned long currentMillis = millis();

  if (currentMillis - statusLedPrevMillis >= 1000) {
    statusLedPrevMillis = currentMillis;
    statusLedLoop();
  }

  if (currentMillis - sensorPrevMillis >= 5000) {
    sensorPrevMillis = currentMillis;
    sensorLoop();
  }
}