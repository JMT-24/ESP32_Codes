// // ============================================================
// // PIR ESP32 — Motion Detector → triggers ESP32-CAM via HTTP
// // DEBUG VERSION with detailed logging
// // ============================================================

// #include <WiFi.h>
// #include <HTTPClient.h>
// #include <time.h>

// // const char* ssid     = "PLDT 2.4GHZ";
// // const char* password = "lionking64";

// const char* ssid = "VIP";
// const char* password = "30Mby20301heart";


// // ── Set this to the IP you found in Step 2 ───────────────────
// // const char* camIP = "192.168.8.76";  // Your CAM IP
// // ─────────────────────────────────────────────────────────────

// const char* camIP = "esp32cam.local";

// // Also keep your Discord webhook for text-only status messages
// const char* discordWebhook =
//   "https://discord.com/api/webhooks/1470551376130998314/zYiiPBGCYF4yAKfbZBq-FgQitlQ6iwBmtpfCnDmDVGj8GJDF45Z2yiDtl9lYkCKxskV4";

// const int   pirPin          = 15;
// const unsigned long COOLDOWN_MS = 10000;  // 10 s between triggers

// int  lastMotionState   = LOW;
// unsigned long lastTriggerTime = 0;

// // NTP
// const char* ntpServer     = "pool.ntp.org";
// const long  gmtOffset_sec = 8 * 3600;

// String getDateTime() {
//   struct tm t;
//   if (!getLocalTime(&t)) return "Time unavailable";
//   char buf[30];
//   strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &t);
//   return String(buf);
// }

// // Send a text-only message to Discord (for boot/error notices)
// void sendDiscordText(const String& msg) {
//   if (WiFi.status() != WL_CONNECTED) {
//     Serial.println("⚠️  WiFi not connected, can't send Discord message");
//     return;
//   }
  
//   Serial.println("📤 Sending text to Discord...");
//   HTTPClient http;
//   http.begin(discordWebhook);
//   http.addHeader("Content-Type", "application/json");
  
//   String safe = msg;
//   safe.replace("\"", "'");
//   safe.replace("\n", "\\n");
  
//   int code = http.POST("{\"content\":\"" + safe + "\"}");
//   Serial.printf("   Discord text response: %d\n", code);
//   http.end();
// }

// // Ask the CAM to capture and upload a photo to Discord
// void triggerCamCapture(const String& caption) {
//   Serial.println("\n═════════════════════════════════════════════════");
//   Serial.println("🎯 TRIGGERING CAMERA CAPTURE");
//   Serial.println("═════════════════════════════════════════════════");
  
//   if (WiFi.status() != WL_CONNECTED) {
//     Serial.println("❌ WiFi lost — skipping capture");
//     return;
//   }

//   Serial.printf("📡 PIR IP:    %s\n", WiFi.localIP().toString().c_str());
//   Serial.printf("📷 CAM IP:    %s\n", camIP);
//   Serial.printf("📊 Free heap: %d bytes\n", ESP.getFreeHeap());
  
//   // ─────────────────────────────────────────────────────────
//   // STEP 1: Health check — can we reach the CAM at all?
//   // ─────────────────────────────────────────────────────────
//   Serial.println("\n[STEP 1] Testing CAM reachability (GET /)...");
//   HTTPClient http;
//   http.begin(String("http://") + camIP + "/");
//   http.setTimeout(5000);
  
//   int healthCheck = http.GET();
//   Serial.printf("   Response code: %d\n", healthCheck);
  
//   if (healthCheck == 200) {
//     Serial.println("   ✅ CAM web server is responding!");
//     String body = http.getString();
//     Serial.printf("   Response body: \"%s\"\n", body.c_str());
//   } else if (healthCheck > 0) {
//     Serial.printf("   ⚠️  Unexpected HTTP code %d\n", healthCheck);
//   } else {
//     Serial.println("   ❌ CANNOT REACH CAM!");
//     Serial.println("\n🔍 TROUBLESHOOTING:");
//     Serial.println("   1. Is the CAM powered on?");
//     Serial.println("   2. Check CAM Serial Monitor — is web server running?");
//     Serial.println("   3. Router settings — is AP Isolation enabled?");
//     Serial.println("   4. Try accessing from browser: http://192.168.8.76/");
//     http.end();
    
//     // Fallback: send text-only alert to Discord
//     sendDiscordText("⚠️ Motion detected but camera unreachable!\n🕒 " + getDateTime());
//     return;
//   }
//   http.end();

//   // ─────────────────────────────────────────────────────────
//   // STEP 2: Send capture command
//   // ─────────────────────────────────────────────────────────
//   Serial.println("\n[STEP 2] Sending /capture command...");
  
//   String url = "http://";
//   url += camIP;
//   url += "/capture?msg=";

//   // URL-encode the caption (spaces → %20, basic encoding)
//   String encoded = "";
//   for (char c : caption) {
//     if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
//       encoded += c;
//     } else if (c == ' ') {
//       encoded += "%20";
//     } else {
//       char hex[4];
//       snprintf(hex, sizeof(hex), "%%%02X", (unsigned char)c);
//       encoded += hex;
//     }
//   }
//   url += encoded;

//   Serial.println("   URL: " + url);
//   Serial.println("   Waiting for CAM to shoot + upload (max 15s)...");

//   http.begin(url);
//   http.setTimeout(15000);  // CAM needs time to shoot + upload to Discord
  
//   unsigned long startTime = millis();
//   int code = http.GET();
//   unsigned long elapsed = millis() - startTime;

//   Serial.printf("   Response code: %d (took %lu ms)\n", code, elapsed);
  
//   if (code == 200) {
//     Serial.println("   ✅ CAM accepted the command!");
//     String response = http.getString();
//     Serial.printf("   CAM says: \"%s\"\n", response.c_str());
//   } else if (code > 0) {
//     Serial.printf("   ⚠️  HTTP error %d\n", code);
//   } else {
//     Serial.println("   ❌ Request timed out or failed");
//     Serial.println("      CAM might be busy processing or crashed");
    
//     // Fallback alert
//     sendDiscordText("⚠️ Motion detected but camera failed to respond!\n🕒 " + getDateTime());
//   }
  
//   http.end();
  
//   Serial.println("═════════════════════════════════════════════════\n");
// }

// void setup() {
//   Serial.begin(115200);
//   delay(1000);
  
//   Serial.println("\n\n╔════════════════════════════════════════════╗");
//   Serial.println("║  PIR Motion Detector → ESP32-CAM Trigger   ║");
//   Serial.println("║  DEBUG VERSION with detailed logging       ║");
//   Serial.println("╚════════════════════════════════════════════╝\n");
  
//   pinMode(pirPin, INPUT);

//   // ─────────────────────────────────────────────────────────
//   // WiFi Connection
//   // ─────────────────────────────────────────────────────────
//   Serial.printf("📶 Connecting to WiFi: %s\n", ssid);
//   WiFi.begin(ssid, password);
  
//   int attempts = 0;
//   while (WiFi.status() != WL_CONNECTED && attempts < 40) {
//     delay(500);
//     Serial.print(".");
//     attempts++;
//   }
  
//   if (WiFi.status() != WL_CONNECTED) {
//     Serial.println("\n❌ WiFi connection FAILED after 20s!");
//     Serial.println("   Check SSID and password, then restart");
//     while(1) delay(1000); // Halt
//   }
  
//   Serial.println("\n✅ WiFi Connected!");
//   Serial.printf("   IP Address: %s\n", WiFi.localIP().toString().c_str());
//   Serial.printf("   Subnet:     %s\n", WiFi.subnetMask().toString().c_str());
//   Serial.printf("   Gateway:    %s\n", WiFi.gatewayIP().toString().c_str());
//   Serial.printf("   Signal:     %d dBm\n", WiFi.RSSI());

//   // ─────────────────────────────────────────────────────────
//   // Time Sync
//   // ─────────────────────────────────────────────────────────
//   Serial.println("\n⏰ Syncing time with NTP...");
//   configTime(gmtOffset_sec, 0, ntpServer);
//   delay(2000);
//   Serial.printf("   Current time: %s\n", getDateTime().c_str());

//   // ─────────────────────────────────────────────────────────
//   // CAM Connectivity Test
//   // ─────────────────────────────────────────────────────────
//   Serial.println("\n🧪 Testing CAM connectivity at startup...");
//   HTTPClient http;
//   http.begin(String("http://") + camIP + "/");
//   http.setTimeout(5000);
  
//   int testCode = http.GET();
//   Serial.printf("   CAM health check: %d\n", testCode);
  
//   if (testCode == 200) {
//     String body = http.getString();
//     Serial.println("   ✅ CAM is ONLINE and responding!");
//     Serial.printf("   Response: \"%s\"\n", body.c_str());
//   } else if (testCode > 0) {
//     Serial.printf("   ⚠️  CAM returned HTTP %d (expected 200)\n", testCode);
//   } else {
//     Serial.println("   ❌ CANNOT REACH CAM!");
//     Serial.println("\n⚠️  WARNING: Camera is not reachable.");
//     Serial.println("   Make sure the ESP32-CAM is:");
//     Serial.println("   1. Powered on and running");
//     Serial.println("   2. Connected to the same WiFi network");
//     Serial.println("   3. Running the Discord photo server code");
//     Serial.println("   4. Check CAM Serial Monitor for its IP address");
//     Serial.println("\n   Continuing anyway — motion events will fail...\n");
//   }
//   http.end();

//   // ─────────────────────────────────────────────────────────
//   // Send boot notification to Discord
//   // ─────────────────────────────────────────────────────────
//   sendDiscordText("🟢 PIR sensor ONLINE\n🕒 " + getDateTime());

//   // ─────────────────────────────────────────────────────────
//   // PIR Warmup
//   // ─────────────────────────────────────────────────────────
//   Serial.println("\n🔥 Warming up PIR sensor (20s)...");
//   for (int i = 20; i > 0; i--) {
//     Serial.printf("   %d seconds remaining...\r", i);
//     delay(1000);
//   }
//   Serial.println("\n✅ PIR sensor ready!");
//   Serial.println("\n👀 Monitoring for motion...\n");
// }

// void loop() {
//   int motionState = digitalRead(pirPin);
//   unsigned long now = millis();

//   if (motionState == HIGH && lastMotionState == LOW) {
//     Serial.println("\n🚨 ─────────────────────────────────────────");
//     Serial.println("   MOTION DETECTED!");
//     Serial.println("   ─────────────────────────────────────────");
    
//     if (now - lastTriggerTime > COOLDOWN_MS) {
//       lastTriggerTime = now;
      
//       String caption =
//         "🚨 Motion Detected!\\n"
//         "📍 Sensor: PIR GPIO 15\\n"
//         "🕒 Time: " + getDateTime();

//       triggerCamCapture(caption);
      
//     } else {
//       unsigned long remaining = COOLDOWN_MS - (now - lastTriggerTime);
//       Serial.printf("   ⏳ Cooldown active (%lu ms remaining)\n", remaining);
//       Serial.println("   Ignoring this trigger to prevent spam\n");
//     }
//   }

//   lastMotionState = motionState;
//   delay(100);
// }


// ============================================================
// PIR ESP32 — Motion Detector → triggers ESP32-CAM via HTTP
// Connects directly to CAM's Access Point
// ============================================================

#include <WiFi.h>
#include <HTTPClient.h>
#include <time.h>

// Connect to CAM's Access Point (not main WiFi)
const char* ssid     = "ESP32-CAM-AP";
const char* password = "camera123";

// CAM IP on its own AP network (default gateway)
const char* camIP = "192.168.4.1";

const int pirPin = 15;
const unsigned long COOLDOWN_MS = 10000;  // 10s between triggers

int lastMotionState = LOW;
unsigned long lastTriggerTime = 0;

// Note: NTP won't work since we're not on main internet
// We'll use millis() for timing instead
unsigned long bootTime = 0;

String getUptime() {
  unsigned long seconds = (millis() - bootTime) / 1000;
  unsigned long minutes = seconds / 60;
  unsigned long hours = minutes / 60;
  
  char buf[50];
  snprintf(buf, sizeof(buf), "%luh %lum %lus", 
           hours, minutes % 60, seconds % 60);
  return String(buf);
}

void triggerCamCapture(const String& caption) {
  Serial.println("\n═════════════════════════════════════════════════");
  Serial.println("🎯 TRIGGERING CAMERA CAPTURE");
  Serial.println("═════════════════════════════════════════════════");
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("❌ WiFi lost — skipping capture");
    return;
  }

  Serial.printf("📡 PIR IP:    %s\n", WiFi.localIP().toString().c_str());
  Serial.printf("📷 CAM IP:    %s\n", camIP);
  Serial.printf("📊 Free heap: %d bytes\n", ESP.getFreeHeap());
  
  // Test CAM reachability
  Serial.println("\n[STEP 1] Testing CAM reachability (GET /)...");
  HTTPClient http;
  http.begin(String("http://") + camIP + "/");
  http.setTimeout(5000);
  
  int healthCheck = http.GET();
  Serial.printf("   Response code: %d\n", healthCheck);
  
  if (healthCheck == 200) {
    Serial.println("   ✅ CAM web server is responding!");
  } else {
    Serial.println("   ❌ CANNOT REACH CAM!");
    http.end();
    return;
  }
  http.end();

  // Send capture command
  Serial.println("\n[STEP 2] Sending /capture command...");
  
  String url = "http://";
  url += camIP;
  url += "/capture?msg=";

  // URL-encode the caption
  String encoded = "";
  for (char c : caption) {
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      encoded += c;
    } else if (c == ' ') {
      encoded += "%20";
    } else {
      char hex[4];
      snprintf(hex, sizeof(hex), "%%%02X", (unsigned char)c);
      encoded += hex;
    }
  }
  url += encoded;

  Serial.println("   URL: " + url);
  Serial.println("   Waiting for CAM to shoot + upload (max 15s)...");

  http.begin(url);
  http.setTimeout(15000);
  
  unsigned long startTime = millis();
  int code = http.GET();
  unsigned long elapsed = millis() - startTime;

  Serial.printf("   Response code: %d (took %lu ms)\n", code, elapsed);
  
  if (code == 200) {
    Serial.println("   ✅ CAM accepted the command!");
  } else if (code > 0) {
    Serial.printf("   ⚠️  HTTP error %d\n", code);
  } else {
    Serial.println("   ❌ Request failed");
  }
  
  http.end();
  Serial.println("═════════════════════════════════════════════════\n");
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n╔════════════════════════════════════════════╗");
  Serial.println("║  PIR Motion Detector → ESP32-CAM Trigger   ║");
  Serial.println("║  Direct connection to CAM Access Point     ║");
  Serial.println("╚════════════════════════════════════════════╝\n");
  
  pinMode(pirPin, INPUT);
  bootTime = millis();

  // Connect to CAM's Access Point
  Serial.printf("📶 Connecting to CAM Access Point: %s\n", ssid);
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\n❌ WiFi connection FAILED!");
    Serial.println("   Make sure ESP32-CAM is powered on and AP is running");
    while(1) delay(1000); // Halt
  }
  
  Serial.println("\n✅ Connected to CAM!");
  Serial.printf("   IP Address: %s\n", WiFi.localIP().toString().c_str());
  Serial.printf("   Gateway:    %s\n", WiFi.gatewayIP().toString().c_str());

  // Test CAM connectivity
  Serial.println("\n🧪 Testing CAM connectivity...");
  HTTPClient http;
  http.begin(String("http://") + camIP + "/");
  http.setTimeout(5000);
  
  int testCode = http.GET();
  Serial.printf("   CAM health check: %d\n", testCode);
  
  if (testCode == 200) {
    String body = http.getString();
    Serial.println("   ✅ CAM is ONLINE!");
    Serial.printf("   Response: \"%s\"\n", body.c_str());
  } else {
    Serial.println("   ❌ CANNOT REACH CAM!");
    Serial.println("   Check CAM Serial Monitor");
  }
  http.end();

  // PIR warmup
  Serial.println("\n🔥 Warming up PIR sensor (20s)...");
  for (int i = 20; i > 0; i--) {
    Serial.printf("   %d seconds remaining...\r", i);
    delay(1000);
  }
  Serial.println("\n✅ PIR sensor ready!");
  Serial.println("\n👀 Monitoring for motion...\n");
}

void loop() {
  int motionState = digitalRead(pirPin);
  unsigned long now = millis();

  if (motionState == HIGH && lastMotionState == LOW) {
    Serial.println("\n🚨 ─────────────────────────────────────────");
    Serial.println("   MOTION DETECTED!");
    Serial.println("   ─────────────────────────────────────────");
    
    if (now - lastTriggerTime > COOLDOWN_MS) {
      lastTriggerTime = now;
      
      String caption =
        "🚨 Motion Detected!\\n"
        "📍 Sensor: PIR GPIO 15\\n"
        "⏱️ Uptime: " + getUptime();

      triggerCamCapture(caption);
      
    } else {
      unsigned long remaining = COOLDOWN_MS - (now - lastTriggerTime);
      Serial.printf("   ⏳ Cooldown active (%lu ms remaining)\n", remaining);
      Serial.println("   Ignoring this trigger\n");
    }
  }

  lastMotionState = motionState;
  delay(100);
}