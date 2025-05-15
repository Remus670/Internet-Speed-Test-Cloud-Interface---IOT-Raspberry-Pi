#include <WiFi.h>
#include <PubSubClient.h>
#include <ESP32Servo.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// --- 1. CONFIGURATION ---
const char* ssid = "Wokwi-GUEST";
const char* password = "";

// HIVEMQ SETTINGS (Update these!)
const char* mqtt_server = "5e7f1f4650e44cd5866c6eae8357936b.s1.eu.hivemq.cloud"; 
const int mqtt_port = 8883;
const char* mqtt_user = "admin";
const char* mqtt_pass = "Segesvar2025*";

// OLED SETTINGS
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// --- 2. OBJECTS ---
WiFiClientSecure espClient;
PubSubClient client(espClient);
Servo myGauge;
const int ledPin = 2;   
const int servoPin = 18; 
const int connLedPin = 4; 
const int relayPin = 5; 


void setup() {
  Serial.begin(115200);
  pinMode(ledPin, OUTPUT);
  pinMode(connLedPin, OUTPUT);
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW); // Start with Relay OFF
  myGauge.attach(servoPin);
  myGauge.write(0); 

  // Initialize OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); 
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 10);
  display.println("Booting System...");
  display.display();

  // Network
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  // --- THE CRITICAL FIX FOR rc=-2 ---
  espClient.setInsecure();         // 1. Ignore SSL Certificates (Required for Wokwi)
  client.setBufferSize(512);       // 2. Increase Buffer Size (Required for HiveMQ)
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void updateScreen(String status, String valueStr) {
  display.clearDisplay();
  
  // Header
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("NETWORK MONITOR");
  display.drawLine(0, 10, 128, 10, WHITE); // Draw a line

  // Main Value
  display.setTextSize(2);
  display.setCursor(0, 20);
  display.println(status);
  
  display.setTextSize(1);
  display.setCursor(0, 45);
  display.print("Value: ");
  display.println(valueStr);
  
  display.display(); // Push to screen
}

void callback(char* topic, byte* payload, unsigned int length) {
  String messageTemp;
  for (int i = 0; i < length; i++) {
    messageTemp += (char)payload[i];
  }
  messageTemp.trim(); 

  Serial.print("Latency: "); Serial.println(messageTemp);

  if (messageTemp == "PANIC") {
    digitalWrite(ledPin, HIGH);
    digitalWrite(relayPin, HIGH);
    myGauge.write(180);
    updateScreen("CRITICAL", "PKT LOSS");
  } 
  else {
    digitalWrite(ledPin, LOW);
    digitalWrite(relayPin, LOW);
    
    int value = messageTemp.toInt();
    int angle = map(value, 0, 200, 0, 180);
    if (angle > 180) angle = 180;
    if (angle < 0) angle = 0;
    myGauge.write(angle);
    
    updateScreen("ONLINE", messageTemp + " ms");
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    digitalWrite(connLedPin, LOW);
    
    // Clear screen to show status
    display.clearDisplay();
    display.setCursor(0,0);
    display.println("Connecting to");
    display.println("Cloud Broker...");
    display.display();
    
    // Create a random Client ID
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    
    // Attempt to connect
    if (client.connect(clientId.c_str(), mqtt_user, mqtt_pass)) {
      Serial.println("Connected!");
      digitalWrite(connLedPin, HIGH);
      
      client.subscribe("szil/latency/final"); 
      
      // Also subscribe to the Alert topic if you used the Panic logic
      client.subscribe("szil/latency/alert");

      display.clearDisplay();
      display.setCursor(0,0);
      display.println("Connected!");
      display.println("Waiting for data...");
      display.display();
    } else {
      Serial.print("Failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  
  // --- CRITICAL FIX: Stability Delay ---
  // Give the WiFi stack 10ms to breathe. 
  // Without this, Wokwi/ESP32 often disconnects randomly.
  delay(10); 
}