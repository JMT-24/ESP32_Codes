#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include "esp_camera.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// WiFi credentials
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// Telegram bot credentials
#define BOT_TOKEN "YOUR_BOT_TOKEN"
#define CHAT_ID "YOUR_CHAT_ID"

WiFiClientSecure clientTCP;
UniversalTelegramBot bot(BOT_TOKEN, clientTCP);

// Camera pins for AI-Thinker ESP32-CAM
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

unsigned long lastTimeBotRan;
const unsigned long botRequestDelay = 2000;

void configCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  
  // CHECK IF PSRAM IS AVAILABLE
  if(psramFound()){
    Serial.println("✅ PSRAM FOUND!");
    config.frame_size = FRAMESIZE_UXGA;  // 1600x1200 high quality!
    config.jpeg_quality = 10;
    config.fb_count = 2;
    config.fb_location = CAMERA_FB_IN_PSRAM;  // USE PSRAM!
    config.grab_mode = CAMERA_GRAB_LATEST;
  } else {
    Serial.println("❌ NO PSRAM - Using basic config");
    config.frame_size = FRAMESIZE_QVGA;  // 320x240 low quality
    config.jpeg_quality = 12;
    config.fb_count = 1;
    config.fb_location = CAMERA_FB_IN_DRAM;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed 0x%x\n", err);
    delay(1000);
    ESP.restart();
  }
  
  Serial.println("Camera OK!");
}

bool sendPhotoTelegram() {
  const char* myDomain = "api.telegram.org";
  
  Serial.println(">>> Free memory before capture: " + String(ESP.getFreeHeap()));
  Serial.println(">>> Capturing photo...");
  
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println(">>> Camera capture FAILED!");
    return false;
  }

  Serial.println(">>> Photo captured: " + String(fb->len) + " bytes");
  Serial.println(">>> Free memory after capture: " + String(ESP.getFreeHeap()));
  
  // Check if we have enough memory to proceed
  if (ESP.getFreeHeap() < 20000) {
    Serial.println(">>> WARNING: Low memory, releasing photo buffer");
    esp_camera_fb_return(fb);
    return false;
  }

  Serial.println(">>> Connecting to Telegram...");
  
  if (!clientTCP.connect(myDomain, 443)) {
    Serial.println(">>> Connection to Telegram FAILED!");
    esp_camera_fb_return(fb);
    return false;
  }

  Serial.println(">>> Connected! Sending photo...");
  
  // Build header - avoid String concatenation to save memory
  clientTCP.print("POST /bot");
  clientTCP.print(BOT_TOKEN);
  clientTCP.println("/sendPhoto HTTP/1.1");
  clientTCP.print("Host: ");
  clientTCP.println(myDomain);
  
  // Calculate content length
  String head = "--X\r\nContent-Disposition: form-data; name=\"chat_id\"; \r\n\r\n";
  head += CHAT_ID;
  head += "\r\n--X\r\nContent-Disposition: form-data; name=\"photo\"; filename=\"esp32.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
  String tail = "\r\n--X--\r\n";
  
  uint32_t totalLen = fb->len + head.length() + tail.length();
  
  clientTCP.print("Content-Length: ");
  clientTCP.println(String(totalLen));
  clientTCP.println("Content-Type: multipart/form-data; boundary=X");
  clientTCP.println();
  clientTCP.print(head);

  // Send image data in chunks
  uint8_t *fbBuf = fb->buf;
  size_t fbLen = fb->len;
  
  for (size_t n = 0; n < fbLen; n = n + 1024) {
    if (n + 1024 < fbLen) {
      clientTCP.write(fbBuf, 1024);
      fbBuf += 1024;
    } else {
      size_t remainder = fbLen % 1024;
      clientTCP.write(fbBuf, remainder);
    }
    delay(1); // Small delay to prevent watchdog timeout
  }

  clientTCP.print(tail);
  
  // IMPORTANT: Free the frame buffer immediately
  esp_camera_fb_return(fb);
  fb = NULL;

  Serial.println(">>> Waiting for response...");
  
  // Wait for response with timeout
  unsigned long timeout = millis();
  while (clientTCP.connected() && !clientTCP.available()) {
    if (millis() - timeout > 10000) {
      Serial.println(">>> Timeout waiting for response");
      clientTCP.stop();
      return false;
    }
    delay(10);
  }

  // Read response (but don't store it to save memory)
  bool success = false;
  while (clientTCP.available()) {
    String line = clientTCP.readStringUntil('\n');
    if (line.indexOf("\"ok\":true") > 0) {
      success = true;
    }
  }
  
  clientTCP.stop();
  
  if (success) {
    Serial.println(">>> Photo sent successfully!");
  } else {
    Serial.println(">>> Photo send may have failed (check Telegram)");
  }
  
  Serial.println(">>> Free memory after send: " + String(ESP.getFreeHeap()));
  
  return success;
}

void handleNewMessages(int numNewMessages) {
  Serial.println("=== Handling " + String(numNewMessages) + " new message(s) ===");

  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    String text = bot.messages[i].text;
    String from_name = bot.messages[i].from_name;
    
    Serial.println("Message from: " + from_name);
    Serial.println("Chat ID: " + chat_id);
    Serial.println("Text: " + text);
    
    if (chat_id != CHAT_ID) {
      Serial.println(">>> Unauthorized user!");
      bot.sendMessage(chat_id, "Unauthorized", "");
      continue;
    }
    
    if (text == "/start") {
      Serial.println(">>> Sending welcome message");
      bot.sendMessage(chat_id, "Hi " + from_name + "! Send /photo for a picture.", "");
    }

    if (text == "/photo") {
      Serial.println(">>> PHOTO COMMAND RECEIVED!");
      bot.sendMessage(chat_id, "Taking photo...", "");
      
      // Add delay to let the "Taking photo..." message send
      delay(500);
      
      // Send photo
      bool success = sendPhotoTelegram();
      
      if (!success) {
        bot.sendMessage(chat_id, "Failed to send photo. Try again.", "");
      }
      
      // Clear any remaining memory
      delay(1000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n=================================");
  Serial.println("ESP32-CAM Telegram Bot Starting");
  Serial.println("=================================");
  
  // CHECK PSRAM STATUS
  if(psramFound()){
    Serial.println("✅ PSRAM: " + String(ESP.getPsramSize()) + " bytes");
    Serial.println("   Free PSRAM: " + String(ESP.getFreePsram()) + " bytes");
  } else {
    Serial.println("❌ PSRAM: NOT DETECTED!");
    Serial.println("   Check Arduino IDE: Tools → PSRAM → Enabled");
  }
  
  Serial.println("Heap RAM: " + String(ESP.getFreeHeap()) + " bytes");
  Serial.println("=================================\n");
  
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  clientTCP.setInsecure();
  
  Serial.println("Initializing camera...");
  configCamera();

  Serial.println("Connecting to WiFi: " + String(ssid));
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);  // Disable WiFi sleep for stability
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nWiFi connection failed! Restarting...");
    delay(3000);
    ESP.restart();
  }
  
  Serial.println("\n=================================");
  Serial.println("WiFi Connected!");
  Serial.println("IP Address: " + WiFi.localIP().toString());
  Serial.println("Free heap: " + String(ESP.getFreeHeap()) + " bytes");
  Serial.println("=================================");
  
  bot.sendMessage(CHAT_ID, "Camera online!", "");
  Serial.println("Ready! Waiting for commands...\n");
}

void loop() {
  if (millis() > lastTimeBotRan + botRequestDelay) {
    Serial.print(".");
    
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    
    if (numNewMessages) {
      Serial.println("\n>>> New message detected!");
      handleNewMessages(numNewMessages);
    }
    
    lastTimeBotRan = millis();
  }
  
  // Feed the watchdog
  yield();
}
