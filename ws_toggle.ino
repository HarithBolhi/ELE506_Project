//for 4 relay

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
  .button { padding: 15px 30px; font-size: 18px; text-align: center; outline: none; color: #fff; background-color: #0f8b8d; border: none; border-radius: 5px; user-select: none; }
  .button:active { background-color: #0d7676; transform: translateY(2px); }
  .state { font-size: 1.2rem; color: #8c8c8c; font-weight: bold; }
  </style>
</head>
<body>
  <div class="topnav">
    <h1>ESP WebSocket Server</h1>
  </div>
  <div class="content">
    %BUTTONS%
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
      document.getElementById('state' + i).innerHTML = data[i] === 1 ? 'ON' : 'OFF';
    }
  }
  function onLoad(event) {
    initWebSocket();
  }
  function toggle(index) {
    websocket.send(JSON.stringify({ led: index }));
  }
</script>
</body>
</html>
)rawliteral";

// Generate buttons for the web page
String generateButtons() {
  String buttons = "";
  for (int i = 0; i < 4; i++) {
    buttons += "<div class='card'>";
    buttons += "<h2>Output - GPIO " + String(ledPins[i]) + "</h2>";
    buttons += "<p class='state'>State: <span id='state" + String(i) + "'>OFF</span></p>";
    buttons += "<p><button onclick='toggle(" + String(i) + ")' class='button'>Toggle</button></p>";
    buttons += "</div>";
  }
  return buttons;
}

void notifyClients() {
  String stateMessage = "[";
  for (int i = 0; i < 4; i++) {
    stateMessage += String(ledStates[i]) + (i < 3 ? "," : "");
  }
  stateMessage += "]";
  ws.textAll(stateMessage);
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    DynamicJsonDocument doc(1024); // Create a JSON document
    DeserializationError error = deserializeJson(doc, data); // Parse the JSON message
    if (!error) { // Check for parsing errors
      int led = doc["led"]; // Extract the "led" field
      if (led >= 0 && led < 4) { // Validate the LED index
        ledStates[led] = !ledStates[led]; // Toggle the LED state
        notifyClients(); // Notify all clients of the new state
      }
    }
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected\n", client->id());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
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
  if (var == "BUTTONS") {
    return generateButtons();
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
  ws.cleanupClients();
  for (int i = 0; i < 4; i++) {
    digitalWrite(ledPins[i], ledStates[i]);
  }
}
