#define MINIMUM_DELAY 900


#include <heltec_unofficial.h>
#include <LoRaWAN_ESP32.h>
#include <time.h>
#include <stdint.h>

LoRaWANNode* node;

RTC_DATA_ATTR uint8_t count = 0;


void setup() {
  heltec_setup();
  Serial.printf("\n\n\nStarting(suffering) up again...\n\n\n");

  // Obtain directly after deep sleep
  // May or may not reflect room temperature, sort of. 
  float temp = heltec_temperature();
  Serial.printf("Temperature: %.1f °C\n", temp);

  // initialize radio
  Serial.println("Radio init");
  int16_t state = radio.begin();
  // if (state != RADIOLIB_ERR_NONE) {
  //   Serial.println("Radio did not initialize. We'll try again later.");
  //   return;
  // }


  if(!persist.isProvisioned()){
    Serial.printf("Saving provisioning data...\n");
    uint8_t appKey[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t nwkKey[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    persist.provision("EU868", NULL, 0x0000000000000001, 0x005e81fa12f4, appKey, nwkKey);
  } 
  
  node = persist.manage(&radio);
  
  // uint64_t dev_addr = node->getDevAddr();
  // Serial.println(dev_addr);
  // size_t slot = getTimeSlot(dev_addr) % 60;
  // Serial.println(slot);


  if (!node->isJoined()) {
    Serial.println("Could not join network. We'll try again later.");
    goToSleep();
  }

  // If we're still here, it means we joined, and we can send something

  // Manages uplink intervals to the TTN Fair Use Policy
  node->setDutyCycle(true, 1250);

  // Sending uplink message to server
  // uint8_t uplinkData[2];
  // uplinkData[0] = count++;
  // uplinkData[1] = temp + 100;

  const char* uplinkData = "UVWNOPQRSTUVWNOP";

  // state = node->sendReceive(uplinkData, sizeof(uplinkData), 1, downlinkData, &lenDown);
  state = node->uplink(uplinkData, 1);
  // state = node->sendReceive(uplinkData, 1, downlinkData, &lenDown);

  if(state == RADIOLIB_ERR_NONE || state == RADIOLIB_ERR_RX_TIMEOUT) {
    Serial.println("Message sent");
  } else {
    Serial.printf("uplink returned error %d, we'll try again later.\n", state);
  }


  //Receiving downlink from Network Server
  uint8_t downlinkData[256];
  size_t lenDown = sizeof(downlinkData);
  state = node->downlink(downlinkData, &lenDown);
  if(state == RADIOLIB_ERR_NONE || state == RADIOLIB_ERR_RX_TIMEOUT) {
    Serial.println("Data received:");
    for(int i = 0; i < lenDown; i++){
      Serial.printf("%02X ", downlinkData[i]);
    }
    Serial.println();

    // Decode and print the data as a UTF-8 string
    char decodedMessage[lenDown + 1]; // +1 for the null terminator
    memcpy(decodedMessage, downlinkData, lenDown);
    decodedMessage[lenDown] = '\0'; // Null terminate the string
    Serial.print("Decoded message: ");
    Serial.println(decodedMessage);
  } else {
    Serial.printf("downlink returned error %d, we'll try again later.\n", state);
  }


  goToSleep();    // Does not return, program starts over next round
}

void loop() {
  // This is never called. There is no repetition: we always go back to
  // deep sleep one way or the other at the end of setup()
}

// Send the device to sleep for 40 seconds
void goToSleep() {
  Serial.println("Going to deep sleep now");
  // allows recall of the session after deepsleep
  persist.saveSession(node);

  // and off to bed we go
  heltec_deep_sleep(40);
}

// Send the device to sleep to its desired time slot
void goToSleep(size_t slot) {
    Serial.println("Going to deep sleep now");
    // allows recall of the session after deepsleep
    persist.saveSession(node);

    time_t timer2;
    time(&timer2);

    struct tm *timeinfo;
    timeinfo = localtime(&timer2);
    int current_seconds = timeinfo->tm_sec;

    // Calculate the time slot in seconds (1 second per slot)
    int slot_time = (int)slot;

    // Calculate the remaining seconds until the next time slot
    int sleep_time = slot_time - current_seconds;
    if (sleep_time < 0) {
        sleep_time += 60;  // Adjust if the calculated sleep time is negative
    }

    Serial.printf("Next TX in %i s\n", sleep_time);
    delay(100);  // So message prints
    // and off to bed we go
    heltec_deep_sleep((uint32_t)sleep_time);
}

#define TOTAL_NUMBER_OF_TIME_SLOTS 64

size_t getTimeSlot(uint64_t dev_addr) {
    int sum = 0;

    // Iterate through each byte of the 64-bit integer
    for (int i = 0; i < 8; i++) {
        uint8_t byte = (dev_addr >> (i * 8)) & 0xFF; // Extract each byte
        sum += byte;
    }

    return sum % TOTAL_NUMBER_OF_TIME_SLOTS;
}