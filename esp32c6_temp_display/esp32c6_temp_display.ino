/*
Device found at 0x38, type = AHT20, capability bits = 0x3
Device found at 0x3C, type = Unknown, capability bits = 0x3
*/
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_AHTX0.h>
#include <esp_now.h>
#include <WiFi.h>

// ESP-NOW
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

typedef struct temps_message {
  uint16_t actualTemp;
  uint16_t desiredTemp;
} temps_message;

temps_message myData;
esp_now_peer_info_t peerInfo;

// buttons
#define BUTTON_UP 18
#define BUTTON_DOWN 19

// i2c (common for temp, oled modules)
#define SDA_PIN 14
#define SCL_PIN 9

// oled display
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET     -1
#define SCREEN_ADDRESS 0x3c
#define TEMP_CIRCLE_CODE 247
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);


// AHT10 temp sensor
Adafruit_AHTX0 aht10;
sensors_event_t aht10Temp, aht10Hum;
volatile uint16_t actualTemp;
volatile float actualHum;
volatile uint16_t desiredTemp = 225;

// debounce and delays
unsigned long lastDebounceUp = 0;
unsigned long lastDebounceDown = 0;
const int debounceDelay = 50;
volatile bool changed = false;

unsigned long lastTempUpdate = 0;
const int updateRate = 1000;

// ESP-NOW callbacks
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  temps_message incomingReadings;
  memcpy(&incomingReadings, incomingData, sizeof(incomingReadings));
  
  desiredTemp = incomingReadings.desiredTemp;
  Serial.print("New Target Received: ");
  Serial.println(desiredTemp);
}

void sendData() {
  myData.actualTemp = actualTemp;
  myData.desiredTemp = desiredTemp;
  esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));
}

void IRAM_ATTR buttonUpInterrupt() {
  unsigned long currentTime = millis();
  if (currentTime - lastDebounceUp > debounceDelay) {
    if (desiredTemp < 400) {
      desiredTemp += 5;
      changed = true;
    }
    lastDebounceUp = currentTime;
  }
}

void IRAM_ATTR buttonDownInterrupt() {
  unsigned long currentTime = millis();
  if (currentTime - lastDebounceDown > debounceDelay) {
    if (desiredTemp > 0) {
      desiredTemp -= 5;
      changed = true;
    }
    lastDebounceDown = currentTime;
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);
  delay(100);
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("Couldnt find oled display"));
    for(;;); // infinite loop bcs no display
  }

  // ESP-NOW setup
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) return;

  esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));

  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(F("OLED OK init"));
  display.display();
  delay(100);


  if (! aht10.begin()) {
    Serial.println(F("Couldnt find ATH10"));
    for(;;); // infinite loop bcs no temp module
  } else{
    Serial.println(F("AHT10 found"));
  }

  // buttons setup
  pinMode(BUTTON_UP, INPUT_PULLUP);
  pinMode(BUTTON_DOWN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON_UP), buttonUpInterrupt, FALLING);
  attachInterrupt(digitalPinToInterrupt(BUTTON_DOWN), buttonDownInterrupt, FALLING);
}


void displayTemps() {
  Serial.print("temp: ");
  Serial.print(aht10Temp.temperature);
  Serial.print("hum: ");
  Serial.println(aht10Hum.relative_humidity);

  display.clearDisplay();
  display.setCursor(15, 15);
  display.setTextSize(3);
  display.print(aht10Temp.temperature, 1);
  display.write(TEMP_CIRCLE_CODE);
  display.println("C");

  display.setTextSize(1);
  display.setCursor(15, 50);
  display.print(aht10Hum.relative_humidity, 1);
  display.println("%");

  display.setCursor(80, 50);
  display.print(desiredTemp / 10.0, 1);
  display.write(TEMP_CIRCLE_CODE);
  display.println("C");

  display.display();
}


void loop() {
  if (millis() - lastTempUpdate > updateRate) {
    aht10.getEvent(&aht10Hum, &aht10Temp);
    actualTemp = (uint16_t) (aht10Temp.temperature * 10);
    actualHum = aht10Hum.relative_humidity;
    sendData();
    displayTemps();
    lastTempUpdate = millis();
  }
  
  if (changed) {
    sendData();
    displayTemps();
    changed = false;
  }
}
