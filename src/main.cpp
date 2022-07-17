#include <Arduino.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <Wire.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <string>
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

// int temp = 0;             // statusLedState used to set the LED


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
const uint16_t kIrLed = 4;
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


void mqttSend(char* topic, char* message) {
    Serial.print("MQTT SEND: ");
    Serial.println(message);
    client.publish(topic, message);
}

void broadcastState() {
 
  char jsonString[256]; // max 256 o no se manda el MQTT!!!!  
  serializeJson(state, jsonString);

  mqttSend("esp32/ac/state", jsonString);

  mqttSend("esp32/ac/sensor/state", "{\"temp\":12}");
}

void sendCommandToAC(int switchNumber) {


  Serial.println("Send AC TEMP!!");

  // if (acState.powerStatus) {
    ac.on();
    ac.setTemp(20);
    ac.setMode(MODE_HEAT);
    ac.setFan(FAN_HI);
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
  // } else {
    // ac.off();
  // }
  ac.send();
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  
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

          
    // al final de todo el proceso, porque si va en el medio corta el resto
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

// ########################################################
// MAIN
// ########################################################


void setup() {

  pinMode(STATUS_LED_GPIO, OUTPUT);
  digitalWrite(STATUS_LED_GPIO, HIGH);

  Serial.begin(115200);
  Serial.println("Hello Rodrigo Butta");

  
  wifiSetup();
  mqttSetup();
  
  ac.begin();
}


void refreshLoop() {
    // Convert the value to a char array
    char tempString[8];
    dtostrf(state[tempKey], 1, 2, tempString);
    Serial.print("Temperature: ");
    Serial.print(tempString);
    Serial.println();

    char modeString[8];
    dtostrf(state[modeKey], 1, 2, modeString);
    Serial.print("Mode: ");
    Serial.print(modeString);
    Serial.println();


}


void statusLedLoop() {

  if (statusLedState == LOW) {
    statusLedState = HIGH;
  } else {
    statusLedState = LOW;
  }

  digitalWrite(STATUS_LED_GPIO, statusLedState);

}

void loop() {
  mqttLoop();

  unsigned long currentMillis = millis();


  if (currentMillis - refreshPrevMillis > 5000) {
    refreshPrevMillis = currentMillis;
    
    refreshLoop();
  }

  long interval = 2000;
  if(!client.connected()) {
    interval = 300;
  }

  if (currentMillis - statusLedPrevMillis >= 1000) {
    statusLedPrevMillis = currentMillis;

    statusLedLoop();

  }



  // ac.on();
  // ac.setTemp(21);
  // ac.setMode(MODE_HEAT);
  // ac.setFan(FAN_HI);
  // ac.send();


}