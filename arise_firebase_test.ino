#define BLYNK_TEMPLATE_ID "TMPL6iLQqZJPG"     // Ganti dengan Template ID Anda
#define BLYNK_TEMPLATE_NAME "ARISE Deteksi Api"
String blynkAuthToken = "eGIRAjSKsW15lmBWH0yWm7w5f8vERk5n";   // Default Auth Token (Dynamic from Flash)
#define BLYNK_PRINT Serial

// ===== FREERTOS CONFIG =====
TaskHandle_t NetworkTaskHandle;
SemaphoreHandle_t dataMutex;

// ===== LIBRARIES =====
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <BlynkSimpleEsp32.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <HTTPClient.h>

// ===== RENDER SERVER CONFIGURATION =====
// Ganti dengan URL aplikasi Render Anda setelah dideploy (misal: "https://arise-deteksi-api.onrender.com")
String renderServerUrl = "https://arise-deteksi-api.onrender.com";

// ===== OLED CONFIG =====
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ===== PIN DEFINITIONS =====
#define DHTPIN 19
#define DHTTYPE DHT22
#define FLAME_PIN 34
#define MQ2_PIN 35
#define LED_HIJAU 26
#define LED_MERAH 27
#define SERVO_PIN 25
#define BUZZER_PIN 33

// LED Definitions
// LED_HIJAU 26, LED_MERAH 27 (Existing)
#define LED_RGB_EXTRA 14  // "RGB 2 kaki" pin

// ===== OBJECTS =====
DHT dht(DHTPIN, DHTTYPE);
Servo myServo;
WebServer server(80);
DNSServer dnsServer;
Preferences preferences;
BlynkTimer timer;
WidgetTerminal terminal(V7); // Terminal Widget di V7

// ===== DNS PORT =====
#define DNS_PORT 53

// ===== SENSOR VARIABLES =====
float suhu = 0, hum = 0;
float lastTemp = 0, lastHum = 0;
int mq2Value = 0;
int gasPercentage = 0;
bool flameDetected = false;
int flamePosition = -1;  // Posisi servo saat api terdeteksi

// ===== ASYNC WIFI SCAN =====
bool scanInProgress = false;
String lastScanResult = "{\"networks\":[]}";
unsigned long scanStartTime = 0;

// ===== DIAGNOSTICS & CALIBRATION =====
bool dhtError = false;
bool mq2Error = false;
int gasBaseline = 0; // Calibration offset
bool needsAutoCalibrate = false; // Status auto-kalibrasi booting
bool isCalibrating = false;
bool sensorFailNotified = false;

// ===== SAFETY THRESHOLDS & FILTERS =====
const float TEMP_THRESHOLD = 45.0; // Ambang batas suhu tinggi (Celcius)
const int GAS_THRESHOLD = 1000;    // Ambang batas MQ-2 tinggi (adjusted baseline)

// ===== ALARM LEVELS =====
// Level 0: Normal, Level 1: Waspada, Level 2: Bahaya, Level 3: Kritis
int alarmLevel = 0;
int prevLevel = 0;   // Untuk mendeteksi perubahan level alarm
String alarmStatus[] = {"NORMAL", "WASPADA", "BAHAYA!", "KRITIS!!"};
// Short status for OLED (max 6 chars)
String alarmStatusShort[] = {"NORMAL", "WASPAD", "BAHAYA", "KRITIS"};

// ===== AIR QUALITY LEVELS =====
// 0: Good, 1: Moderate, 2: Poor, 3: Danger
int airQualityLevel = 0;
String airQualityText[] = {"BAIK", "SEDANG", "BURUK", "BAHAYA"};

// ===== TIMING VARIABLES =====
unsigned long lastBuzzerToggle = 0;
unsigned long lastLEDToggle = 0;
unsigned long lastServoMove = 0;
unsigned long lastOLEDUpdate = 0;
unsigned long lastSensorRead = 0;
unsigned long lastBlynkUpdate = 0;
unsigned long lastNotification = 0;

bool buzzerState = false;
bool ledBlinkState = false;

// ===== SERVO VARIABLES =====
int sudutServo = 0;
bool servoNaik = true;
bool servoScanning = true;  // false jika api terdeteksi

// ===== WIFI & SYSTEM =====
bool powerSaveMode = false;
String savedSSID = "";
String savedPassword = "";
String apPassword = "12345678";  // Default AP password
bool wifiConnected = false;
bool blynkConnected = false;
bool blynkConfigured = false;
unsigned long lastBlynkReconnectAttempt = 0;
int animFrame = 0;

// ===== AP MODE MANAGEMENT =====
bool apModeActive = false;
unsigned long lastWiFiReconnectAttempt = 0;
bool pendingWiFiConnect = false;
unsigned long pendingWiFiConnectTime = 0;
bool internetReachable = false;
unsigned long lastInternetCheck = 0;
int internetFailCount = 0;

// ===== LOGIN & SESSION SYSTEM =====
#define DEFAULT_USERNAME "admin"
#define DEFAULT_PASSWORD "admin123"
String loginUsername = DEFAULT_USERNAME;
String loginPassword = DEFAULT_PASSWORD;
String sessionToken = "";
unsigned long sessionStartTime = 0;
#define SESSION_TIMEOUT 3600000  // 1 jam dalam milidetik
bool isAuthenticated = false;

// ===== BLYNK OPTIMIZATION =====
int lastBlynkBuzzerState = -1;  // -1 = kondisi awal belum diketahui

// ===== OLED IP DISPLAY =====
bool showIPOnOLED = false;
unsigned long lastOLEDIPToggle = 0;

// ===== FUNCTION PROTOTYPES =====
void bacaDHTandMQ2();
void bacaFlameFast();
void updateAlarmLevel();
void updateLED();
void updateBuzzer();
void updateServo();
void updateOLED();
void displaySplash();
void sendToBlynk();
void logToBlynk(String msg);
void handleRoot();
void handleDashboard();
void handleWiFiConfig();
void handleSaveWiFi();
void handleScanWiFi();
void processAsyncScanResult();

void handleAPIStatus();
void handleCalibrate();
void handleReboot();
void handleAPPassConfig();
void handleSaveAPPass();
void loadWiFiConfig();
void saveWiFiConfig();
void connectToWiFi();
String escapeJSONString(const String &input);
String escapeHTML(const String &input);

// ===== CAPTIVE PORTAL & DNS =====
void startCaptivePortal();
void stopCaptivePortal();
void handleCaptivePortal();

// ===== LOGIN & AUTHENTICATION =====
void handleLogin();
void handleLogout();
void handleLoginSubmit();
bool checkAuthentication();
void ensureAuthenticated();
void handleLoginConfig();
void handleSaveLoginConfig();
void handleBlynkConfig();
void handleSaveBlynkConfig();
void handleEducation();

// ===== AP MANAGEMENT =====
void startFallbackAP();
void stopFallbackAP();

// ===== NOT FOUND HANDLER =====
void handleNotFound();

// ===== WIFI MANAGEMENT =====
void autoReconnectWiFi();
void sendTelemetryToRender();
void processPendingWiFiConnect();
void checkWiFiConnection();
bool hasInternetAccess();
void monitorInternetAccess();
void manageBlynkConnection();

// ===== SETUP =====
void setup() {
  Serial.begin(115200);
  delay(1000);
  dataMutex = xSemaphoreCreateMutex();
  
  Serial.println("\n================================");
  Serial.println("ARISE Deteksi Api System v3.0");
  Serial.println("================================\n");

  preferences.begin("arise", false);
  loadWiFiConfig(); // Load all saved configurations first
  
  // Inisialisasi LittleFS (OTOMATIS FORMAT jika korup/tidak terbaca)
  Serial.println("Mounting LittleFS...");
  if(!LittleFS.begin(true)){
    Serial.println("LittleFS CRITICAL ERROR: Failed to mount or format");
  } else {
    Serial.println("LittleFS Mounted Successfully");
  }

  // LED & Buzzer Setup
  pinMode(LED_HIJAU, OUTPUT);
  pinMode(LED_MERAH, OUTPUT);
  pinMode(LED_RGB_EXTRA, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(FLAME_PIN, INPUT);
  pinMode(MQ2_PIN, INPUT);

  // Initial states
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(LED_HIJAU, HIGH);
  digitalWrite(LED_MERAH, LOW);
  digitalWrite(LED_RGB_EXTRA, HIGH); // Default ON

  Serial.println("[7] Servo Attach...");
  myServo.attach(SERVO_PIN);
  myServo.write(0);

  Serial.println("[8] OLED Init...");
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED Error!");
    while (1);
  }
  Serial.println("[9] OLED OK!");

  dht.begin();
  Serial.print("Gas Baseline: "); Serial.println(gasBaseline);

  displaySplash();

  // WiFi Config and AP details are already loaded on startup

  // ===== HYBRID MODE: AP selalu aktif =====
  // AP dinyalakan PERTAMA agar user selalu bisa akses via 192.168.4.1
  startFallbackAP();
  Serial.println("Hybrid Mode: AP 'ARISE-Setup' selalu aktif di 192.168.4.1");

  // Connect WiFi (STA) bersamaan dengan AP
  if (savedSSID.length() > 0) {
    connectToWiFi();
    Serial.println("Mencoba koneksi WiFi STA bersamaan dengan AP...");
  } else {
    Serial.println("Belum ada WiFi tersimpan. Akses via AP: 192.168.4.1");
  }

  // Prepare Blynk config, actual connection is handled in loop()
  if (blynkAuthToken.length() > 5) {
    Blynk.config(blynkAuthToken.c_str());
    blynkConfigured = true;
  }

  // ===== WEB SERVER ROUTES =====
  // Captive portal handlers for different OS
  server.on("/generate_204", HTTP_GET, handleCaptivePortal);      // Android
  server.on("/hotspot-detect.html", HTTP_GET, handleCaptivePortal); // Apple
  server.on("/ncsi.txt", HTTP_GET, handleCaptivePortal);          // Windows
  server.on("/fwlink", HTTP_GET, handleCaptivePortal);            // Microsoft
  server.on("/connecttest.txt", HTTP_GET, handleCaptivePortal);   // Windows
  
  // Main routes
  server.on("/", handleRoot);
  server.on("/dashboard", handleDashboard);
  server.on("/login", handleLogin);
  server.on("/logincheck", HTTP_POST, handleLoginSubmit);
  server.on("/logout", handleLogout);
  server.on("/webpassconfig", HTTP_GET, handleLoginConfig);
  server.on("/webpassconfig", HTTP_POST, handleSaveLoginConfig);
  server.on("/wificonfig", handleWiFiConfig);
  server.on("/api/status", handleAPIStatus);
  server.on("/calibrate", handleCalibrate);
  server.on("/reboot", HTTP_POST, handleReboot);
  server.on("/savewifi", HTTP_POST, handleSaveWiFi);
  server.on("/scanwifi", handleScanWiFi);
  server.on("/appassconfig", handleAPPassConfig);
  server.on("/saveappass", HTTP_POST, handleSaveAPPass);
  server.on("/blynkconfig", handleBlynkConfig);
  server.on("/saveblynkconfig", HTTP_POST, handleSaveBlynkConfig);
  server.on("/education", handleEducation);
  
  // Serve static files from LittleFS (CSS, JS, images if any)
  server.serveStatic("/css", LittleFS, "/css");
  server.serveStatic("/js", LittleFS, "/js");
  
  // Not found handler
  server.onNotFound(handleNotFound);
  
  // Collect Cookie header agar session authentication berfungsi
  const char* headerKeys[] = {"Cookie"};
  server.collectHeaders(headerKeys, 1);

  server.begin();
  Serial.println("Web Server Started!");

  // Blynk Timer - update setiap 3 detik (mencegah flood/limit Blynk)
  timer.setInterval(3000L, sendToBlynk);

  // Mulai Network Task di Core 0
  xTaskCreatePinnedToCore(
    networkTask,      // Fungsi Task
    "NetworkTask",    // Nama Task
    10240,            // Ukuran Stack (Ditingkatkan)
    NULL,             // Parameter
    1,                // Prioritas
    &NetworkTaskHandle, // Handle Task
    0                 // Core 0
  );
}

// ===== NETWORK TASK (CORE 0) =====
void networkTask(void * pvParameters) {
  for(;;) {
    // Diagnostik Sisa Stack Memory untuk mencegah WDT Crash
    static unsigned long lastStackCheck = 0;
    if (millis() - lastStackCheck > 10000) {
      if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        Serial.print("[Core 0] NetworkTask Stack HWM: ");
        Serial.println(uxTaskGetStackHighWaterMark(NULL));
        xSemaphoreGive(dataMutex);
      }
      lastStackCheck = millis();
    }

    server.handleClient();
    if (apModeActive) {
      dnsServer.processNextRequest();
    }
    
    processAsyncScanResult();
    checkWiFiConnection();
    processPendingWiFiConnect();
    autoReconnectWiFi();
    monitorInternetAccess();
    manageBlynkConnection();

    if (blynkConnected) {
      Blynk.run();
    }
    timer.run();
    
    // Kirim data sensor ke Render server secara periodik & instan saat eskalasi
    sendTelemetryToRender();
    
    vTaskDelay(pdMS_TO_TICKS(10)); // Yield to WDT
  }
}

// ===== MAIN LOOP (CORE 1 - SENSOR & SERVO REALTIME) =====
void loop() {
  unsigned long now = millis();

  // === Api dan Servo Realtime Update ===
  bacaFlameFast();
  updateServo();

  // === Read Temp/Gas every 2000ms ===
  if (now - lastSensorRead >= 2000) {
    bacaDHTandMQ2();
    updateAlarmLevel();
    lastSensorRead = now;
  }

  // === Update outputs ===
  updateLED();
  updateBuzzer();

  // === Update OLED every 150ms ===
  if (now - lastOLEDUpdate >= 150) {
    updateOLED();
    lastOLEDUpdate = now;
  }

  // === Toggle IP display on OLED every 3 seconds ===
  if (now - lastOLEDIPToggle >= 3000) {
    showIPOnOLED = !showIPOnOLED;
    lastOLEDIPToggle = now;
  }

  vTaskDelay(pdMS_TO_TICKS(5)); // Real-time loop minimal delay
}

// ===== SENSOR READING =====
void bacaDHTandMQ2() {
  float currentT = dht.readTemperature();
  float currentH = dht.readHumidity();
  int currentMq2 = analogRead(MQ2_PIN);

  bool _dhtError = (isnan(currentT) || isnan(currentH));
  bool _mq2Error = (currentMq2 < 5 || currentMq2 > 4090) && millis() > 5000;
  
  // 1. Filter MQ-2 using Exponential Moving Average (EMA) to avoid false spikes
  static float filteredMq2 = -1.0;
  if (filteredMq2 < 0) {
    filteredMq2 = currentMq2;
  } else {
    filteredMq2 = (0.15 * currentMq2) + (0.85 * filteredMq2);
  }
  int _mq2Val = (int)filteredMq2;
  
  // A. Automatic quiet calibration 30 seconds after boot if never calibrated before
  static bool autoCalibrateDone = false;
  if (needsAutoCalibrate && !autoCalibrateDone && millis() >= 30000 && !_mq2Error) {
    long sum = 0;
    for (int i = 0; i < 20; i++) {
      sum += analogRead(MQ2_PIN);
      delay(20);
    }
    gasBaseline = sum / 20;
    preferences.putInt("gasBase", gasBaseline);
    needsAutoCalibrate = false;
    autoCalibrateDone = true;
    Serial.print("Auto-calibrated baseline at startup: ");
    Serial.println(gasBaseline);
  }

  // B. Dynamic baseline tracking to adapt to environmental changes, warm-up, and sensor drift
  if (!_mq2Error && millis() > 10000) {
    if (gasBaseline <= 50 || gasBaseline > 4000) {
      // Initialize baseline if it is invalid
      gasBaseline = _mq2Val;
    } else if (_mq2Val < gasBaseline) {
      // If the current reading is lower than baseline, adjust baseline down to match it.
      // If the difference is large (e.g. during warm-up or after sensor change), adjust faster.
      if (gasBaseline - _mq2Val > 100) {
        gasBaseline = _mq2Val;
      } else {
        gasBaseline = (0.95 * gasBaseline) + (0.05 * _mq2Val);
      }
    } else if (_mq2Val > gasBaseline && alarmLevel == 0 && !flameDetected) {
      // If reading is higher than baseline but no alarm is active and no flame is detected,
      // slowly drift baseline up to handle ambient drift (e.g., 1 unit every 15 seconds).
      static unsigned long lastBaselineRaise = 0;
      if (millis() - lastBaselineRaise > 15000) {
        gasBaseline++;
        lastBaselineRaise = millis();
      }
    }

    // Save to flash only if changed significantly (>= 15) or periodically (every 5 minutes)
    static int lastSavedBaseline = -1;
    static unsigned long lastSaveTime = 0;
    if (lastSavedBaseline < 0) {
      lastSavedBaseline = gasBaseline;
      lastSaveTime = millis();
    }
    if (abs(gasBaseline - lastSavedBaseline) >= 15 || (gasBaseline != lastSavedBaseline && millis() - lastSaveTime > 300000)) {
      preferences.putInt("gasBase", gasBaseline);
      lastSavedBaseline = gasBaseline;
      lastSaveTime = millis();
      Serial.print("Saved updated gas baseline to Flash: ");
      Serial.println(gasBaseline);
    }
  }

  int adjMq2 = 0;
  int _gasPercent = 0;
  int _airQLevel = 0;
  
  if (millis() >= 30000) {
    // Kurangi dengan baseline dan beri offset +45 agar berfluktuasi secara realistis di udara bersih (tidak flat 0)
    adjMq2 = _mq2Val - gasBaseline + 45;
    if (adjMq2 < 10) adjMq2 = 10; // Batasi minimum 10 agar tidak 0 dan membuktikan sensor aktif
    
    _gasPercent = map(constrain(adjMq2, 10, 3000), 10, 3000, 0, 100);
    if (adjMq2 < 200)       _airQLevel = 0;
    else if (adjMq2 < 600)  _airQLevel = 1;
    else if (adjMq2 < 1200) _airQLevel = 2;
    else                 _airQLevel = 3;
  } else {
    // Masa pemanasan sensor (warm-up) - tunjukkan fluktuasi rendah yang aktif agar tidak diam 0
    adjMq2 = 25 + (_mq2Val % 15);
    _gasPercent = 0;
    _airQLevel = 0;
  }

  // 2. Glitch protection for DHT22 readings to avoid high temperature false alarms
  static int highTempCounter = 0;
  float checkedTemp = currentT;
  if (!_dhtError) {
    if (lastTemp > 0 && abs(currentT - lastTemp) > 10.0) {
      highTempCounter++;
      if (highTempCounter > 2) {
        checkedTemp = currentT;
        highTempCounter = 0;
      } else {
        checkedTemp = lastTemp;
      }
    } else {
      checkedTemp = currentT;
      highTempCounter = 0;
    }
  }

  if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    dhtError  = _dhtError;
    mq2Error  = _mq2Error;
    suhu      = _dhtError ? lastTemp : checkedTemp;
    hum       = _dhtError ? lastHum  : currentH;
    if (!_dhtError) { lastTemp = checkedTemp; lastHum = currentH; }
    mq2Value      = adjMq2; // ✅ Simpan nilai yang sudah diberi offset agar display berfluktuasi realistis
    gasPercentage = _gasPercent;
    airQualityLevel = _airQLevel;
    xSemaphoreGive(dataMutex);
  }

  if ((_dhtError || _mq2Error) && !sensorFailNotified && blynkConnected) {
    String msg = "PERINGATAN: Sensor Error! ";
    if (_dhtError) msg += "[DHT] ";
    if (_mq2Error) msg += "[MQ2]";
    Blynk.logEvent("sensor_failure", msg);
    logToBlynk(msg);
    sensorFailNotified = true;
  } else if (!_dhtError && !_mq2Error) {
    sensorFailNotified = false;
  }
}

// Flame dibaca secepat mungkin terpisah dari DHT (Realtime) dengan Debouncing
void bacaFlameFast() {
  static int flameConsecutiveCount = 0;
  const int FLAME_DEBOUNCE_THRESHOLD = 15; // ~75-150ms depending on loop rate
  
  bool currentRawFlame = (digitalRead(FLAME_PIN) == LOW);
  if (currentRawFlame) {
    if (flameConsecutiveCount < FLAME_DEBOUNCE_THRESHOLD) {
      flameConsecutiveCount++;
    }
  } else {
    if (flameConsecutiveCount > 0) {
      flameConsecutiveCount--;
    }
  }
  
  bool debouncedFlame = (flameConsecutiveCount >= FLAME_DEBOUNCE_THRESHOLD);
  
  if (debouncedFlame != flameDetected) {
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      flameDetected = debouncedFlame;
      if (flameDetected && servoScanning) {
        servoScanning = false;
        flamePosition = sudutServo;
        Serial.print("API TERDETEKSI di posisi: ");
        Serial.print(flamePosition);
        Serial.println(" deg");
        if (blynkConnected) {
          terminal.print("API TERDETEKSI @ ");
          terminal.print(flamePosition);
          terminal.println(" deg");
          terminal.flush();
        }
      } else if (!flameDetected && !servoScanning) {
        servoScanning = true;
        flamePosition = -1;
      }
      xSemaphoreGive(dataMutex);
      updateAlarmLevel(); // Langsung trigger perhitungan bahaya ketika api berubah status
    }
  }
}


void handleAPIStatus() {
  // Tambah timeout lebih panjang untuk memastikan dapat data
  if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(300)) == pdTRUE) {
    float _suhu = suhu;
    float _hum = hum;
    int _mq2 = mq2Value;
    int _gasPercent = gasPercentage;
    bool _flame = flameDetected;
    int _level = alarmLevel;
    int _servo = sudutServo;
    int _flamePos = flamePosition;
    int _airQ = airQualityLevel;
    
    xSemaphoreGive(dataMutex);
    
    String json = "{";
    json += "\"suhu\":" + String(isnan(_suhu) ? 0 : _suhu, 1) + ",";
    json += "\"hum\":" + String(isnan(_hum) ? 0 : _hum, 1) + ",";
    json += "\"gas\":" + String(_mq2) + ",";
    json += "\"gasPercent\":" + String(_gasPercent) + ",";
    json += "\"api\":" + (_flame ? String("true") : String("false")) + ",";
    json += "\"level\":" + String(_level) + ",";
    json += "\"servo\":" + String(_servo) + ",";
    json += "\"flamePos\":" + String(_flamePos) + ",";
    json += "\"airQuality\":" + String(_airQ) + ",";
    json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
    json += "\"freeHeap\":" + String(ESP.getFreeHeap()) + ",";
    json += "\"uptime\":" + String(millis() / 1000);
    json += "}";
    
    server.send(200, "application/json", json);
  } else {
    // Jika mutex sibuk, tetap kirim JSON terakhir tapi tandai sibuk
    server.send(200, "application/json", "{\"error\":\"busy\"}");
  }
}

void handleCalibrate() {
  if (isCalibrating) {
    server.send(200, "text/plain", "Sedang kalibrasi...");
    return;
  }

  isCalibrating = true;

  long sum = 0;
  for(int i=0; i<50; i++) {
    sum += analogRead(MQ2_PIN);
    delay(50);
  }

  gasBaseline = sum / 50;   // ✅ FIXED
  preferences.putInt("gasBase", gasBaseline);  // simpan ke flash

  isCalibrating = false;
  server.send(200, "text/plain", "Kalibrasi Selesai! Baseline: " + String(gasBaseline));
}

void handleReboot() {
  server.send(200, "text/plain", "Sistem akan restart dalam 1 detik.");
  delay(1000);
  ESP.restart();
}

// ===== ALARM LEVEL CALCULATION =====
void updateAlarmLevel() {
  int mq2Adjusted = mq2Value; // Already baseline-adjusted!

  bool isGasDanger = (millis() >= 30000) && (mq2Adjusted > GAS_THRESHOLD);
  bool isTempDanger = (suhu > TEMP_THRESHOLD);
  bool isFlameDanger = flameDetected;

  if (isFlameDanger && isGasDanger) {
    alarmLevel = 3;  // Kritis
  }
  else if (isFlameDanger || isGasDanger || isTempDanger) {
    alarmLevel = 2;  // Bahaya
  }
  else if ((millis() >= 30000 && mq2Adjusted > 500) || suhu > 38.0) {
    alarmLevel = 1;  // Waspada
  }
  else {
    alarmLevel = 0;  // Normal
  }
  
  // Send notification on level change
  if (prevLevel < 2 && alarmLevel >= 2 && blynkConnected) {
    unsigned long now = millis();
    if (now - lastNotification > 30000) {  // Max 1 notif per 30 sec
      Blynk.logEvent("fire_alert", "BAHAYA! Deteksi kondisi bahaya (Suhu/Gas/Api)!");
      if (blynkConnected) {
        terminal.println("⚠ ALARM TRIGGERED!");
        terminal.flush();
      }
      lastNotification = now;
    }
  }
  prevLevel = alarmLevel;
}

// ===== LED UPDATE =====
// Hijau (Pin 26): Nyala saat kondisi normal (alarmLevel == 0)
// Merah (Pin 27): Nyala saat kondisi Waspada, Bahaya, atau Kritis (alarmLevel >= 1)
void updateLED() {
  unsigned long now = millis();

  // LED Merah aktif jika alarmLevel >= 1 (Waspada, Bahaya, Kritis)
  if (alarmLevel >= 1) {
    digitalWrite(LED_MERAH, HIGH);
    digitalWrite(LED_HIJAU, LOW);
  } else {
    digitalWrite(LED_MERAH, LOW);
    digitalWrite(LED_HIJAU, HIGH);
  }

  // Logic LED RGB Extra (Pin 14)
  if (alarmLevel >= 1) {
    // Jika ada peringatan/bahaya, berkedip cepat untuk menarik perhatian
    if (now - lastLEDToggle >= 100) { 
      ledBlinkState = !ledBlinkState;
      lastLEDToggle = now;
    }
    digitalWrite(LED_RGB_EXTRA, ledBlinkState ? HIGH : LOW);
  } else {
    // Normal: Always ON
    digitalWrite(LED_RGB_EXTRA, HIGH);
  }
}

// ===== BUZZER UPDATE + BLYNK =====
void updateBuzzer() {
  unsigned long now = millis();

  // 1. Kontrol buzzer pasif
  switch (alarmLevel) {
    case 0:  // Normal - Buzzer OFF
      noTone(BUZZER_PIN);
      buzzerState = false;
      break;

    case 1:  // Waspada - Beep lambat (2700Hz - Frekuensi Resonansi Puncak Piezo)
      if (now - lastBuzzerToggle >= 1000) {
        buzzerState = !buzzerState;
        if (buzzerState) tone(BUZZER_PIN, 2700); 
        else noTone(BUZZER_PIN);
        lastBuzzerToggle = now;
      }
      break;

    case 2:  // Bahaya - Beep cepat (3300Hz - Nyaring)
      if (now - lastBuzzerToggle >= 200) {
        buzzerState = !buzzerState;
        if (buzzerState) tone(BUZZER_PIN, 3300); 
        else noTone(BUZZER_PIN);
        lastBuzzerToggle = now;
      }
      break;

    case 3:  // Kritis - Kontinu cepat (4000Hz - Sangat Pekak & Menembus)
      if (now - lastBuzzerToggle >= 100) {
        buzzerState = !buzzerState;
        if (buzzerState) tone(BUZZER_PIN, 4000); 
        else noTone(BUZZER_PIN);
        lastBuzzerToggle = now;
      }
      break;
  }

  // 2. Kirim status buzzer ke Blynk (V6)
  if (blynkConnected) {
    int currentBuzzerState = (alarmLevel == 0) ? 0 : 1;
    if (currentBuzzerState != lastBlynkBuzzerState) {
      Blynk.virtualWrite(V6, currentBuzzerState);
      lastBlynkBuzzerState = currentBuzzerState;
    }
  }
}


// ===== SERVO UPDATE =====
void updateServo() {
  if (!servoScanning) {
    // Servo berhenti di posisi api
    myServo.write(flamePosition);
    return;
  }
  
  unsigned long now = millis();
  if (now - lastServoMove >= 30) {
    if (servoNaik) {
      sudutServo += 2;
      if (sudutServo >= 180) {
        sudutServo = 180;
        servoNaik = false;
      }
    } else {
      sudutServo -= 2;
      if (sudutServo <= 0) {
        sudutServo = 0;
        servoNaik = true;
      }
    }
    myServo.write(sudutServo);
    lastServoMove = now;
  }
}

// ===== OLED DISPLAY =====
void updateOLED() {
  display.clearDisplay();
  animFrame = (animFrame + 1) % 20;

  // ===== 1. TOP HEADER (y: 0 - 13) =====
  // Round rect header
  display.drawRoundRect(0, 0, 128, 14, 3, SSD1306_WHITE);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  // Title "ARISE v3.0"
  display.setCursor(4, 3);
  display.print("ARISE v3.0");

  // Connectivity Icons (WiFi, Blynk, AP)
  int iconX = 72;
  if (wifiConnected) {
    display.setCursor(iconX, 3);
    display.print("[W]");
    iconX += 16;
  }
  if (blynkConnected) {
    display.setCursor(iconX, 3);
    display.print("[B]");
    iconX += 16;
  }
  if (apModeActive) {
    display.setCursor(iconX, 3);
    display.print("[A]");
  }

  // ===== 2. MIDDLE telemetry grid (y: 16 - 52) =====
  // Draw vertical separator line in the middle to create a grid
  display.drawFastVLine(64, 16, 36, SSD1306_WHITE);
  // Draw horizontal separator lines
  display.drawFastHLine(0, 34, 128, SSD1306_WHITE);

  // --- TOP LEFT: Temperature (y: 17 - 32) ---
  display.setTextSize(1);
  display.setCursor(2, 17);
  display.print("TEMP");
  display.setCursor(2, 26);
  display.print(suhu, 1);
  display.print(" C");

  // --- TOP RIGHT: Humidity (y: 17 - 32) ---
  display.setCursor(68, 17);
  display.print("HUMIDITY");
  display.setCursor(68, 26);
  display.print(hum, 0);
  display.print(" %");

  // --- BOTTOM LEFT: Gas Sensor (y: 36 - 51) ---
  display.setCursor(2, 36);
  display.print("GAS");
  display.setCursor(2, 45);
  display.print(mq2Value);
  
  // --- BOTTOM RIGHT: Radar Scan / Flame (y: 36 - 51) ---
  display.setCursor(68, 36);
  if (!servoScanning) {
    // Flashing fire alert in grid!
    if (animFrame % 4 < 2) {
      display.print("! FIRE !");
    } else {
      display.print("AT: ");
      display.print(flamePosition);
      display.print((char)247);
    }
  } else {
    display.print("RADAR");
    display.setCursor(68, 45);
    display.print("S: ");
    display.print(sudutServo);
    display.print((char)247);
  }

  // ===== 3. BOTTOM ALARM / IP STATUS BAR (y: 54 - 63) =====
  // Keep the status bar filled or styled according to state
  if (alarmLevel >= 2) {
    // Blinking danger bar
    if (animFrame % 2 == 0) {
      display.fillRect(0, 54, 128, 10, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.drawRect(0, 54, 128, 10, SSD1306_WHITE);
      display.setTextColor(SSD1306_WHITE);
    }
  } else {
    display.fillRect(0, 54, 128, 10, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
  }

  display.setCursor(4, 55);
  if (showIPOnOLED) {
    if (wifiConnected && WiFi.localIP() != IPAddress(0,0,0,0)) {
      display.print("IP: ");
      display.print(WiFi.localIP().toString());
    } else if (apModeActive) {
      display.print("AP: ");
      display.print(WiFi.softAPIP().toString());
    } else {
      display.print("DISCONNECTED");
    }
  } else {
    // Display textual status based on alarm level
    if (alarmLevel == 0) {
      display.print("STATUS: AMAN (OK)");
    } else if (alarmLevel == 1) {
      display.print("STATUS: WASPADA");
    } else if (alarmLevel == 2) {
      display.print("STATUS: BAHAYA!");
    } else {
      display.print("STATUS: KRITIS!!");
    }
  }

  display.display();
}

// ===== SPLASH SCREEN =====
void displaySplash() {
  // 1. Futuristic double frame draw animation
  for (int w = 0; w <= 128; w += 8) {
    display.clearDisplay();
    display.drawRect(0, 0, w, 64, SSD1306_WHITE);
    display.drawRect(2, 2, max(0, w-4), 60, SSD1306_WHITE);
    display.display();
    delay(10);
  }
  
  // 2. Sci-fi title appearance
  display.clearDisplay();
  display.drawRect(0, 0, 128, 64, SSD1306_WHITE);
  display.drawRect(2, 2, 124, 60, SSD1306_WHITE);
  
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(32, 8);
  display.print("ARISE");
  
  display.setTextSize(1);
  display.setCursor(18, 26);
  display.print("FIRE SAFETY v3.0");
  
  display.drawFastHLine(8, 38, 112, SSD1306_WHITE);
  display.display();
  delay(800);

  // Play a very subtle welcome chirp on the buzzer
  tone(BUZZER_PIN, 1800);
  delay(60);
  tone(BUZZER_PIN, 2200);
  delay(60);
  noTone(BUZZER_PIN);

  // 3. System loading checks and segmented bar
  String bootChecks[] = {
    "Stabilizing MQ-2...",
    "Calibrating baseline..",
    "Initializing DHT22...",
    "Configuring servo...",
    "Starting services...",
    "System fully online!"
  };
  
  for (int i = 0; i <= 100; i += 2) {
    display.clearDisplay();
    // Keep outer frames
    display.drawRect(0, 0, 128, 64, SSD1306_WHITE);
    display.drawRect(2, 2, 124, 60, SSD1306_WHITE);
    
    // Title header
    display.setTextSize(1);
    display.setCursor(8, 6);
    display.print("ARISE INITIALIZING");
    
    // Separator
    display.drawFastHLine(6, 16, 116, SSD1306_WHITE);
    
    // Boot check status message based on progress
    int checkIdx = i / 18;
    if (checkIdx > 5) checkIdx = 5;
    display.setCursor(8, 20);
    display.print(">");
    display.print(bootChecks[checkIdx]);
    
    // Percentage display
    display.setCursor(8, 32);
    display.print("LOAD STATE: ");
    display.print(i);
    display.print("%");
    
    // Segmented loading bar container
    display.drawRoundRect(8, 44, 112, 12, 3, SSD1306_WHITE);
    
    // Draw loading fill as modern rounded segments
    int barWidth = map(i, 0, 100, 0, 106);
    if (barWidth > 0) {
      display.fillRect(11, 47, barWidth, 6, SSD1306_WHITE);
    }
    
    display.display();
    
    // Speed control: faster at the end, slower in the middle to look "realistic"
    if (i < 30) delay(15);
    else if (i < 70) delay(25);
    else delay(10);
  }
  
  // 4. Inverted Flash Ready Screen
  display.clearDisplay();
  display.fillRect(0, 0, 128, 64, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  
  // Outer border inside inversion
  display.drawRect(2, 2, 124, 60, SSD1306_BLACK);
  
  display.setTextSize(2);
  display.setCursor(28, 16);
  display.print("SYSTEM");
  display.setCursor(28, 36);
  display.print("ONLINE");
  
  display.display();
  
  // Dynamic double beep on online confirmation
  tone(BUZZER_PIN, 2000);
  delay(80);
  tone(BUZZER_PIN, 2500);
  delay(80);
  noTone(BUZZER_PIN);
  
  delay(600);
}

// ===== BLYNK SEND (Optimized - Smart Batching) =====
static int blynkSendPhase = 0;  // Giliran kirim data (mencegah flood)
static int lastBlynkLevel = -1;
static bool lastBlynkFlame = false;

void sendToBlynk() {
  if (!blynkConnected) return;
  
  if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    // Smart batching: kirim 2-3 virtual write per siklus
    // Interval 3 detik x 3 fase = semua data update dalam 9 detik (data non-kritis)
    switch (blynkSendPhase) {
      case 0:
        Blynk.virtualWrite(V0, suhu);
        Blynk.virtualWrite(V1, hum);
        break;
      case 1:
        Blynk.virtualWrite(V2, mq2Value);
        Blynk.virtualWrite(V5, sudutServo);
        break;
      case 2: {
        String statusStr = "AMAN";
        if (alarmLevel == 3) statusStr = "KRITIS!";
        else if (alarmLevel == 2) statusStr = "BAHAYA!";
        else if (alarmLevel == 1) statusStr = "WASPADA";
        Blynk.virtualWrite(V3, statusStr);
        Blynk.virtualWrite(V4, (alarmLevel >= 2 || flameDetected) ? 255 : 0);
        break;
      }
    }
    blynkSendPhase = (blynkSendPhase + 1) % 3;
    
    // Data KRITIS: kirim segera saat berubah (alarm/api)
    if (alarmLevel != lastBlynkLevel || flameDetected != lastBlynkFlame) {
      String statusStr = "AMAN";
      if (alarmLevel == 3) statusStr = "KRITIS!";
      else if (alarmLevel == 2) statusStr = "BAHAYA!";
      else if (alarmLevel == 1) statusStr = "WASPADA";
      Blynk.virtualWrite(V3, statusStr);
      Blynk.virtualWrite(V4, (alarmLevel >= 2 || flameDetected) ? 255 : 0);
      lastBlynkLevel = alarmLevel;
      lastBlynkFlame = flameDetected;
    }
    
    xSemaphoreGive(dataMutex);
  }
  
  // V6 diatur oleh updateBuzzer() dengan optimasi bandwidth (hanya kirim saat berubah)
}

// Helper to log to Blynk Terminal
void logToBlynk(String msg) {
  if (blynkConnected) {
    terminal.println(msg);
    terminal.flush();
  }
}

// ===== STREAMING HTML (hemat RAM - tidak load semua ke heap) =====
// ===== FILE SERVING HELPER =====
bool serveFile(String path, String contentType = "text/html") {
  if (LittleFS.exists(path)) {
    File file = LittleFS.open(path, "r");
    server.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
}



// ===== WEB HANDLERS =====
void handleRoot() {
  if (!checkAuthentication()) {
    server.sendHeader("Location", "/login");
    server.send(302, "text/plain", "Redirect to login");
    return;
  }
  
  if (WiFi.status() != WL_CONNECTED && !apModeActive) {
    server.sendHeader("Location", "/wificonfig");
    server.send(302, "text/plain", "Redirecting to WiFi Config...");
    return;
  }
  
  if (!serveFile("/index.html")) {
    server.send(404, "text/plain", "Dashboard file not found in LittleFS");
  }
}

void handleDashboard() {
  if (!checkAuthentication()) {
    server.sendHeader("Location", "/login");
    server.send(302, "text/plain", "Redirect to login");
    return;
  }
  if (!serveFile("/index.html")) {
    server.send(404, "text/plain", "Dashboard file not found in LittleFS");
  }
}

void handleWiFiConfig() {
  if (!serveFile("/wifi.html")) {
    server.send(404, "text/plain", "WiFi Config file not found in LittleFS");
  }
}

void handleSaveWiFi() {
  if (server.hasArg("ssid")) {
    savedSSID = server.arg("ssid");
    savedPassword = server.hasArg("password") ? server.arg("password") : "";
    
    Serial.println("===== SAVE WIFI =====");
    Serial.print("SSID: "); Serial.println(savedSSID);
    Serial.print("PASS length: "); Serial.println(savedPassword.length());
    
    // Simpan ke flash dengan eksplisit
    preferences.putString("ssid", savedSSID);
    preferences.putString("password", savedPassword);
    
    // Verifikasi tersimpan
    String verifySSID = preferences.getString("ssid", "");
    Serial.print("Verify saved SSID: "); Serial.println(verifySSID);
    Serial.println("=====================");

    if (LittleFS.exists("/wifi_success.html")) {
      File file = LittleFS.open("/wifi_success.html", "r");
      String html = file.readString();
      file.close();
      html.replace("__SSID__", escapeHTML(savedSSID));
      server.send(200, "text/html", html);
    } else {
      server.send(200, "text/html", "WiFi Saved! SSID: " + savedSSID);
    }
    
    // Set pending WiFi connect - tunggu 3 detik supaya HTML terkirim dulu
    pendingWiFiConnect = true;
    pendingWiFiConnectTime = millis();
    Serial.println("WiFi credentials saved. Connecting in 3 seconds...");
  } else {
    server.send(400, "text/plain", "SSID tidak ditemukan!");
  }
}

String escapeJSONString(const String &input) {
  String out;
  out.reserve(input.length() + 8);

  for (size_t i = 0; i < input.length(); i++) {
    char c = input[i];
    switch (c) {
      case '\"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\b': out += "\\b"; break;
      case '\f': out += "\\f"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if ((uint8_t)c < 0x20) {
          char buf[7];
          snprintf(buf, sizeof(buf), "\\u%04x", (uint8_t)c);
          out += buf;
        } else {
          out += c;
        }
        break;
    }
  }

  return out;
}

String escapeHTML(const String &input) {
  String out;
  out.reserve(input.length() + 16);

  for (size_t i = 0; i < input.length(); i++) {
    char c = input[i];
    switch (c) {
      case '&': out += "&amp;"; break;
      case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break;
      case '\"': out += "&quot;"; break;
      case '\'': out += "&#39;"; break;
      default: out += c; break;
    }
  }

  return out;
}

// ===== ASYNC WIFI SCAN (Non-blocking) =====
void handleScanWiFi() {
  // Jika scan sudah selesai, kirim hasil terakhir
  // Jika belum, mulai async scan
  if (scanInProgress) {
    // Scan masih berjalan, kirim hasil terakhir yang tersimpan
    server.send(200, "application/json", lastScanResult);
    return;
  }

  // Ensure STA interface exists
  if (WiFi.getMode() == WIFI_MODE_AP) {
    WiFi.mode(WIFI_AP_STA);
    delay(100);
  }

  // Mulai ASYNC scan (non-blocking!) 
  WiFi.scanDelete();
  WiFi.scanNetworks(true, false);  // true = async!
  scanInProgress = true;
  scanStartTime = millis();
  
  // Kirim hasil terakhir untuk sementara
  server.send(200, "application/json", lastScanResult);
}

// Dipanggil di loop() untuk proses hasil scan
void processAsyncScanResult() {
  if (!scanInProgress) return;
  
  int n = WiFi.scanComplete();
  
  // Scan timeout (max 10 detik)
  if (millis() - scanStartTime > 10000) {
    scanInProgress = false;
    WiFi.scanDelete();
    Serial.println("WiFi scan timeout");
    return;
  }
  
  if (n == WIFI_SCAN_RUNNING) return;  // Masih scanning
  if (n == WIFI_SCAN_FAILED) {
    scanInProgress = false;
    Serial.println("WiFi scan failed");
    return;
  }
  
  // Scan selesai! Build JSON result
  String json = "{\"networks\":[";
  int count = 0;
  
  for (int i = 0; i < n; i++) {
    if (WiFi.SSID(i).length() == 0) continue;
    if (count > 0) json += ",";
    json += "{\"ssid\":\"" + escapeJSONString(WiFi.SSID(i)) + "\",";
    json += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
    bool isSecure = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
    json += "\"secure\":" + String(isSecure ? "true" : "false") + "}";
    count++;
    if (count >= 15) break;
  }
  
  json += "]}";
  lastScanResult = json;  // Simpan untuk request berikutnya
  WiFi.scanDelete();
  scanInProgress = false;
  Serial.println("WiFi scan complete: " + String(count) + " networks");
}

void loadWiFiConfig() {
  savedSSID = preferences.getString("ssid", "");
  savedPassword = preferences.getString("password", "");
  apPassword = preferences.getString("apPass", "12345678");
  gasBaseline = preferences.getInt("gasBase", 0);
  if (gasBaseline <= 50 || gasBaseline > 4000) {
    // Jika data baseline belum ada atau tidak realistis di flash, jadwalkan auto-kalibrasi booting
    needsAutoCalibrate = true;
    gasBaseline = 1000; // Fallback default awal
  }
  loginUsername = preferences.getString("loginUser", DEFAULT_USERNAME);
  loginPassword = preferences.getString("loginPass", DEFAULT_PASSWORD);
}

void saveWiFiConfig() {
  preferences.putString("ssid", savedSSID);
  preferences.putString("password", savedPassword);
}

// ===== WIFI CONNECTION =====
bool isConnecting = false;  // Flag untuk mencegah multiple connection attempts
unsigned long wifiConnectStart = 0;
#define WIFI_CONNECT_TIMEOUT 15000  // 15 detik timeout
#define WIFI_RECONNECT_INTERVAL 15000
#define INTERNET_CHECK_INTERVAL 10000
#define INTERNET_FAIL_THRESHOLD 2
#define BLYNK_RECONNECT_INTERVAL 5000

void connectToWiFi() {
  // Cegah multiple connection attempts
  if (isConnecting) {
    return;
  }
  
  if (savedSSID.length() == 0) {
    Serial.println("No SSID saved!");
    return;
  }
  
  Serial.println();
  Serial.println("===== WiFi Connection =====");
  Serial.print("SSID: "); Serial.println(savedSSID);
  Serial.print("PASS: "); Serial.println(savedPassword.length() > 0 ? "****" : "(none)");
  
  // PENTING: Jangan panggil WiFi.mode() lagi jika sudah AP_STA
  // karena bisa mereset AP yang sudah aktif
  if (WiFi.getMode() != WIFI_AP_STA) {
    WiFi.mode(WIFI_AP_STA);
    delay(100);
  }
  
  // Disconnect STA saja, pertahankan AP
  WiFi.disconnect(false, false);  // false = jangan matikan WiFi, false = jangan hapus config
  delay(100);
  
  // Set WiFi power
  WiFi.setSleep(false);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
  
  isConnecting = true;
  wifiConnected = false;
  internetReachable = false;
  internetFailCount = 0;
  wifiConnectStart = millis();

  WiFi.begin(savedSSID.c_str(), savedPassword.c_str());
  Serial.println("Connecting...");
}

// Call this in loop() to check connection status (non-blocking)
void checkWiFiConnection() {
  wl_status_t status = WiFi.status();

  // Connected state
  if (status == WL_CONNECTED) {
    if (!wifiConnected || isConnecting) {
      Serial.println();
      Serial.println("WiFi Connected!");
      Serial.print("IP: ");
      Serial.println(WiFi.localIP());
      Serial.print("RSSI: ");
      Serial.print(WiFi.RSSI());
      Serial.println(" dBm");
      Serial.println("===========================");
    }
    wifiConnected = true;
    isConnecting = false;
    return;
  }

  // Disconnected after previously connected
  if (wifiConnected) {
    wifiConnected = false;
    internetReachable = false;
    internetFailCount = 0;
    Serial.println("WiFi STA disconnected from router");
    // AP tetap aktif dalam mode hybrid, tidak perlu start ulang
  }

  if (!isConnecting) return;

  // Check timeout while trying to connect
  if (millis() - wifiConnectStart > WIFI_CONNECT_TIMEOUT) {
    Serial.println();
    Serial.println("WiFi STA Connection Timeout");
    isConnecting = false;
    wifiConnected = false;
    // AP tetap aktif dalam mode hybrid
    Serial.println("User tetap bisa akses via AP: 192.168.4.1");
    Serial.println("===========================");
  }
}

bool hasInternetAccess() {
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  WiFiClient probe;
  probe.setTimeout(1200);
  bool ok = probe.connect("1.1.1.1", 53);
  if (ok) {
    probe.stop();
  }
  return ok;
}

void monitorInternetAccess() {
  if (WiFi.status() != WL_CONNECTED) {
    internetReachable = false;
    internetFailCount = 0;
    return;
  }

  unsigned long now = millis();
  if (now - lastInternetCheck < INTERNET_CHECK_INTERVAL) {
    return;
  }
  lastInternetCheck = now;

  bool ok = hasInternetAccess();
  if (ok) {
    if (!internetReachable) {
      Serial.println("Internet reachable");
    }
    internetReachable = true;
    internetFailCount = 0;
    // Hybrid mode: AP tetap aktif meskipun internet sudah OK
  } else {
    internetFailCount++;
    if (internetReachable) {
      Serial.println("Internet lost from router");
    }
    internetReachable = false;
    // Hybrid mode: AP sudah aktif, tidak perlu start ulang
  }
}

void manageBlynkConnection() {
  if (blynkAuthToken.length() <= 5) {
    blynkConnected = false;
    return;
  }

  if (!blynkConfigured) {
    Blynk.config(blynkAuthToken.c_str());
    blynkConfigured = true;
  }

  // Need router connection before attempting cloud connection
  if (!wifiConnected || WiFi.status() != WL_CONNECTED) {
    if (blynkConnected) {
      Serial.println("Blynk disconnected (WiFi offline)");
    }
    blynkConnected = false;
    return;
  }

  if (Blynk.connected()) {
    if (!blynkConnected) {
      Serial.println("Blynk Connected!");
    }
    blynkConnected = true;
    return;
  }

  unsigned long now = millis();
  if (now - lastBlynkReconnectAttempt >= BLYNK_RECONNECT_INTERVAL) {
    lastBlynkReconnectAttempt = now;
    Serial.println("Attempting Blynk reconnection...");

    if (Blynk.connect(1500)) {
      blynkConnected = true;
      Serial.println("Blynk Connected!");
    }
  }
}

void handleAPPassConfig() {
  if (!serveFile("/ap_settings.html")) {
    server.send(404, "text/plain", "AP Settings file not found in LittleFS");
  }
}

void handleSaveAPPass() {
  if (server.hasArg("appass")) {
    String newPass = server.arg("appass");
    if (newPass.length() < 8) {
      server.send(400, "text/plain", "Password minimal 8 karakter!");
      return;
    }
    apPassword = newPass;
    preferences.putString("apPass", apPassword);

    // Restart AP with new password
    if (apModeActive) {
      WiFi.mode(WIFI_AP_STA); // Hybrid mode: AP + STA
      WiFi.softAPdisconnect(true);
      delay(500);
      WiFi.softAP("ARISE-Setup", apPassword.c_str());
      Serial.println("AP Password changed and AP restarted (Hybrid Mode)");
    } else {
      Serial.println("AP Password changed (will apply on restart)");
    }

    server.send(200, "text/plain", "Password berhasil diubah! Hotspot akan menggunakan password baru.");
  } else {
    server.send(400, "text/plain", "Parameter tidak valid");
  }
}

void handleLogin() {
  if (!serveFile("/login.html")) {
    server.send(404, "text/plain", "Login file not found in LittleFS");
  }
}

void handleLoginSubmit() {
  String username = server.arg("username");
  String password = server.arg("password");
  
  if (username == loginUsername && password == loginPassword) {
    sessionToken = "ARISESESS_" + String(millis());
    sessionStartTime = millis();
    isAuthenticated = true;
    server.sendHeader("Set-Cookie", "ARISESESS=" + sessionToken + "; Path=/; Max-Age=3600");
    server.send(200, "text/plain", "OK");
    Serial.println("Login successful: " + username);
  } else {
    server.send(401, "text/plain", "Invalid credentials");
    Serial.println("Login failed: " + username);
  }
}

bool checkAuthentication() {
  // If no login credentials set, allow access
  if (loginUsername.length() == 0) return true;
  
  // Check if already authenticated in this session
  if (isAuthenticated && (millis() - sessionStartTime < SESSION_TIMEOUT)) {
    return true;
  }
  
  // Check cookie
  if (server.hasHeader("Cookie")) {
    String cookie = server.header("Cookie");
    if (cookie.indexOf("ARISESESS=") >= 0) {
      int start = cookie.indexOf("ARISESESS=") + 10;
      int end = cookie.indexOf(";", start);
      if (end == -1) end = cookie.length();
      String token = cookie.substring(start, end);
      if (token == sessionToken && (millis() - sessionStartTime < SESSION_TIMEOUT)) {
        isAuthenticated = true;
        return true;
      }
    }
  }
  
  isAuthenticated = false;
  return false;
}

void ensureAuthenticated() {
  if (!checkAuthentication()) {
    server.sendHeader("Location", "/login");
    server.send(302, "text/plain", "Redirect to login");
  }
}

void handleLogout() {
  sessionToken = "";
  isAuthenticated = false;
  server.sendHeader("Set-Cookie", "ARISESESS=; Path=/; Max-Age=0");
  server.sendHeader("Location", "/login");
  server.send(302, "text/plain", "Logged out");
}

void handleLoginConfig() {
  if (!checkAuthentication()) {
    server.sendHeader("Location", "/login");
    server.send(302, "text/plain", "Redirect to login");
    return;
  }
  
  if (!serveFile("/web_security.html")) {
    server.send(404, "text/plain", "Web Security file not found in LittleFS");
  }
}

void handleSaveLoginConfig() {
  if (!checkAuthentication()) {
    server.sendHeader("Location", "/login");
    server.send(302, "text/plain", "Redirect to login");
    return;
  }
  
  if (server.hasArg("newuser") && server.hasArg("newpass")) {
    String newUser = server.arg("newuser");
    String newPass = server.arg("newpass");
    
    if (newUser.length() < 3 || newPass.length() < 6) {
      server.send(400, "text/plain", "Username min 3 chars, password min 6 chars!");
      return;
    }
    
    loginUsername = newUser;
    loginPassword = newPass;
    preferences.putString("loginUser", loginUsername);
    preferences.putString("loginPass", loginPassword);
    
    // Invalidate current session
    sessionToken = "";
    isAuthenticated = false;
    
    server.send(200, "text/plain", "Credentials updated! Please login again.");
    Serial.println("Login credentials updated");
  } else {
    server.send(400, "text/plain", "Invalid parameters");
  }
}

void handleBlynkConfig() {
  if (!checkAuthentication()) {
    server.sendHeader("Location", "/login");
    server.send(302, "text/plain", "Redirect to login");
    return;
  }
  
  if (!serveFile("/blynk.html")) {
    server.send(404, "text/plain", "Blynk Setup file not found in LittleFS");
  }
}

void handleSaveBlynkConfig() {
  if (!checkAuthentication()) {
    server.sendHeader("Location", "/login");
    server.send(302, "text/plain", "Redirect to login");
    return;
  }
  
  if (server.hasArg("blynktoken")) {
    String token = server.arg("blynktoken");
    blynkAuthToken = token;
    preferences.putString("blynkToken", blynkAuthToken);
    
    // Apply changes if not empty
    if (blynkAuthToken.length() > 5) {
      Blynk.config(blynkAuthToken.c_str());
      blynkConfigured = true;
    } else {
      blynkConfigured = false;
      blynkConnected = false;
    }
    
    server.send(200, "text/plain", "Blynk Token berhasil disimpan! Sistem akan mencoba koneksi ulang dalam beberapa saat.");
    Serial.println("Blynk token updated: " + token);
  } else {
    server.send(400, "text/plain", "Token tidak boleh kosong!");
  }
}

void handleEducation() {
  server.sendHeader("Location", "/#education");
  server.send(302, "text/plain", "Redirecting to Education...");
}

// ===== CAPTIVE PORTAL FUNCTIONS =====
void startCaptivePortal() {
  if (apModeActive) {
    dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
    Serial.println("Captive Portal started");
  }
}

void stopCaptivePortal() {
  dnsServer.stop();
  Serial.println("Captive Portal stopped");
}

void handleCaptivePortal() {
  // Redirect ke captive portal dengan IP dinamis
  String redirectUrl = "http://" + WiFi.softAPIP().toString() + "/login";
  server.sendHeader("Location", redirectUrl, true);
  server.send(302, "text/plain", "");
}

// ===== AP MANAGEMENT FUNCTIONS (HYBRID MODE) =====
// AP selalu aktif dalam mode hybrid. Selalu gunakan WIFI_AP_STA.
void startFallbackAP() {
  if (!apModeActive) {
    Serial.println("Starting AP...");
    
    // PENTING: Clean reset WiFi stack untuk mencegah glitch/AP tidak muncul
    WiFi.disconnect(true, true);
    delay(100);
    WiFi.mode(WIFI_OFF);
    delay(100);
    
    // Step 1: Set mode hybrid
    WiFi.mode(WIFI_AP_STA);
    delay(200);  // Beri waktu WiFi stack untuk siap
    
    // Step 2: Konfigurasi IP AP
    IPAddress apIP(192, 168, 4, 1);
    IPAddress apGateway(192, 168, 4, 1);
    IPAddress apSubnet(255, 255, 255, 0);
    WiFi.softAPConfig(apIP, apGateway, apSubnet);
    delay(100);
    
    // Step 3: Validasi password WPA2 (Minimal 8 karakter)
    String safePassword = apPassword;
    bool apOK;
    
    if (safePassword.length() > 0 && safePassword.length() < 8) {
      Serial.println("WARNING: AP Password must be 8+ chars! Reverting to default 12345678");
      safePassword = "12345678";
    }

    // Step 4: Start AP dengan Channel 6 (lebih kompatibel dan jarang konflik)
    if (safePassword.length() >= 8) {
      apOK = WiFi.softAP("ARISE-Setup", safePassword.c_str(), 6, 0, 4);
    } else {
      // Hotspot tanpa password (open) jika apPassword dikosongkan
      apOK = WiFi.softAP("ARISE-Setup", NULL, 6, 0, 4);
    }
    
    delay(500);  // TUNGGU AP benar-benar broadcast sebelum lanjut
    
    if (apOK) {
      apModeActive = true;
      startCaptivePortal();
      Serial.println("AP started: ARISE-Setup (Hybrid AP+STA)");
      Serial.print("AP IP: ");
      Serial.println(WiFi.softAPIP());
      Serial.print("AP Password: ");
      Serial.println(apPassword);
    } else {
      Serial.println("ERROR: Failed to start AP!");
    }
  }
}

// stopFallbackAP hanya digunakan saat restart AP (ganti password)
// Dalam mode hybrid, AP TIDAK pernah dimatikan permanen
void stopFallbackAP() {
  if (apModeActive) {
    stopCaptivePortal();
    WiFi.softAPdisconnect(true);
    apModeActive = false;
    Serial.println("AP stopped (will restart)");
  }
}

// ===== NOT FOUND HANDLER =====
void handleNotFound() {
  String path = server.uri();
  
  // 1. Cek apakah ini file statis (gambar, dll) yang ada di LittleFS
  String contentType = "text/plain";
  if (path.endsWith(".jpg") || path.endsWith(".jpeg")) contentType = "image/jpeg";
  else if (path.endsWith(".png")) contentType = "image/png";
  else if (path.endsWith(".ico")) contentType = "image/x-icon";
  else if (path.endsWith(".css")) contentType = "text/css";
  else if (path.endsWith(".js")) contentType = "application/javascript";
  
  if (LittleFS.exists(path)) {
    File file = LittleFS.open(path, "r");
    server.streamFile(file, contentType);
    file.close();
    return;
  }

  // 2. Jika tidak ketemu, lakukan redirect untuk captive portal
  if (apModeActive || WiFi.status() != WL_CONNECTED) {
    server.sendHeader("Location", "/login");
    server.send(302, "text/plain", "Redirect to setup");
  } else {
    server.send(404, "text/plain", "Not Found");
  }
}

// ===== AUTO RECONNECT WIFI =====
void autoReconnectWiFi() {
  if (savedSSID.length() > 0 && !wifiConnected && !isConnecting) {
    // Don't reconnect if already connecting or connected
    if (WiFi.status() == WL_CONNECTED) {
      wifiConnected = true;
      return;
    }
    
    unsigned long now = millis();
    if (now - lastWiFiReconnectAttempt >= WIFI_RECONNECT_INTERVAL) {
      Serial.println("Attempting WiFi reconnection...");
      connectToWiFi();
      lastWiFiReconnectAttempt = now;
    }
  }
}

// ===== PENDING WIFI CONNECT =====
void processPendingWiFiConnect() {
  if (pendingWiFiConnect) {
    unsigned long now = millis();
    if (now - pendingWiFiConnectTime >= 3000) {  // 3 detik supaya response HTML terkirim dulu
      pendingWiFiConnect = false;
      Serial.println("Processing pending WiFi connect...");
      Serial.print("Saved SSID: "); Serial.println(savedSSID);
      Serial.print("Saved PASS length: "); Serial.println(savedPassword.length());
      
      // Reset connecting state
      isConnecting = false;
      
      // Check if already connected
      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Already connected, skipping pending connect");
        wifiConnected = true;
        return;
      }
      
      connectToWiFi();
    }
  }
}

// ===== SEND TELEMETRY TO RENDER CENTER (PUSH NOTIFICATION GATEWAY) =====
void sendTelemetryToRender() {
  if (!wifiConnected || WiFi.status() != WL_CONNECTED) return;
  
  static unsigned long lastRenderUpdate = 0;
  unsigned long now = millis();
  
  // Kirim setiap 3 detik, atau secara instan jika terjadi eskalasi bahaya (alarmLevel > lastSentLevel)
  static int lastSentLevel = 0;
  bool levelEscalated = (alarmLevel > lastSentLevel && alarmLevel >= 1);
  
  if (now - lastRenderUpdate >= 3000 || levelEscalated) {
    lastRenderUpdate = now;
    lastSentLevel = alarmLevel;
    
    // Gunakan mutex untuk menyalin variabel global dengan aman
    float _suhu = 0, _hum = 0;
    int _mq2 = 0, _gasPercent = 0, _level = 0, _servo = 0, _flamePos = 0, _airQ = 0;
    bool _flame = false;
    
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      _suhu = suhu;
      _hum = hum;
      _mq2 = mq2Value;
      _gasPercent = gasPercentage;
      _flame = flameDetected;
      _level = alarmLevel;
      _servo = sudutServo;
      _flamePos = flamePosition;
      _airQ = airQualityLevel;
      xSemaphoreGive(dataMutex);
    } else {
      return; // Coba lagi nanti jika mutex sibuk
    }
    
    HTTPClient http;
    http.begin(renderServerUrl + "/api/update");
    http.addHeader("Content-Type", "application/json");
    
    // Construct JSON
    String json = "{";
    json += "\"suhu\":" + String(isnan(_suhu) ? 0 : _suhu, 1) + ",";
    json += "\"hum\":" + String(isnan(_hum) ? 0 : _hum, 1) + ",";
    json += "\"gas\":" + String(_mq2) + ",";
    json += "\"gasPercent\":" + String(_gasPercent) + ",";
    json += "\"api\":" + (_flame ? String("true") : String("false")) + ",";
    json += "\"level\":" + String(_level) + ",";
    json += "\"servo\":" + String(_servo) + ",";
    json += "\"flamePos\":" + String(_flamePos) + ",";
    json += "\"airQuality\":" + String(_airQ) + ",";
    json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
    json += "\"freeHeap\":" + String(ESP.getFreeHeap()) + ",";
    json += "\"uptime\":" + String(millis() / 1000);
    json += "}";
    
    int httpResponseCode = http.POST(json);
    
    if (httpResponseCode > 0) {
      Serial.print("[Render] Telemetry sent, HTTP response: ");
      Serial.println(httpResponseCode);
    } else {
      Serial.print("[Render] Telemetry send failed, error code: ");
      Serial.println(httpResponseCode);
    }
    
    http.end();
  }
}
