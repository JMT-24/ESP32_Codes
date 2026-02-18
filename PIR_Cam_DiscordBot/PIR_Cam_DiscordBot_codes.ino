// ============================================================
// ESP32-CAM — Discord Photo Server (POWER OPTIMIZED)
// Station: Connects to main WiFi for Discord uploads
// AP: Creates network for PIR to connect directly
// ============================================================

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include "esp_camera.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// Main WiFi for internet access (Discord uploads)
const char* ssid     = "VIP";
const char* password = "30Mby20301heart";

// Access Point for PIR connection
const char* apSSID     = "ESP32-CAM-AP";
const char* apPassword = "camera123";

// Discord webhook
const char* discordHost = "discord.com";
const char* discordPath = "/api/webhooks/1473522025455685705/SrieDQEjLJs1sPaWOV-e96mJuVdVvT5Nu0UgOQ49hQCf5J39Vob9ZP4kFQI7L2CE6r4F";

WebServer server(80);
WiFiClientSecure clientTLS;

// ── Camera pin map (AI-Thinker) ───────────────────────────────
#define PWDN_GPIO_NUM  32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM   0
#define SIOD_GPIO_NUM  26
#define SIOC_GPIO_NUM  27
#define Y9_GPIO_NUM    35
#define Y8_GPIO_NUM    34
#define Y7_GPIO_NUM    39
#define Y6_GPIO_NUM    36
#define Y5_GPIO_NUM    21
#define Y4_GPIO_NUM    19
#define Y3_GPIO_NUM    18
#define Y2_GPIO_NUM     5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM  23
#define PCLK_GPIO_NUM  22
// ─────────────────────────────────────────────────────────────

void configCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {
    Serial.println("✅ PSRAM found!");
    // POWER OPTIMIZATION: Lower resolution + quality to reduce processing load
    config.frame_size   = FRAMESIZE_SVGA;   // 800x600 (was UXGA 1600x1200)
    config.jpeg_quality = 12;               // 12 = decent quality, less CPU (was 10)
    config.fb_count     = 1;                // Single buffer (was 2)
    config.fb_location  = CAMERA_FB_IN_PSRAM;
    config.grab_mode    = CAMERA_GRAB_LATEST;
  } else {
    Serial.println("❌ No PSRAM - using low quality");
    config.frame_size   = FRAMESIZE_QVGA;  // 320x240
    config.jpeg_quality = 12;
    config.fb_count     = 1;
    config.fb_location  = CAMERA_FB_IN_DRAM;
  }

  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("Camera init failed — restarting");
    delay(1000);
    ESP.restart();
  }
  Serial.println("Camera ready");
}

bool sendPhotoToDiscord(const String& caption) {
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("❌ Capture failed");
    return false;
  }

  Serial.printf("📷 Photo captured: %u bytes\n", fb->len);
  Serial.println("📤 Uploading to Discord...");

  if (!clientTLS.connect(discordHost, 443)) {
    Serial.println("❌ TLS connect failed");
    esp_camera_fb_return(fb);
    return false;
  }

  // Multipart boundary
  const String boundary = "GC0p4Jq0M2Yt08jU534c";

  // Part 1: JSON payload with caption
  String jsonPart =
    "--" + boundary + "\r\n"
    "Content-Disposition: form-data; name=\"payload_json\"\r\n"
    "Content-Type: application/json\r\n\r\n"
    "{\"content\":\"" + caption + "\"}\r\n";

  // Part 2: Image file
  String imgHead =
    "--" + boundary + "\r\n"
    "Content-Disposition: form-data; name=\"file\"; filename=\"motion.jpg\"\r\n"
    "Content-Type: image/jpeg\r\n\r\n";

  String tail = "\r\n--" + boundary + "--\r\n";

  uint32_t totalLen = jsonPart.length() + imgHead.length() + fb->len + tail.length();

  // HTTP headers
  clientTLS.print("POST ");
  clientTLS.print(discordPath);
  clientTLS.println(" HTTP/1.1");
  clientTLS.print("Host: ");
  clientTLS.println(discordHost);
  clientTLS.print("Content-Type: multipart/form-data; boundary=");
  clientTLS.println(boundary);
  clientTLS.print("Content-Length: ");
  clientTLS.println(totalLen);
  clientTLS.println("Connection: close");
  clientTLS.println();

  // Send body
  clientTLS.print(jsonPart);
  clientTLS.print(imgHead);

  // Send image in 1KB chunks
  uint8_t* p   = fb->buf;
  size_t   len = fb->len;
  for (size_t i = 0; i < len; i += 1024) {
    size_t chunk = (i + 1024 < len) ? 1024 : (len - i);
    clientTLS.write(p + i, chunk);
    delay(1);
  }
  clientTLS.print(tail);

  esp_camera_fb_return(fb);

  // Read response
  unsigned long timeout = millis();
  while (!clientTLS.available() && millis() - timeout < 10000) delay(10);

  bool success = false;
  while (clientTLS.available()) {
    String line = clientTLS.readStringUntil('\n');
    if (line.startsWith("HTTP/1.1 2")) success = true;
  }
  clientTLS.stop();

  if (success) {
    Serial.println("✅ Photo uploaded to Discord!");
  } else {
    Serial.println("❌ Discord upload failed");
  }

  return success;
}

void handleCapture() {
  String caption = server.hasArg("msg") ? server.arg("msg") : "📷 Photo captured";
  
  Serial.println("\n═══════════════════════════════════════");
  Serial.println("📸 CAPTURE REQUEST RECEIVED");
  Serial.println("═══════════════════════════════════════");
  
  server.send(200, "text/plain", "Capturing...");
  sendPhotoToDiscord(caption);
  
  Serial.println("═══════════════════════════════════════\n");
}

void handleRoot() {
  server.send(200, "text/plain", "ESP32-CAM Discord server OK");
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n╔════════════════════════════════════════════╗");
  Serial.println("║   ESP32-CAM Discord Photo Server          ║");
  Serial.println("║   POWER OPTIMIZED for Battery Operation   ║");
  Serial.println("╚════════════════════════════════════════════╝\n");
  
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  clientTLS.setInsecure();

  // Initialize camera BEFORE WiFi to reduce simultaneous power draw
  configCamera();
  
  // Small delay to let camera settle
  delay(500);

  // ═══════════════════════════════════════════════════════════
  // DUAL MODE: Connect to main WiFi + Create own Access Point
  // ═══════════════════════════════════════════════════════════
  
  // POWER OPTIMIZATION: Reduce WiFi transmit power
  WiFi.setTxPower(WIFI_POWER_15dBm);  // Reduce from default 19.5dBm
  
  // Step 1: Connect to main WiFi for Discord uploads
  Serial.println("📶 Connecting to main WiFi for internet access...");
  Serial.printf("   SSID: %s\n", ssid);
  
  WiFi.mode(WIFI_AP_STA);  // AP + Station mode
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ Connected to main WiFi");
    Serial.printf("   IP on main network: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\n⚠️  Could not connect to main WiFi");
    Serial.println("   Discord uploads will fail!");
  }

  // Small delay before starting AP
  delay(500);

  // Step 2: Create Access Point for PIR to connect to
  Serial.println("\n📡 Creating Access Point for PIR...");
  
  // POWER OPTIMIZATION: Limit AP to 1 client (the PIR)
  WiFi.softAP(apSSID, apPassword, 1, 0, 1);  // channel 1, hidden=0, max_clients=1
  
  IPAddress apIP = WiFi.softAPIP();
  Serial.println("✅ Access Point created!");
  Serial.printf("   SSID: %s\n", apSSID);
  Serial.printf("   Password: %s\n", apPassword);
  Serial.printf("   AP IP: %s\n", apIP.toString().c_str());
  
  Serial.println("\n⚠️  IMPORTANT: Configure PIR board:");
  Serial.printf("   WiFi SSID: %s\n", apSSID);
  Serial.printf("   WiFi Password: %s\n", apPassword);
  Serial.printf("   CAM IP: %s\n", apIP.toString().c_str());

  // Step 3: Start web server
  server.on("/", handleRoot);
  server.on("/capture", handleCapture);
  server.begin();
  
  Serial.println("\n🌐 HTTP server started");
  Serial.printf("📊 Free heap: %d bytes\n", ESP.getFreeHeap());
  Serial.println("════════════════════════════════════════\n");
  Serial.println("Ready! Waiting for PIR requests...\n");
}

void loop() {
  server.handleClient();
  yield();
}