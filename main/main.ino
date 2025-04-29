#include <WiFi.h>
#include <esp_wifi.h>
#include <FastLED.h>

// LED Configuration
#define LED_PIN     13
#define ROW_SIZE    8
#define COL_SIZE    8
#define LED_COUNT   (ROW_SIZE * COL_SIZE)
#define BRIGHTNESS  128

// Dynamic Configuration
uint32_t cooling = 30;
uint32_t frameRate = 30;
uint32_t sparking = 128;

// Packet Counter
uint32_t totalPacket = 0;
uint32_t totalByte = 0;

// Fire simulation data
uint8_t heat[LED_COUNT];

// LED data array
CRGB leds[LED_COUNT];

// Timestamp tracking
unsigned long lastStatusUpdate = 0;
unsigned long lastFrameUpdate = 0;

// Function to map 2D coordinates to 1D LED index
uint16_t mapXYToIndex(uint8_t x, uint8_t y) {
  return y * ROW_SIZE + x;
}

// Function to map a value from one range to another
uint32_t mapRange(uint32_t value, uint32_t fromLow, uint32_t fromHigh, uint32_t toLow, uint32_t toHigh) {
  return (value - fromLow) * (toHigh - toLow) / (fromHigh - fromLow) + toLow;
}

// WiFi packet callback function
void WiFiPacketCallback(void* buf, wifi_promiscuous_pkt_type_t type) {
  if (type != WIFI_PKT_DATA) return;
  
  wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
  totalPacket++;
  totalByte += pkt->rx_ctrl.sig_len;
}

// Fire simulation function
void fireSimulation() {
  // Step 1: Cool down every cell a little
  for (int y = 0; y < COL_SIZE; y++) {
    for (int x = 0; x < ROW_SIZE; x++) {
      uint16_t index = mapXYToIndex(x, y);
      uint8_t coolingAmount = random(0, (cooling * 10 / ROW_SIZE) + 2);
      if (heat[index] > coolingAmount) {
        heat[index] = heat[index] - coolingAmount;
      } else {
        heat[index] = 0;
      }
    }

    // Step 2: Heat spreads to neighbors
    for (int x = 1; x < ROW_SIZE - 1; x++) {
      uint16_t index = mapXYToIndex(x, y);
      heat[index] = (heat[mapXYToIndex(x - 1, y)] + 
                     heat[mapXYToIndex(x, y)] + 
                     heat[mapXYToIndex(x + 1, y)]) / 3;
    }

    // Step 3: Randomly ignite new sparks near the bottom
    if (random(255) < sparking && totalByte > 0) {
      int x = random(ROW_SIZE);
      uint16_t index = mapXYToIndex(x, y);
      heat[index] = min(heat[index] + 255, 255);
      
      // Consume some of the packet data
      if (totalByte > 128) {
        totalByte -= 128;
      } else {
        totalByte = 0;
      }
    }
  }

  // Step 4: Convert heat to LED colors
  for (int i = 0; i < LED_COUNT; i++) {
    uint8_t brightness = heat[i];
    uint8_t hue;
    uint8_t sat = 255;
    
    // Hotter = yellower
    hue = mapRange(255 - brightness, 0, 255, 80, 100);
    
    // Near-white hot spots
    if (brightness > 240) {
      sat = brightness - 240;
    }
    
    // Set the LED color
    leds[i] = CHSV(hue, sat, brightness);
  }
}

void setup() {
  // Initialize Serial
  Serial.begin(115200);
  Serial.println("ESP32 WiFi Fire Animation");

  // Initialize FastLED
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, LED_COUNT);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.clear();
  FastLED.show();

  // Initialize WiFi in promiscuous mode
  WiFi.mode(WIFI_STA);
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&cfg);
  esp_wifi_set_storage(WIFI_STORAGE_RAM);
  esp_wifi_set_mode(WIFI_MODE_STA);
  esp_wifi_start();

  // Set promiscuous mode and callback
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(&WiFiPacketCallback);

  // Initialize heat array
  for (int i = 0; i < LED_COUNT; i++) {
    heat[i] = 0;
  }

  // Initialize random seed
  randomSeed(analogRead(0));
}

void loop() {
  unsigned long currentMillis = millis();

  // Update animation at the specified frame rate
  if (currentMillis - lastFrameUpdate >= (1000 / frameRate)) {
    fireSimulation();
    FastLED.show();
    lastFrameUpdate = currentMillis;
  }

  // Print status update and adjust parameters every second
  if (currentMillis - lastStatusUpdate >= 1000) {
    Serial.printf("Total Packets: %u, Total Bytes: %u\n", totalPacket, totalByte);
    
    // Adjust fire parameters based on network activity
    cooling = mapRange(totalPacket, 0, 50, 0, 100);
    sparking = mapRange(totalByte, 0, 5000, 0, 255);
    
    // Reset counters
    totalPacket = 0;
    totalByte = 0;
    
    lastStatusUpdate = currentMillis;
  }
}