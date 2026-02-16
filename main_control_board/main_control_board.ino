// display control based on this tutorial: https://randomnerdtutorials.com/esp32-tft-touchscreen-display-2-8-ili9341-arduino
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <esp_now.h>
#include <WiFi.h>

// ESP-NOW
// broadcast address (sends to everyone)
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

typedef struct temps_message {
  uint16_t actualTemp;
  uint16_t desiredTemp;
} temps_message;

temps_message myData;
esp_now_peer_info_t peerInfo;

TFT_eSPI tft = TFT_eSPI();
#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33
SPIClass touchscreenSPI = SPIClass(VSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define TFT_BL 21

#define BOILER_THRESHOLD 0.5
uint16_t currentTemp = 225;
uint16_t desiredTemp = 220;
bool boilerOn = false;
unsigned long lastTouchTime = 0;
bool changed = false;


struct Button {
  int x, y, w, h;
  String label;
  uint16_t color;
};

Button btnUp = {220, 50, 80, 60, "+", TFT_BLUE};
Button btnDown = {220, 130, 80, 60, "-", TFT_BLUE};

void drawUI();
void updateTemps();
void drawButton(Button b);
bool isButtonPressed(Button b, int tx, int ty);

// ESP-NOW recieve callback
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  temps_message incomingReadings;
  memcpy(&incomingReadings, incomingData, sizeof(incomingReadings));
  currentTemp = incomingReadings.actualTemp;
  desiredTemp = incomingReadings.desiredTemp;
  updateTemps();
}

void setup() {
  Serial.begin(115200);

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  // Touch setup
  touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  touchscreen.begin(touchscreenSPI);
  touchscreen.setRotation(1);

  // Display setup
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  // ESP-NOW setup
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Callback register
  esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));
  //esp_now_register_send_cb(OnDataSent);

  // Peer register (broadcast)
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }

  drawUI();
}

void loop() {
  // touch handling
  if (touchscreen.tirqTouched() && touchscreen.touched()) {
    if (millis() - lastTouchTime > 150) {
      TS_Point p = touchscreen.getPoint();
      
      // touch calibration
      int tx = map(p.x, 200, 3700, 1, SCREEN_WIDTH);
      int ty = map(p.y, 240, 3800, 1, SCREEN_HEIGHT);
      int tz = p.z;

      if (tz > 100) {
        if (isButtonPressed(btnUp, tx, ty)) {
          desiredTemp += 5;
          changed = true;
        }
        else if (isButtonPressed(btnDown, tx, ty)) {
          desiredTemp -= 5;
          changed = true;
        }
        if (changed) {
          updateTemps(); // Update screen
          sendData();    // Sync to Slave
          changed = false;
        }
      }
      lastTouchTime = millis();
    }
  }

  // Boiler control -> turns on is the temp drops by defined threshold
  // under the desired temp
  // (so it doesnt switch on off very often), then turns of if desired temp is reached
  bool shouldBoilerBeOn = currentTemp < (desiredTemp - BOILER_THRESHOLD);
  
  if (shouldBoilerBeOn != boilerOn) {
    boilerOn = shouldBoilerBeOn;
    updateTemps();
  }

}

void sendData() {
  myData.actualTemp = currentTemp; // Send back what we know (or 0)
  myData.desiredTemp = desiredTemp;
  esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));
}

void drawUI() {
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  //tft.setTextDatum(TL_DATUM);
  tft.drawString("Home thermostat control", 10, 10, 2);

  drawButton(btnUp);
  drawButton(btnDown);

  updateTemps();
}

void drawButton(Button b) {
  tft.fillRect(b.x, b.y, b.w, b.h, b.color);
  tft.drawRect(b.x, b.y, b.w, b.h, TFT_WHITE); // blue rect with white outline
  tft.setTextColor(TFT_WHITE, b.color);
  tft.setTextDatum(MC_DATUM); // Middle Center
  tft.drawString(b.label, b.x + (b.w/2), b.y + (b.h/2), 4);
}

void updateTemps() {
  // actual temp
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("Current temp:", 20, 60, 2);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(String(currentTemp / 10.0, 1) + " C", 100, 100, 6);

  // desired temp
  tft.setTextDatum(TL_DATUM);
  tft.drawString("Target temp:", 20, 160, 2);
  tft.setTextDatum(ML_DATUM);
  tft.drawString(String(desiredTemp / 10.0, 1) + " C", 100, 175, 4);

  // boiler status
  int circleColor = boilerOn ? TFT_RED : TFT_GREEN;
  String statusText = boilerOn ? "HEATING" : "IDLE";
  
  tft.fillRect(200, 200, 120, 40, TFT_BLACK); // "clear" boiler text and status
  tft.drawRect(200, 200, 120, 40, TFT_BLACK);
  tft.fillCircle(280, 210, 15, circleColor);
  tft.setTextColor(circleColor, TFT_BLACK);
  tft.setTextDatum(MR_DATUM);
  tft.drawString(statusText, 255, 210, 2);
}

bool isButtonPressed(Button b, int tx, int ty) {
  return (tx > b.x && tx < (b.x + b.w) && ty > b.y && ty < (b.y + b.h));
}