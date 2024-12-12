// Import required libraries
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

// Replace with your network credentials
const char* ssid = "Bolhinet Maxis 2.4g";
const char* password = "Bolhi87654321";

// Define LED pins and states
const int ledPins[] = {16, 5, 4, 0}; // GPIO16, GPIO5, GPIO4, GPIO0
bool ledStates[] = {0, 0, 0, 0};     // Initial states for the 4 LEDs
int randomNumbers[] = {0, 0, 0, 0};  // Store random numbers for display

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>ESP Web Server</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
  html { font-family: Arial, Helvetica, sans-serif; text-align: center; }
  h1 { font-size: 1.8rem; color: white; }
  .topnav { overflow: hidden; background-color: #143642; }
  body { margin: 0; }
  .content { padding: 30px; max-width: 600px; margin: 0 auto; }
  .card { background-color: #F8F7F9; box-shadow: 2px 2px 12px 1px rgba(140,140,140,.5); padding: 20px; margin: 10px; }
  .state { font-size: 1.2rem; color: #8c8c8c; font-weight: bold; }
  .number { font-size: 1.2rem; color: #143642; font-weight: bold; }
  </style>
</head>
<body>
  <div class="topnav">
    <h1>ESP Automatic Random Number</h1>
  </div>
  <div class="content">
    %CARDS%
  </div>
<script>
  var gateway = `ws://${window.location.hostname}/ws`;
  var websocket;
  window.addEventListener('load', onLoad);
  function initWebSocket() {
    console.log('Trying to open a WebSocket connection...');
    websocket = new WebSocket(gateway);
    websocket.onopen    = onOpen;
    websocket.onclose   = onClose;
    websocket.onmessage = onMessage;
  }
  function onOpen(event) { console.log('Connection opened'); }
  function onClose(event) {
    console.log('Connection closed');
    setTimeout(initWebSocket, 2000);
  }
  function onMessage(event) {
    var data = JSON.parse(event.data);
    for (let i = 0; i < 4; i++) {
      document.getElementById('state' + i).innerHTML = data.states[i] === 1 ? 'ON' : 'OFF';
      document.getElementById('number' + i).innerHTML = data.numbers[i];
    }
  }
  function onLoad(event) {
    initWebSocket();
  }
</script>
</body>
</html>
)rawliteral";

// Generate cards for the web page
String generateCards() {
  String cards = "";
  for (int i = 0; i < 4; i++) {
    cards += "<div class='card'>";
    cards += "<h2>Output - GPIO " + String(ledPins[i]) + "</h2>";
    cards += "<p class='state'>State: <span id='state" + String(i) + "'>OFF</span></p>";
    cards += "<p class='number'>Random Number: <span id='number" + String(i) + "'>0</span></p>";
    cards += "</div>";
  }
  return cards;
}

void notifyClients() {
  DynamicJsonDocument doc(512);
  JsonArray states = doc.createNestedArray("states");
  JsonArray numbers = doc.createNestedArray("numbers");
  
  for (int i = 0; i < 4; i++) {
    states.add(ledStates[i]);
    numbers.add(randomNumbers[i]);
  }
  
  String jsonString;
  serializeJson(doc, jsonString);
  ws.textAll(jsonString);
}

void updateRandomNumbers() {
  for (int i = 0; i < 4; i++) {
    randomNumbers[i] = random(0, 101); // Generate random number
    ledStates[i] = (randomNumbers[i] > 50); // Update LED state
    digitalWrite(ledPins[i], ledStates[i]); // Update hardware state
  }
  notifyClients(); // Notify connected clients of updates
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected\n", client->id());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    default:
      break;
  }
}

void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

String processor(const String& var) {
  if (var == "CARDS") {
    return generateCards();
  }
  return String();
}

void setup() {
  Serial.begin(115200);
  for (int i = 0; i < 4; i++) {
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], LOW);
  }
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println(WiFi.localIP());

  initWebSocket();

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html, processor);
  });

  server.begin();
}

void loop() {
  static unsigned long lastUpdate = 0;
  unsigned long now = millis();
  
  if (now - lastUpdate >= 5000) { // Update every 5 seconds
    lastUpdate = now;
    updateRandomNumbers();
  }
  
  ws.cleanupClients();
}
