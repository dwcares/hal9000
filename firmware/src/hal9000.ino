/*
 * HAL 9000 Voice Assistant
 *
 * Push-to-talk voice assistant using microstream and OpenAI.
 * Press and hold the button to talk, release to send.
 * LED indicates connection status.
 *
 * Hardware:
 *   - Microphone (with MAX4466 amp) on MIC_PIN
 *   - Speaker (with PAM8403 amp) on SPEAKER_PIN
 *   - Push button on BUTTON_PIN (active low)
 *   - Status LED on LED_PIN
 */

#include "Microstream.h"

// --- Hardware Pins ---
#define LED_PIN      D0       // Status LED (or use D7 for onboard)
#define BUTTON_PIN   D4       // Push-to-talk button
#define MIC_PIN      A6       // Microphone input (DAC pin, works as ADC)
#define SPEAKER_PIN  A3       // Speaker output (DAC: A3 on Photon, A6 on Argon)

// --- Server Configuration ---
#define SERVER_PORT  5000
#define SERVER_PATH  "/"
#define EEPROM_ADDR  0        // EEPROM address for cached IP

Microstream mic;

char serverHost[64] = "";     // Server IP from cloud discovery
bool serverConfigured = false;
bool buttonDown = false;
unsigned long lastStatusTime = 0;
unsigned long connectedSince = 0;

// --- EEPROM helpers for IP caching ---
void saveServerIP(const char* ip) {
  uint8_t len = strlen(ip);
  EEPROM.write(EEPROM_ADDR, len);
  for (uint8_t i = 0; i < len; i++) {
    EEPROM.write(EEPROM_ADDR + 1 + i, ip[i]);
  }
  Serial.printlnf("Cached server IP: %s", ip);
}

bool loadServerIP(char* ip, size_t maxLen) {
  uint8_t len = EEPROM.read(EEPROM_ADDR);
  if (len == 0 || len == 255 || len >= maxLen) return false;
  for (uint8_t i = 0; i < len; i++) {
    ip[i] = EEPROM.read(EEPROM_ADDR + 1 + i);
  }
  ip[len] = '\0';
  return true;
}

// --- Cloud event handler ---
void onServerIP(const char* event, const char* data) {
  Serial.printlnf("Received server IP: %s", data);

  // Only update if IP changed
  if (strcmp(serverHost, data) != 0) {
    strncpy(serverHost, data, sizeof(serverHost) - 1);
    saveServerIP(serverHost);

    if (!serverConfigured) {
      serverConfigured = true;
      startMicrostream();
    }
  }
}

// --- Callbacks ---
void onMicConnected() {
  connectedSince = millis();
  Serial.println("Connected to server");
}

void onMicDisconnected() {
  connectedSince = 0;
  Serial.println("Disconnected from server");
}

void onPlaybackStart() {
  Serial.println("Playing response...");
}

void onPlaybackEnd() {
  Serial.println("Playback done");
}

void startMicrostream() {
  MicrostreamConfig cfg;
  cfg.sampleRate = 16000;
  cfg.bitDepth = 16;
  cfg.micPin = MIC_PIN;
  cfg.speakerPin = SPEAKER_PIN;
  cfg.captureBufferSize = 8192;
  cfg.playbackBufferSize = 8192;

  Serial.printlnf("Connecting to %s:%d%s", serverHost, SERVER_PORT, SERVER_PATH);

  mic.begin(serverHost, SERVER_PORT, SERVER_PATH, cfg);
  mic.onConnected(onMicConnected);
  mic.onDisconnected(onMicDisconnected);
  mic.onPlaybackStart(onPlaybackStart);
  mic.onPlaybackEnd(onPlaybackEnd);
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("--- HAL 9000 ---");

  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Subscribe to server IP events
  Particle.subscribe("microstream/server-ip", onServerIP, MY_DEVICES);

  // Try to load cached IP
  if (loadServerIP(serverHost, sizeof(serverHost))) {
    Serial.printlnf("Using cached server IP: %s", serverHost);
    serverConfigured = true;
    startMicrostream();
  } else {
    Serial.println("No cached IP, waiting for server...");
  }

  // Announce we're online (triggers server to publish IP)
  Particle.publish("microstream/device-online", "", PRIVATE);
}

void loop() {
  if (serverConfigured) {
    mic.update();
  }

  // --- Push-to-talk button ---
  bool pressed = (digitalRead(BUTTON_PIN) == LOW);

  if (pressed && !buttonDown && serverConfigured && mic.isConnected()) {
    buttonDown = true;
    mic.startRecording();
    Serial.println("Recording...");
  }

  if (!pressed && buttonDown) {
    buttonDown = false;
    if (mic.isRecording()) {
      mic.stopRecording();
      Serial.println("Sent audio, waiting for response...");
    }
  }

  // --- LED feedback: on = connected, off = disconnected ---
  digitalWrite(LED_PIN, serverConfigured && mic.isConnected() ? HIGH : LOW);

  // --- Status ---
  if (millis() - lastStatusTime > 5000) {
    lastStatusTime = millis();
    if (!serverConfigured) {
      Serial.println("Status: WAITING FOR SERVER IP");
    } else if (mic.isConnected()) {
      Serial.printlnf("Status: CONNECTED | Uptime: %lus", (millis() - connectedSince) / 1000);
    } else {
      Serial.println("Status: DISCONNECTED");
    }
  }
}
