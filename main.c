# Lab.-4-Micros
En este repositorio se encuentran los códigos realizados para esta practica, acerca de la Conectividad Wifi.
#include <WiFi.h>
#include <WiFiUdp.h>
#include <DHT.h>

#define SSID "UMMG-PUB-CAJICA"
#define WIFI_PASSWORD ""

#define UDP_PORT 3333
#define APP_PORT 3333

#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

#define LDR_PIN 5

#define RELAY_1 6
#define RELAY_2 7
#define RELAY_3 15
#define RELAY_4 16
const uint8_t RELAYS[4] = {RELAY_1, RELAY_2, RELAY_3, RELAY_4};

WiFiUDP udp;

bool relayState[4] = {false, false, false, false};
IPAddress peerIP;
bool havePeer = false;
unsigned long lastStream = 0;
unsigned long lastHello = 0;

char packetBuf[128];

String ackPayload() {
  String a = "A:";
  for (int i = 0; i < 4; i++) {
    if (i) a += ",";
    a += relayState[i] ? '1' : '0';
  }
  return a;
}

void sendToPhone(const String &msg) {
  if (!havePeer) return;
  udp.beginPacket(peerIP, APP_PORT);
  udp.print(msg);
  udp.endPacket();
}

void applyCommand(const String &cmd) {
  if (cmd == "PING") {
    sendToPhone(ackPayload());
    return;
  }
  if (cmd.length() < 4 || cmd.charAt(0) != 'A') return;
  int idx = cmd.charAt(1) - '1';
  int val = cmd.charAt(3) - '0';
  if (idx < 0 || idx > 3 || (val != 0 && val != 1)) return;
  relayState[idx] = val;
  digitalWrite(RELAYS[idx], val ? HIGH : LOW);
  sendToPhone(ackPayload());
}

void readPacket() {
  int n = udp.parsePacket();
  if (n <= 0) return;
  peerIP = udp.remoteIP();
  havePeer = true;
  int len = udp.read(packetBuf, sizeof(packetBuf) - 1);
  if (len < 0) len = 0;
  packetBuf[len] = 0;
  applyCommand(String(packetBuf));
}

void setup() {
  Serial.begin(115200);
  dht.begin();
  for (int i = 0; i < 4; i++) {
    pinMode(RELAYS[i], OUTPUT);
    digitalWrite(RELAYS[i], relayState[i] ? HIGH : LOW);
  }
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  udp.begin(UDP_PORT);
}

void loop() {
  readPacket();

  if (!havePeer) {
    if (millis() - lastHello >= 500) {
      lastHello = millis();
      udp.beginPacket(IPAddress(255, 255, 255, 255), APP_PORT);
      udp.print("HELLO");
      udp.endPacket();
    }
  }

  if (millis() - lastStream >= 250) {
    lastStream = millis();
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    int lux = analogReadMilliVolts(LDR_PIN);
    if (isnan(t) || isnan(h)) return;
    char line[48];
    snprintf(line, sizeof(line), "%.1f,%.1f,%d", t, h, lux);
    sendToPhone(line);
  }
}
