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

// const uint8_t kLgAcFanLowest = 0;  // 0b0000
// const uint8_t kLgAcFanLow = 1;     // 0b0001
// const uint8_t kLgAcFanMedium = 2;  // 0b0010
// const uint8_t kLgAcFanMax = 4;     // 0b0100
// const uint8_t kLgAcFanAuto = 5;    // 0b0101
// const uint8_t kLgAcFanLowAlt = 9;  // 0b1001
// const uint8_t kLgAcFanHigh = 10;   // 0b1010
// // Nr. of slots in the look-up table
// const uint8_t kLgAcFanEntries = kLgAcFanHigh + 1;
// const uint8_t kLgAcTempAdjust = 15;
// const uint8_t kLgAcMinTemp = 16;  // Celsius
// const uint8_t kLgAcMaxTemp = 30;  // Celsius
// const uint8_t kLgAcCool = 0;  // 0b000
// const uint8_t kLgAcDry = 1;   // 0b001
// const uint8_t kLgAcFan = 2;   // 0b010
// const uint8_t kLgAcAuto = 3;  // 0b011
// const uint8_t kLgAcHeat = 4;  // 0b100
// const uint8_t kLgAcPowerOff = 3;  // 0b11
// const uint8_t kLgAcPowerOn = 0;   // 0b00
// const uint8_t kLgAcSignature = 0x88;

// const uint32_t kLgAcOffCommand          = 0x88C0051;
// const uint32_t kLgAcLightToggle         = 0x88C00A6;

// const uint32_t kLgAcSwingVToggle        = 0x8810001;
// const uint32_t kLgAcSwingSignature      = 0x8813;
// const uint32_t kLgAcSwingVLowest        = 0x8813048;
// const uint32_t kLgAcSwingVLow           = 0x8813059;
// const uint32_t kLgAcSwingVMiddle        = 0x881306A;
// const uint32_t kLgAcSwingVUpperMiddle   = 0x881307B;
// const uint32_t kLgAcSwingVHigh          = 0x881308C;
// const uint32_t kLgAcSwingVHighest       = 0x881309D;
// const uint32_t kLgAcSwingVSwing         = 0x8813149;
// const uint32_t kLgAcSwingVAuto          = kLgAcSwingVSwing;
// const uint32_t kLgAcSwingVOff           = 0x881315A;
// const uint8_t  kLgAcSwingVLowest_Short      = 0x04;
// const uint8_t  kLgAcSwingVLow_Short         = 0x05;
// const uint8_t  kLgAcSwingVMiddle_Short      = 0x06;
// const uint8_t  kLgAcSwingVUpperMiddle_Short = 0x07;
// const uint8_t  kLgAcSwingVHigh_Short        = 0x08;
// const uint8_t  kLgAcSwingVHighest_Short     = 0x09;
// const uint8_t  kLgAcSwingVSwing_Short       = 0x14;
// const uint8_t  kLgAcSwingVAuto_Short        = kLgAcSwingVSwing_Short;
// const uint8_t  kLgAcSwingVOff_Short         = 0x15;

// // AKB73757604 Constants
// // SwingH
// const uint32_t kLgAcSwingHAuto            = 0x881316B;
// const uint32_t kLgAcSwingHOff             = 0x881317C;
// // SwingV
// const uint8_t  kLgAcVaneSwingVHighest     = 1;  ///< 0b001
// const uint8_t  kLgAcVaneSwingVHigh        = 2;  ///< 0b010
// const uint8_t  kLgAcVaneSwingVUpperMiddle = 3;  ///< 0b011
// const uint8_t  kLgAcVaneSwingVMiddle      = 4;  ///< 0b100
// const uint8_t  kLgAcVaneSwingVLow         = 5;  ///< 0b101
// const uint8_t  kLgAcVaneSwingVLowest      = 6;  ///< 0b110
// const uint8_t  kLgAcVaneSwingVSize        = 8;
// const uint8_t  kLgAcSwingVMaxVanes = 4;  ///< Max Nr. of Vanes


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


// struct state {
//   uint8_t temperature = 22, fan = 0, operation = 0;
//   bool powerStatus;
// };


// settings
char deviceName[] = "AC Remote Control";


const std::string tempKey = "temp";
const std::string modeKey = "mode";
const std::string fanKey = "fan";
const std::string currentTempKey = "currentTemp";

// bool welcomeBroadcast = false;


void mqttSend(char* topic, char* message) {
    Serial.print("Sending MQTT: ");
    Serial.println(message);
    client.publish(topic, message);
}

void broadcastState() {
  Serial.println("Broadcasting state to MQTT..");
 
  char jsonString[256]; // max 256 o no se manda el MQTT!!!!  
  serializeJson(state, jsonString);

  mqttSend("esp32/ac/state", jsonString);
}

void sendStateToIr() {
  Serial.println("Sending IR..");

  // DISABLE INNNN ***

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
    // if (acState.operation == 0) {
    //   ac.setMode(MODE_AUTO);
    //   ac.setFan(FAN_AUTO);
    //   acState.fan = 0;
    // } else if (acState.operation == 1) {
    //   ac.setMode(MODE_COOL);
    // } else if (acState.operation == 2) {
    //   ac.setMode(MODE_DRY);
    // } else if (acState.operation == 3) {
    //   ac.setMode(MODE_HEAT);
    // } else if (acState.operation == 4) {
    //   ac.setMode(MODE_FAN);
    // }

    // if (acState.operation != 0) {
    //   if (acState.fan == 0) {
    //     ac.setFan(FAN_AUTO);
    //   } else if (acState.fan == 1) {
    //     ac.setFan(FAN_MIN);
    //   } else if (acState.fan == 2) {
    //     ac.setFan(FAN_MED);
    //   } else if (acState.fan == 3) {
    //     ac.setFan(FAN_HI);
    //   }
    // }
  }
    
  ac.send();

  delay(100);
  irrecv.enableIRIn();

  // ENABLE INNNNNN IR
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.println("Receiving MQTT..");
  Serial.print("Topic: ");
  Serial.println(topic);
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
          Serial.println(">> Updating TEMPERATURE...");
          state[tempKey] = tempValue;
        }
      }

      if(payloadJson.containsKey(modeKey)) {
        const String modeValue = payloadJson[modeKey];
        if (modeValue) {
          if (modeValue.length() > 1) {
            Serial.println(">> Updating MODE...");
            state[modeKey] = modeValue;
          }
        }
      }

      if(payloadJson.containsKey(fanKey)) {
        const String fanValue = payloadJson[fanKey];
        if (fanValue) {
          if (fanValue.length() > 1) {
            Serial.println(">> Updating FAN...");
            state[fanKey] = fanValue;
          }
        }
      }

    sendStateToIr();

    broadcastState();
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

      // if(welcomeBroadcast == false) {
      //   welcomeBroadcast == true;
      //   broadcastState();
      // }
      
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


  state[tempKey] = 20;
  state[modeKey] = "off";
  state[fanKey] = "max";
  state[currentTempKey] = 0;

  Serial.print("PIN STATUS:");
  Serial.println(STATUS_LED_GPIO);

  Serial.print("PIN IR TRANSMITTER:");
  Serial.println(kIrLed);

  Serial.print("PIN IR RECEIVER:");
  Serial.println(kRecvPin);

  Serial.print("PIN TEMP SENSOR:");
  Serial.println(oneWireBus);

  // Start the DS18B20 sensor
  sensors.begin();
}


// void refreshLoop() {
    // String modeString = state[modeKey];
    // Serial.print("Mode: ");
    // Serial.print(modeString);
    // Serial.println();

    // char tempString[8];
    // dtostrf(state[tempKey], 1, 2, tempString);
    // Serial.print("Temperature: ");
    // Serial.print(tempString);
    // Serial.println();

    // String fanString = state[fanKey];
    // Serial.print("Fan: ");
    // Serial.print(fanString);
    // Serial.println();

    // broadcastState();
// }


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

  //  Serial.print(newTemp);
  // Serial.println("ºC");
}


void irReceiverLoop() {
  if (irrecv.decode(&results)) {   
    Serial.println("Receiving IR..");

    stdAc::state_t decodedIr, p; 
    IRAcUtils::decodeToState(&results, &decodedIr, &p);

    Serial.print("Power: ");
    String powerStr;
    switch (decodedIr.power) {
      case true:      powerStr = "on"; break;
      case false:     powerStr = "off"; break;
    }
    Serial.println(powerStr);

    Serial.print("Mode: ");
    String modeStr;
    switch (decodedIr.mode) {
      case stdAc::opmode_t::kOff:  modeStr = "off"; break;
      case stdAc::opmode_t::kAuto: modeStr = "auto"; break;
      case stdAc::opmode_t::kCool: modeStr = "cool"; break;
      case stdAc::opmode_t::kHeat: modeStr = "heat"; break;
      case stdAc::opmode_t::kDry:  modeStr = "dry"; break;
      case stdAc::opmode_t::kFan:  modeStr = "fan_only"; break;
      default:                     modeStr = "kUnknownStr";
    }
    if(decodedIr.power == false) {
      modeStr = "off";
    }
    Serial.println(modeStr);
    
    Serial.print("Temp: ");
    Serial.println(decodedIr.degrees);

    Serial.print("Fan: ");
    String fanspeedStr;
    switch (decodedIr.fanspeed) {
      case stdAc::fanspeed_t::kAuto:   fanspeedStr = "auto"; break;
      case stdAc::fanspeed_t::kMax:    fanspeedStr = "max"; break;
      case stdAc::fanspeed_t::kHigh:   fanspeedStr = "high"; break;
      case stdAc::fanspeed_t::kMedium: fanspeedStr = "med"; break;
      case stdAc::fanspeed_t::kLow:    fanspeedStr = "low"; break;
      case stdAc::fanspeed_t::kMin:    fanspeedStr = "min"; break;
      default:                         fanspeedStr = "unknown";
    }
    Serial.println(fanspeedStr);

    state[tempKey] = decodedIr.degrees;
    state[modeKey] = modeStr;
    state[fanKey] = fanspeedStr;

    // Serial.print("SwingV: ");
    // String swingvStr;
    // switch (decodedIr.swingv) {
    //   case stdAc::swingv_t::kOff:     swingvStr = "kOffStr"; break;
    //   case stdAc::swingv_t::kAuto:    swingvStr = "kAutoStr"; break;
    //   case stdAc::swingv_t::kHighest: swingvStr = "kHighestStr"; break;
    //   case stdAc::swingv_t::kHigh:    swingvStr = "kHighStr"; break;
    //   case stdAc::swingv_t::kMiddle:  swingvStr = "kMiddleStr"; break;
    //   case stdAc::swingv_t::kLow:     swingvStr = "kLowStr"; break;
    //   case stdAc::swingv_t::kLowest:  swingvStr = "kLowestStr"; break;
    //   default:                        swingvStr = "kUnknownStr"; break;
    // }
    // Serial.println(swingvStr);

    // Serial.print("SwingH: ");
    // String swinghStr;
    // switch (decodedIr.swingh) {
    //   case stdAc::swingh_t::kOff:      swinghStr = "kOffStr"; break;
    //   case stdAc::swingh_t::kAuto:     swinghStr = "kAutoStr"; break;
    //   case stdAc::swingh_t::kLeftMax:  swinghStr = "kLeftMaxStr"; break;
    //   case stdAc::swingh_t::kLeft:     swinghStr = "kLeftStr"; break;
    //   case stdAc::swingh_t::kMiddle:   swinghStr = "kMiddleStr"; break;
    //   case stdAc::swingh_t::kRight:    swinghStr = "kRightStr"; break;
    //   case stdAc::swingh_t::kRightMax: swinghStr = "kRightMaxStr"; break;
    //   case stdAc::swingh_t::kWide:     swinghStr = "kWideStr"; break;
    //   default:                         swinghStr = "kUnknownStr"; break;
    // }
    // Serial.println(swinghStr);

    //     struct state_t {
    //   decode_type_t protocol = decode_type_t::UNKNOWN;
    //   int16_t model = -1;  // `-1` means unused.
    //   bool power = false;
    //   stdAc::opmode_t mode = stdAc::opmode_t::kOff;
    //   float degrees = 25;
    //   bool celsius = true;
    //   stdAc::fanspeed_t fanspeed = stdAc::fanspeed_t::kAuto;
    //   stdAc::swingv_t swingv = stdAc::swingv_t::kOff;
    //   stdAc::swingh_t swingh = stdAc::swingh_t::kOff;
    //   bool quiet = false;
    //   bool turbo = false;
    //   bool econo = false;
    //   bool light = false;
    //   bool filter = false;
    //   bool clean = false;
    //   bool beep = false;
    //   int16_t sleep = -1;  // `-1` means off.
    //   int16_t clock = -1;  // `-1` means not set.
    // };
    
    broadcastState();

    // yield();  // Feed the WDT as the text output can take a while to print.
    // Serial.println();    // Blank line between entries
    // yield();             // Feed the WDT (again)
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

  // if (currentMillis - refreshPrevMillis > 10000) {
  //   refreshPrevMillis = currentMillis;
  //   refreshLoop();
  // }

  if (currentMillis - statusLedPrevMillis >= 1000) {
    statusLedPrevMillis = currentMillis;
    statusLedLoop();
  }

  if (currentMillis - sensorPrevMillis >= 5000) {
    sensorPrevMillis = currentMillis;
    sensorLoop();
  }
}