#include <IRremoteESP8266.h>
#include <IRsend.h>

//// ###### User configuration space for AC library classes ##########

#include <ir_LG.h>  //  replace library based on your AC unit model, check https://github.com/crankyoldgit/IRremoteESP8266

// #define AUTO_MODE kCoolixAuto
// #define COOL_MODE kCoolixCool
// #define DRY_MODE kCoolixDry
// #define HEAT_MODE kCoolixHeat
// #define FAN_MODE kCoolixFan

// #define FAN_AUTO kCoolixFanAuto
// #define FAN_MIN kCoolixFanMin
// #define FAN_MED kCoolixFanMed
// #define FAN_HI kCoolixFanMax

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

void setup() {
  Serial.begin(115200);
  
  Serial.println("Begin Ac..");
  
  ac.begin();
}

void loop() {
  Serial.println("Send AC TEMP!!");

  // if (acState.powerStatus) {
    ac.on();
    ac.setTemp(20);
    // if (acState.operation == 0) {
    //   ac.setMode(AUTO_MODE);
    //   ac.setFan(FAN_AUTO);
    //   acState.fan = 0;
    // } else if (acState.operation == 1) {
    //   ac.setMode(COOL_MODE);
    // } else if (acState.operation == 2) {
    //   ac.setMode(DRY_MODE);
    // } else if (acState.operation == 3) {
    //   ac.setMode(HEAT_MODE);
    // } else if (acState.operation == 4) {
    //   ac.setMode(FAN_MODE);
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

  delay(2000);

  ac.on();
  ac.setTemp(21);
  ac.send();

  delay(2000);

  ac.on();
  ac.setTemp(22);
  ac.send();

  delay(2000);

  ac.on();
  ac.setTemp(21);
  ac.send();

  delay(2000);

}