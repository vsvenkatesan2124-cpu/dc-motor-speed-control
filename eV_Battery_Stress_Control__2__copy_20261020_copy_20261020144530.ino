#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>
#include <SD.h>
#include <SPI.h>

// ===== Configuration =====
const char* ssid = "Funda";
const char* password = "vsvenky2005";
const int SD_CS = 5;
const char* filename = "/project/data.txt";

// Servo Configuration
const int servoPins[6] = {13, 12, 14, 27, 26, 25};
const int raisedPos = 90;   // Raised position
const int loweredPos = 0;   // Lowered position

// Button Configuration
const int forwardButtonPin = 32;
const int previousButtonPin = 33;

// UI Colors
const String colorRaised = "#4CAF50";  // Green
const String colorLowered = "#F44336"; // Red
const String primaryColor = "#1E90FF"; // Dodger Blue
const String secondaryColor = "#FFA500"; // Orange
const String backgroundColor = "#87CEEB"; // Sky Blue

// ===== Global Objects =====
WebServer server(80);
Servo servos[6];

// ===== State Variables =====
String textData = "";
String onlineText = "";
int currentCharIndex = 0;
bool useSDMode = true;
bool sdCardAvailable = false;

// ===== Function Prototypes =====
void initializeServos();
void initializeSDCard(bool initialSetup = false);
void connectToWiFi();
void setupWebServer();
void updateBrailleDisplay(char c);
void handleButtons();
void handleModeChange();
void handleOnlineUpdate();
void handleClearOnline();
void handlePrevious();
void handleNext();
void handleCurrentBraille();
String getCurrentChar();
void charToBraille(char c, bool* pattern);

void setup() {
  Serial.begin(115200);
  while (!Serial); // Wait for serial port

  pinMode(forwardButtonPin, INPUT_PULLUP);
  pinMode(previousButtonPin, INPUT_PULLUP);
  
  initializeServos();
  initializeSDCard(true);
  
  connectToWiFi();
  setupWebServer();

  updateBrailleDisplay(' ');
}

void loop() {
  server.handleClient();
  handleButtons();
  delay(10);
}

// ===== Hardware Initialization =====
void initializeServos() {
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);

  Serial.println("Initializing servos...");
  for (int i = 0; i < 6; i++) {
    servos[i].setPeriodHertz(50);
    if (!servos[i].attach(servoPins[i], 500, 2400)) {
      Serial.println("Error attaching servo " + String(i));
      continue;
    }
    servos[i].write(loweredPos);
    delay(100);
  }
}

void initializeSDCard(bool initialSetup) {
  sdCardAvailable = false;
  textData = "";

  if (SD.begin(SD_CS)) {
    SD.end();
  }

  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  delay(100);

  Serial.println("Initializing SD card...");
  
  for (int i = 0; i < 3; i++) {
    if (SD.begin(SD_CS)) {
      sdCardAvailable = true;
      break;
    }
    delay(500);
    Serial.print("Retrying SD initialization... ");
  }

  if (!sdCardAvailable) {
    Serial.println("\nSD Card initialization failed!");
    return;
  }

  File file = SD.open(filename);
  if (!file) {
    Serial.println("Failed to open file");
    SD.end();
    sdCardAvailable = false;
    return;
  }

  while (file.available()) {
    char c = file.read();
    if (isPrintable(c)) textData += c;
  }
  file.close();

  sdCardAvailable = (textData.length() > 0);
  Serial.println(sdCardAvailable ? 
    "SD Card ready (" + String(textData.length()) + " chars)" : 
    "SD Card empty");

  if (!initialSetup && sdCardAvailable) {
    currentCharIndex = 0;
    updateBrailleDisplay(textData.charAt(0));
  }
}

// ===== WiFi Connection =====
void connectToWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi...");
  
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 15000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected! IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("\nFailed to connect! Starting AP...");
    WiFi.softAP("BrailleAP", "12345678");
    Serial.println("AP IP: " + WiFi.softAPIP().toString());
  }
}

// ===== Web Server Setup =====
void setupWebServer() {
  server.on("/", HTTP_GET, []() {
    String html = R"=====(
<!DOCTYPE html><html><head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Braille Display Controller</title>
<style>
  body {
    font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
    margin: 0;
    padding: 20px;
    background: )=====" + backgroundColor + R"=====(;
    color: #333;
  }
  .container {
    max-width: 900px;
    margin: 0 auto;
    background: white;
    padding: 25px;
    border-radius: 12px;
    box-shadow: 0 4px 12px rgba(0,0,0,0.1);
  }
  h1 {
    color: #2c3e50;
    text-align: center;
    margin-bottom: 25px;
  }
  button {
    background: )=====" + primaryColor + R"=====(;
    color: white;
    border: none;
    padding: 10px 18px;
    border-radius: 6px;
    cursor: pointer;
    margin: 5px;
    font-size: 16px;
    transition: all 0.3s;
  }
  button:hover {
    background: #0066CC;
    transform: translateY(-2px);
  }
  .btn-secondary {
    background: )=====" + secondaryColor + R"=====(;
  }
  .btn-secondary:hover {
    background: #CC8400;
  }
  .text-display {
    font-size: 24px;
    padding: 15px;
    background: #f8f9fa;
    border-radius: 8px;
    min-height: 60px;
    text-align: center;
    margin: 15px 0;
  }
  .current-char {
    font-size: 72px;
    text-align: center;
    margin: 25px 0;
    color: #2c3e50;
  }
  .dot-status {
    display: flex;
    justify-content: center;
    gap: 15px;
    margin: 20px 0;
  }
  .dot {
    width: 40px;
    height: 40px;
    border-radius: 50%;
    display: flex;
    align-items: center;
    justify-content: center;
    font-weight: bold;
    box-shadow: 0 3px 6px rgba(0,0,0,0.1);
  }
  .status-bar {
    background: #f8f9fa;
    padding: 10px;
    border-radius: 6px;
    margin-bottom: 20px;
  }
  input[type="text"] {
    padding: 8px;
    font-size: 16px;
    border: 1px solid #ddd;
    border-radius: 4px;
    margin-right: 10px;
  }
</style>
</head><body>
<div class="container">
  <h1>Braille Display Controller</h1>
  
  <div class="status-bar">
    <strong>Status:</strong> 
    <span id="wifiStatus">)=====" + (WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "AP Mode") + R"=====(</span> | 
    <span id="sdStatus">)=====" + 
    (sdCardAvailable ? "SD Card Ready" : 
    (useSDMode ? "SD Card Failed" : "SD Card Disabled")) + 
    R"=====(</span>
  </div>

  <div style="margin: 20px 0;">
    <form action="/mode" method="post">
      <button type="submit" name="mode" value="sd" )=====" + (useSDMode ? "disabled" : "") + R"=====(>SD Mode</button>
      <button type="submit" name="mode" value="online" class="btn-secondary" )=====" + (!useSDMode ? "disabled" : "") + R"=====(>Online Mode</button>
    </form>
  </div>

  )=====";

    if (useSDMode) {
      html += R"=====(
  <div>
    <h3>SD Card Content</h3>
    <div class="text-display">)=====" + (textData.length() > 0 ? textData : "No content available") + R"=====(</div>
    <p>Current Position: )=====" + String(currentCharIndex + 1) + "/" + String(textData.length()) + R"=====(</p>
    <div>
      <form action="/prev" method="post"><button type="submit">Previous</button></form>
      <form action="/next" method="post"><button type="submit">Next</button></form>
    </div>
  </div>
)=====";
    } else {
      html += R"=====(
  <div>
    <h3>Online Input</h3>
    <form action="/updateOnline" method="post">
      <input type="text" name="letter" maxlength="1" placeholder="Enter character">
      <button type="submit">Add Character</button>
      <button type="button" onclick="clearText()" class="btn-secondary">Clear</button>
    </form>
    <div class="text-display" id="onlineText">)=====" + (onlineText.length() > 0 ? onlineText : " ") + R"=====(</div>
  </div>
)=====";
    }

    html += R"=====(
  <div>
    <h3>Current Braille Character</h3>
    <div class="current-char" id="currentChar">)=====" + getCurrentChar() + R"=====(</div>
    <div class="dot-status" id="dotStatus"></div>
  </div>

  <script>
  function updateDisplay() {
    fetch('/currentBraille')
      .then(r => r.json())
      .then(data => {
        document.getElementById('currentChar').textContent = data.char;
        let dots = '';
        data.pattern.forEach((state, index) => {
          dots += `<div class="dot" style="background:${state ? ')" + colorRaised + R"=====(' : ')" + colorLowered + R"=====('}">${index+1}</div>`;
        });
        document.getElementById('dotStatus').innerHTML = dots;
      });
  }
  setInterval(updateDisplay, 1000);
  
  function clearText() {
    fetch('/clearOnline', {method: 'POST'}).then(() => location.reload());
  }
  </script>
</div>
</body></html>
)=====";

    server.send(200, "text/html", html);
  });

  server.on("/mode", HTTP_POST, handleModeChange);
  server.on("/updateOnline", HTTP_POST, handleOnlineUpdate);
  server.on("/clearOnline", HTTP_POST, handleClearOnline);
  server.on("/prev", HTTP_POST, handlePrevious);
  server.on("/next", HTTP_POST, handleNext);
  server.on("/currentBraille", HTTP_GET, handleCurrentBraille);
  
  server.begin();
  Serial.println("HTTP server started");
}

// ===== Server Handlers =====
void handleModeChange() {
  if (server.hasArg("mode")) {
    bool newMode = (server.arg("mode") == "sd");
    
    if (newMode && !useSDMode) {
      useSDMode = true;
      initializeSDCard(false);
      
      if (sdCardAvailable && textData.length() > 0) {
        currentCharIndex = 0;
        updateBrailleDisplay(textData.charAt(currentCharIndex));
      }
    } else if (!newMode) {
      useSDMode = false;
      if (SD.begin(SD_CS)) {
        SD.end();
      }
    }
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleOnlineUpdate() {
  if (server.hasArg("letter")) {
    String letter = server.arg("letter");
    if (letter.length() > 0) {
      onlineText += letter.charAt(0);
      updateBrailleDisplay(letter.charAt(0));
    }
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleClearOnline() {
  onlineText = "";
  server.sendHeader("Location", "/");
  server.send(303);
}

void handlePrevious() {
  if (currentCharIndex > 0) currentCharIndex--;
  if (sdCardAvailable && textData.length() > 0) {
    updateBrailleDisplay(textData.charAt(currentCharIndex));
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleNext() {
  if (currentCharIndex < textData.length() - 1) currentCharIndex++;
  if (sdCardAvailable && textData.length() > 0) {
    updateBrailleDisplay(textData.charAt(currentCharIndex));
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleCurrentBraille() {
  String json = "{\"char\":\"" + getCurrentChar() + "\",\"pattern\":[";
  bool pattern[6];
  charToBraille(getCurrentChar()[0], pattern);
  for (int i = 0; i < 6; i++) {
    json += String(pattern[i] ? 1 : 0);
    if (i < 5) json += ",";
  }
  json += "]}";
  server.send(200, "application/json", json);
}

// ===== Core Functions =====
String getCurrentChar() {
  if (useSDMode) {
    return textData.length() > 0 ? String(textData.charAt(currentCharIndex)) : " ";
  }
  return onlineText.length() > 0 ? String(onlineText.charAt(onlineText.length()-1)) : " ";
}

void updateBrailleDisplay(char c) {
  bool pattern[6] = {false};
  charToBraille(c, pattern);
  for (int i = 0; i < 6; i++) {
    if (servos[i].attached()) {
      servos[i].write(pattern[i] ? raisedPos : loweredPos);
      delay(20);
    }
  }
  Serial.println("Displaying: " + String(c));
}

void charToBraille(char c, bool* pattern) {
  memset(pattern, 0, 6 * sizeof(bool));
  c = tolower(c);

  // Numbers (prefixed with number sign)
  if (c >= '0' && c <= '9') {
    pattern[2] = pattern[3] = pattern[4] = pattern[5] = true;
    char numChar = (c == '0') ? 'j' : 'a' + (c - '1');
    c = numChar;
  }

  // Letters
  if (c >= 'a' && c <= 'z') {
    switch(c) {
      case 'a': pattern[0] = true; break;
      case 'b': pattern[0] = pattern[1] = true; break;
      case 'c': pattern[0] = pattern[3] = true; break;
      case 'd': pattern[0] = pattern[3] = pattern[4] = true; break;
      case 'e': pattern[0] = pattern[4] = true; break;
      case 'f': pattern[0] = pattern[1] = pattern[3] = true; break;
      case 'g': pattern[0] = pattern[1] = pattern[3] = pattern[4] = true; break;
      case 'h': pattern[0] = pattern[1] = pattern[4] = true; break;
      case 'i': pattern[1] = pattern[3] = true; break;
      case 'j': pattern[1] = pattern[3] = pattern[4] = true; break;
      case 'k': pattern[0] = pattern[2] = true; break;
      case 'l': pattern[0] = pattern[1] = pattern[2] = true; break;
      case 'm': pattern[0] = pattern[2] = pattern[3] = true; break;
      case 'n': pattern[0] = pattern[2] = pattern[3] = pattern[4] = true; break;
      case 'o': pattern[0] = pattern[2] = pattern[4] = true; break;
      case 'p': pattern[0] = pattern[1] = pattern[2] = pattern[3] = true; break;
      case 'q': pattern[0] = pattern[1] = pattern[2] = pattern[3] = pattern[4] = true; break;
      case 'r': pattern[0] = pattern[1] = pattern[2] = pattern[4] = true; break;
      case 's': pattern[1] = pattern[2] = pattern[3] = true; break;
      case 't': pattern[1] = pattern[2] = pattern[3] = pattern[4] = true; break;
      case 'u': pattern[0] = pattern[2] = pattern[5] = true; break;
      case 'v': pattern[0] = pattern[1] = pattern[2] = pattern[5] = true; break;
      case 'w': pattern[1] = pattern[3] = pattern[4] = pattern[5] = true; break;
      case 'x': pattern[0] = pattern[2] = pattern[3] = pattern[5] = true; break;
      case 'y': pattern[0] = pattern[2] = pattern[3] = pattern[4] = pattern[5] = true; break;
      case 'z': pattern[0] = pattern[2] = pattern[4] = pattern[5] = true; break;
    }
  }
  // Punctuation
  else {
    switch(c) {
      case '.': pattern[1] = pattern[4] = pattern[5] = true; break;
      case ',': pattern[1] = true; break;
      case '!': pattern[1] = pattern[2] = pattern[4] = true; break;
      case '?': pattern[1] = pattern[2] = pattern[5] = true; break;
      case ';': pattern[1] = pattern[2] = true; break;
      case ':': pattern[1] = pattern[4] = true; break;
      case '-': pattern[2] = pattern[5] = true; break;
      case '/': pattern[2] = pattern[3] = true; break;
      case '(': 
      case ')': pattern[1] = pattern[2] = pattern[4] = pattern[5] = true; break;
      case '\'': pattern[2] = true; break;
      case '"': pattern[1] = pattern[2] = pattern[5] = true; break;
      case '@': pattern[3] = true; break;
      case ' ': break;
      default:  // Unknown character
        for(int i=0; i<6; i++) pattern[i] = true;
    }
  }
}

// ===== Button Handling =====
void handleButtons() {
  if (!useSDMode || !sdCardAvailable) return;

  static unsigned long lastPress = 0;
  if (millis() - lastPress < 300) return;

  if (digitalRead(forwardButtonPin) == LOW) {
    if (currentCharIndex < textData.length() - 1) {
      currentCharIndex++;
      updateBrailleDisplay(textData.charAt(currentCharIndex));
      lastPress = millis();
    }
  }
  else if (digitalRead(previousButtonPin) == LOW) {
    if (currentCharIndex > 0) {
      currentCharIndex--;
      updateBrailleDisplay(textData.charAt(currentCharIndex));
      lastPress = millis();
    }
  }
}