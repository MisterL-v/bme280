/* 
 * @title WeatherData
 * @author MisterL_v
 * @version 1.4 (inkl. Device-Specific Informations)
 * @date 03/2021
 * 
*/

// Before you run this code, you have to change all Device-Specific, Sever- and WiFi-Variables!

/*
 * BME-280 Wiring:
 * VIN        GND       SCL       SDA
 * Red        black    green     yellow
*/

// Librarys
#include <ESP8266WiFi.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

// Device-Specific
int BAUD_RATE = 115200;
int deep_sleep = 1800; //Sendefrequenz in Sekunden
long d_id = 0;
String d_pin = ""; 
String a_token = "";
String device_hostname = "";

// BME-280-Sensor
Adafruit_BME280 bme;
float w_temperature;
float w_pressure;
float w_humidity;

// WiFi-Access
#define STASSID ""
#define STAPSK  ""
const char* ssid     = STASSID;
const char* password = STAPSK;

// Network
const char* host = "www.datakrash.net"; // example: raspberrypi
const uint16_t httpPort = 80;
String path = "/api/weatherdata/addsensordata.php"; // example: /api/data/addsensor.php
String path_reports = "/api/reports/addreport.php";
WiFiClient client;

// Start of the Program
void setup() {
  Serial.begin(BAUD_RATE);
  delay(2500);
  Serial.println("Booting...");
  delay(5000);
  Serial.println("----> Setup <----");
  connect_to_network();
  initialize_sensor();
  Serial.println("Setup Done.");
}

// Program Loop
void loop() { 
  Serial.println("----> Loop <----");
  printValues();
  storedata();
  deepsleep();
}

// Initialize of an network connection
void connect_to_network() {
  Serial.println("-- Network Connection --");
  Serial.print("Connecting to ");
  Serial.print(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { // Verbindungsaufbau
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  WiFi.hostname(device_hostname);
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

// Initialize of sensor
void initialize_sensor() {
  Serial.println("-- BME280 Test --");
  bool status;
  status = bme.begin(0x76);  
  if (!status) { // Initialisierung des Sensors per I²C
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    String error_wire = "204";
    String error_wire_details = "Could%20not%20find%20a%20valid%20BME280%20sensor,%20check%20wiring!";
    Serial.println("-- Saving Values in DB --");
    client.connect(host, httpPort);
    delay(10000);
    if (!client.connect(host, httpPort)) {
      Serial.println("connection failed");
      return;
    }
    String url_reports = String(path_reports) + "?" + "d_id=" + String(d_id) + "&d_pin=" + String(d_pin) + "&a_token=" + String(a_token) + "&r_errorcode=" + String(error_wire) + "&r_details=" + String(error_wire_details);
    Serial.print("Requesting URL: ");
    Serial.println(url_reports);
    client.print(String("GET ") + url_reports + " HTTP/1.1\r\n" + "Host: " + host + "\r\n" + "Connection: close\r\n\r\n"); // Daten werden per GET-Befehl an Server übergeben
    unsigned long timeout = millis();
    while (client.available() == 0) {
      if (millis() - timeout > 5000){
        Serial.println(">>> Client Timeout !");
        client.stop(); return; 
      } 
    }
    while (client.available()) { 
      String line = client.readStringUntil('\r'); Serial.print(line);
    }
    Serial.println();
    client.stop();
    ESP.restart();
  }
}

// Serial output of sensor values
void printValues() {
  Serial.println("-- Sensor Values --");
  
  w_temperature = bme.readTemperature();
  Serial.print("Temperature = ");
  Serial.print(w_temperature);
  Serial.println(" *C");
  
  w_pressure = bme.readPressure();
  Serial.print("Pressure = ");
  Serial.print(w_pressure / 100.0F);
  Serial.println(" hPa");
  
  w_humidity = bme.readHumidity();
  Serial.print("Humidity = ");
  Serial.print(w_humidity);
  Serial.println(" %");

  if(w_pressure == 0 or w_pressure == NULL or w_humidity == NULL or w_temperature == NULL) {
      Serial.println("Invalid sensor measure. [NULL] Automatic restart.");
      String error_values = "416";
      String error_values_details = "Invalid%20sensor%20measure%20[NULL].";
      Serial.println("-- Saving Values in DB --");
      client.connect(host, httpPort);
      delay(10000);
      if (!client.connect(host, httpPort)) {
        Serial.println("connection failed");
        return;
      }
      String url_reports = String(path_reports) + "?" + "d_id=" + String(d_id) + "&d_pin=" + String(d_pin) + "&a_token=" + String(a_token) + "&r_errorcode=" + String(error_values) + "&r_details=" + String(error_values_details);
      Serial.print("Requesting URL: ");
      Serial.println(url_reports);
      client.print(String("GET ") + url_reports + " HTTP/1.1\r\n" + "Host: " + host + "\r\n" + "Connection: close\r\n\r\n"); // Daten werden per GET-Befehl an Server übergeben
      unsigned long timeout = millis();
      while (client.available() == 0) {
        if (millis() - timeout > 5000){
          Serial.println(">>> Client Timeout !");
          client.stop(); return; 
        } 
      }
      while (client.available()) { 
        String line = client.readStringUntil('\r'); Serial.print(line);
      }
      Serial.println();
      client.stop();
      ESP.restart();
  }
}

// Save and store data in database
void storedata() {
  Serial.println("-- Saving Values in DB --");
  client.connect(host, httpPort);
  delay(10000);
  if (!client.connect(host, httpPort)) {
  Serial.println("connection failed");
  return;
  }
  String url = String(path) + "?" + "d_id=" + String(d_id) + "&d_pin=" + String(d_pin) + "&a_token=" + String(a_token) + "&w_temperature=" + String(w_temperature) + "&w_pressure=" + String(w_pressure) + "&w_humidity=" + String(w_humidity);
  Serial.print("Requesting URL: ");
  Serial.println(url);
  client.print(String("GET ") + url + " HTTP/1.1\r\n" + "Host: " + host + "\r\n" + "Connection: close\r\n\r\n"); // Daten werden per GET-Befehl an Server übergeben
  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 5000){
      Serial.println(">>> Client Timeout !");
      client.stop(); return; 
      } 
  }
  while (client.available())
  { 
    String line = client.readStringUntil('\r'); Serial.print(line);
  }
  Serial.println();
  client.stop();
}

// Deepsleep to save energy and ressources
void deepsleep() {
  Serial.println("-- Deep Sleep --");
  Serial.println("Going to deep sleep. Good Night!");
  Serial.println("Automatic awake in " + String(deep_sleep) + " Seconds");
  ESP.deepSleep(deep_sleep * 1000000);
  yield();
}
