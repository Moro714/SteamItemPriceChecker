#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <XPT2046.h>
#include <ESP8266WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>

// WiFi credentials
const char* ssid     = "Orange-MhP7-2.4G";
const char* password = "yM7nh2k5";

// TFT display pins
#define TFT_CS   15 // D8
#define TFT_DC   2  // D4
#define TFT_RST  -1 // Use -1 if tied to RST
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

// Touchscreen pins
#define TOUCH_CS 4  // D2
#define TOUCH_IRQ 5 // D1
XPT2046 touch(TOUCH_CS, TOUCH_IRQ);

// NTP setup
WiFiUDP ntpUDP;
const long utcOffsetInSeconds = 3600 * 3;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds, 60000);

// Timer management
unsigned long lastPriceUpdate = 0;
unsigned long lastDateUpdate = 0;
unsigned long priceUpdateInterval = 20UL * 60UL * 1000UL;
unsigned long dateUpdateInterval = 24UL * 60UL * 60UL * 1000UL;

// Inventory items
struct CaseItem {
  const char* name;
  int quantity;
};

CaseItem inventory[] = {
  {"CS20%20Case", 5},
  {"Danger%20Zone%20Case", 4},
  {"Snakebite%20Case", 20},
  {"Clutch%20Case", 7},
  {"Falchion%20Case", 7},
  {"Shadow%20Case", 2},
  {"Chroma%202%20Case", 1},
  {"Horizon%20Case", 2},
  {"Prisma%20Case", 4},
  {"Prisma%202%20Case", 1},
  {"Recoil%20Case", 34},
  {"Fracture%20Case", 4},
  {"Dreams%20%26%20Nightmares%20Case", 1}
};

float totalValue = 0.0;

String formatItemName(const char* rawName) {
  String name = rawName;
  name.replace("%20", " ");
  name.replace("%26", "&");
  return name;
}

void fetchPrice(const char* marketHashName, int quantity) {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure client;
    client.setInsecure(); // Skip certificate validation for testing
    HTTPClient https;
    
    String url = "https://steamcommunity.com/market/priceoverview/?country=US&currency=1&appid=730&market_hash_name=" + String(marketHashName);
    https.begin(client, url);  // HTTPS begin
    https.addHeader("User-Agent", "Mozilla/5.0");  // Pretend to be a browser
    
    int httpCode = https.GET();

    if (httpCode == HTTP_CODE_OK) {
      String payload = https.getString();
      Serial.println("Payload: " + payload);
      DynamicJsonDocument doc(1024);
      DeserializationError error = deserializeJson(doc, payload);

      if (!error && doc["success"] == true && doc["lowest_price"]) {
        String priceStr = doc["lowest_price"].as<String>();
        priceStr.replace("$", "");
        priceStr.replace(",", "");
        float price = priceStr.toFloat();
        totalValue += price * quantity;
      } else {
        Serial.println("Failed to parse price or success=false");
      }
    } else {
      Serial.print("HTTP error for ");
      Serial.print(marketHashName);
      Serial.print(": ");
      Serial.println(httpCode);
    }

    https.end();
  }
}



void updateDateTimeDisplay() {
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_GREEN, ILI9341_BLACK);
  tft.setCursor(20, 40);
  tft.print("Date: " + timeClient.getFormattedTime().substring(0, 5));

  tft.setCursor(20, 60);
  tft.print("Time: " + timeClient.getFormattedTime());
}

void updatePriceDisplay() {
  totalValue = 0.0;
  int totalItems = sizeof(inventory) / sizeof(inventory[0]);

  for (int i = 0; i < totalItems; i++) {
    float progress = ((i + 1) / (float)totalItems) * 100.0;
    tft.setTextSize(3);
    tft.fillRect(60, 120, 200, 30, ILI9341_BLACK);
    tft.setCursor(100, 120);
    tft.print(String((int)progress) + "%");

    tft.setTextSize(2);
    tft.fillRect(20, 160, 220, 20, ILI9341_BLACK);
    tft.setCursor(20, 160);
    tft.print("Loading:");

    tft.fillRect(20, 180, 220, 20, ILI9341_BLACK);
    tft.setCursor(20, 180);
    tft.print(formatItemName(inventory[i].name));

    fetchPrice(inventory[i].name, inventory[i].quantity);
    delay(1500);
  }

  tft.setTextSize(2);
  tft.fillRect(20, 80, 200, 20, ILI9341_BLACK);
  tft.setCursor(20, 80);
  tft.print("Total Value: $");
  tft.print(String(totalValue, 2));

  tft.setTextSize(3);
  tft.fillRect(60, 120, 200, 30, ILI9341_BLACK);
  tft.setCursor(100, 120);
  tft.print("Done");

  tft.setTextSize(2);
  tft.fillRect(20, 160, 220, 20, ILI9341_BLACK);
  tft.fillRect(20, 180, 220, 20, ILI9341_BLACK);
}

void setup() {
  Serial.begin(9600);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi Connected");

  timeClient.begin();
  tft.begin();
  tft.setRotation(0);
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextColor(ILI9341_GREEN);

  timeClient.update();
  updateDateTimeDisplay();
  updatePriceDisplay();

  lastPriceUpdate = millis();
  lastDateUpdate = millis();
}

void loop() {
  timeClient.update();

  tft.setTextSize(2);
  tft.setTextColor(ILI9341_GREEN, ILI9341_BLACK);
  tft.setCursor(90, 60);
  tft.print(timeClient.getFormattedTime());

  if (millis() - lastPriceUpdate > priceUpdateInterval) {
    updatePriceDisplay();
    lastPriceUpdate = millis();
  }

  if (millis() - lastDateUpdate > dateUpdateInterval) {
    updateDateTimeDisplay();
    lastDateUpdate = millis();
  }

  delay(1000);
}
