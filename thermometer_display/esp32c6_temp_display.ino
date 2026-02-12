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
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);


// AHT10 temp sensor
Adafruit_AHTX0 aht10;
sensors_event_t aht10Temp, aht10Hum;
volatile float temp;
volatile float hum;
volatile uint16_t target_temp = 225;

// debounce and delays
unsigned long lastDebounceUp = 0;
unsigned long lastDebounceDown = 0;
const int debounceDelay = 50;
volatile bool changed = false;

unsigned long lastTempUpdate = 0;
const int updateRate = 1000;


void IRAM_ATTR buttonUpInterrupt() {
  unsigned long currentTime = millis();
  if (currentTime - lastDebounceUp > debounceDelay) {
    if (target_temp < 400) {
      target_temp += 5;
      changed = true;
    }
    lastDebounceUp = currentTime;
  }
}

void IRAM_ATTR buttonDownInterrupt() {
  unsigned long currentTime = millis();
  if (currentTime - lastDebounceDown > debounceDelay) {
    if (target_temp > 0) {
      target_temp -= 5;
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
  display.write(247);
  display.println("C");

  display.setTextSize(1);
  display.setCursor(15, 50);
  display.print(aht10Hum.relative_humidity, 1);
  display.println("%");

  display.setCursor(80, 50);
  display.print(target_temp / 10.0, 1);
  display.write(247);
  display.println("C");

  display.display();
}


void loop() {
  if (millis() - lastTempUpdate > updateRate) {
    aht10.getEvent(&aht10Hum, &aht10Temp);
    temp = aht10Temp.temperature;
    hum = aht10Hum.relative_humidity;
    displayTemps();
    lastTempUpdate = millis();
  }
  
  if (changed) {
    displayTemps();
    changed = false;
  }
}
