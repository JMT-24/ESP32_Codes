
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