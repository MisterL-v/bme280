/* 
 * @title WeatherData
 * @author MisterL_v
 * @version 1.5 (inkl. developer mode and configuration mode)
 * @date 06/2021
 * 
*/

// Before you run this code, you have to change all Device-Specific, Sever- and WiFi-Variables!

/*
 * BME-280 Wiring:
 * VIN        GND       SCL       SDA
 * Red        black    green     yellow
*/

// Librarys
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <ArduinoJson.h>

// Board Functions
#define CONFIGURATION_MODE true
#define DEVELOPER_MODE true
#define RESTART_CONFIGURATION false
#define BUZZER D8
#define RED_LED D5
#define GREEN_LED D4
#define BLUE_LED D3
#define DELAY_ALERT 10000 // Time to wait between report and continuing
#define LED_BLINK 500 // LED blink frequency 
#define DURATION_SETUP 2000 // frequency to test every component in setup.
#define WIFI_TIMEOUT 300000

// Device-Specific
long d_id;
String d_pin;
String a_token;
String d_hostname;
boolean d_dev_mode;
String d_status;
long d_baudrate;
long d_transmission;

// BME-280-Sensor
Adafruit_BME280 bme;
float w_temperature;
float w_pressure;
float w_humidity;

// WiFi-Access
String ssid;     
String password;

// Network
const char* host = "www.datakrash.net"; // example: raspberrypi
const uint16_t httpPort = 80;
String path = "/api/weatherdata/addsensordata.php"; // example: /api/data/addsensor.php
String path_reports = "/api/reports/addreport.php";

WiFiClient client;

// Start of the Program
void setup() {
  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(BLUE_LED, OUTPUT);
  set_rgb_led("off");
  load_settings();
  Serial.begin(d_baudrate);
  device_components();
  set_rgb_led("green");
  delay(5000);
  Serial.println("Booting and Initializing Datakrash-Board.");
  post_device_informations();
  delay(5000);
  connect_to_network();
  configuration_mode();
  initialize_sensor();
  Serial.println("Completed Board-Setup.");
}

// Program Loop
void loop() {
  device_status(); 
  printValues();
  store_sensor_data();
  deepsleep();
}

// Initialize of an network connection
void connect_to_network() {
  Serial.println("[Network Connection]");
  Serial.print("Connecting to ");
  Serial.print(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { // Verbindungsaufbau
    set_rgb_led("green");
    delay(LED_BLINK);
    set_rgb_led("off");
    Serial.print(".");
    delay(LED_BLINK);
    Serial.print(".");
    int connection_time = millis();
    if (connection_time >= WIFI_TIMEOUT) {
      set_rgb_led("red");
      Serial.println("WiFi Timeout detected -> Automatic Restart");
      delay(DELAY_ALERT);
      ESP.restart();
    }
  }
  set_rgb_led("green");
  Serial.println("");
  Serial.println("WiFi Connected");
  WiFi.hostname(d_hostname);
  Serial.println("Device Hostname: " + String(d_hostname));
  Serial.print("IP-Address of Device: ");
  Serial.println(WiFi.localIP());
}

// Initialize of sensor
void initialize_sensor() {
  Serial.println("[BME280 Test]");
  bool status;
  status = bme.begin(0x76);  
  if (!status) { // Initialisierung des Sensors per I²C
    set_rgb_led("red");
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    delay(DELAY_ALERT);
    Serial.println("");
    store_report("204", "Could%20not%20find%20a%20valid%20BME280%20sensor,%20check%20wiring!");
    ESP.restart();
  }
  else {
    set_rgb_led("green"); 
    Serial.println("Detected BME-280 Sensor successfully"); 
  }
}

// Serial output of sensor values
void printValues() {
  Serial.println("[Sensor Measurement]");
  
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
    set_rgb_led("red");
    Serial.println("Invalid sensor measure. [NULL] Automatic restart.");
    delay(DELAY_ALERT);
    Serial.println("");
    store_report("416", "Invalid%20sensor%20measure%20[NULL].");
    ESP.restart();
  }
}

// Save and store data in database
void store_sensor_data() {
  Serial.println("[Saving new sensor data.]");
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
  // Check HTTP status
  char status[32] = {0};
  client.readBytesUntil('\r', status, sizeof(status));
  if (strcmp(status, "HTTP/1.1 200 OK") != 0) {
    set_rgb_led("red");
    Serial.print(F("Unexpected response: "));
    Serial.println(status);
  }

  // Skip HTTP headers
  char endOfHeaders[] = "\r\n\r\n";
  if (!client.find(endOfHeaders)) {
    set_rgb_led("red");
    Serial.println(F("Invalid response"));
  }
  DynamicJsonDocument doc(192);
  // Parse JSON object
  DeserializationError error = deserializeJson(doc, client);
  if (error) {
    set_rgb_led("red");
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.c_str());
    store_report("415","Es%20ist%20ein%20Fehler%20bei%20der%20Deserialisierung%20von%20JSON-Objekten%20aufgetreten.");
    delay(DELAY_ALERT);
    return;
  }
  bool operation_done = doc["operation_done"]; // true
  if (operation_done == true) {
    set_rgb_led("green");
    Serial.println("Added new sensor data sucessfully");  
  } else {
    set_rgb_led("red");
    Serial.println("Error while adding new sensor data");  
  }
  delay(DELAY_ALERT);
  return;
}

// Deepsleep to save energy and ressources
void deepsleep() {
  Serial.println("[Deep Sleep]");
  Serial.println("Going to deep sleep. Good Night!");
  Serial.println("Automatic awake in " + String(d_transmission) + " Seconds");
  set_rgb_led("off");
  ESP.deepSleep(d_transmission * 1000000);
  set_rgb_led("off");
  yield();
}

void set_rgb_led(String color) {
  if (d_dev_mode == true || DEVELOPER_MODE == true) {
    if (color == "red") {
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(BLUE_LED, LOW);
    digitalWrite(RED_LED, HIGH);
    }
    if (color == "green") {
      digitalWrite(RED_LED, LOW);
      digitalWrite(BLUE_LED, LOW);
      digitalWrite(GREEN_LED, HIGH);
    }
    if (color == "blue") {
      digitalWrite(GREEN_LED, LOW);
      digitalWrite(RED_LED, LOW);
      digitalWrite(BLUE_LED, HIGH);
    }
  }
  if (color == "off") {
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(RED_LED, LOW);
    digitalWrite(BLUE_LED, LOW);
  }
}

void store_report(String error_values, String error_details) {
  Serial.println("[Saving a new report]");
  String url_reports = String(path_reports) + "?" + "d_id=" + String(d_id) + "&d_pin=" + String(d_pin) + "&a_token=" + String(a_token) + "&r_errorcode=" + String(error_values) + "&r_details=" + String(error_details);
  client.connect(host, httpPort);
    delay(10000);
    if (!client.connect(host, httpPort)) {
      Serial.println("connection failed");
      return;
    }
  Serial.print("Requesting URL: ");
  Serial.println(url_reports);
  client.print(String("GET ") + url_reports + " HTTP/1.1\r\n" + "Host: " + host + "\r\n" + "Connection: close\r\n\r\n"); // Daten werden per GET-Befehl an Server übergeben
  
  // Check HTTP status
  char status[32] = {0};
  client.readBytesUntil('\r', status, sizeof(status));
  if (strcmp(status, "HTTP/1.1 200 OK") != 0) {
    Serial.print(F("Unexpected response: "));
    Serial.println(status);
    return;
  }

  // Skip HTTP headers
  char endOfHeaders[] = "\r\n\r\n";
  if (!client.find(endOfHeaders)) {
    Serial.println(F("Invalid response"));
    return;
  }
  return;
}

void configuration_mode () {
  if (CONFIGURATION_MODE == true) {
    Serial.println("[Configuration Mode]");
    set_rgb_led("blue");
    client.connect(host, httpPort);
    delay(10000);
    if (!client.connect(host, httpPort)) {
      set_rgb_led("red");
      Serial.println("connection failed");
      delay(DELAY_ALERT);
      return;
    }
    Serial.print("Requesting URL: ");
    String path_device_informations = "/api/informations/device_informations.php";
    String url_device_informations = String(path_device_informations) + "?" + "d_id=" + String(d_id) + "&d_pin=" + String(d_pin) + "&a_token=" + String(a_token);
    Serial.println(url_device_informations);
    client.print(String("GET ") + url_device_informations + " HTTP/1.1\r\n" + "Host: " + host + "\r\n" + "Connection: close\r\n\r\n"); // Daten werden per GET-Befehl an Server übergeben
  
    // Check HTTP status
    char status[32] = {0};
    client.readBytesUntil('\r', status, sizeof(status));
    if (strcmp(status, "HTTP/1.1 200 OK") != 0) {
      Serial.print(F("Unexpected response: "));
      Serial.println(status);
      return;
    }
  
    // Skip HTTP headers
    char endOfHeaders[] = "\r\n\r\n";
    if (!client.find(endOfHeaders)) {
      set_rgb_led("red");
      Serial.println(F("Invalid response"));
      store_report("415","Ungültige%20Antwort%20des%20Servers.");
      delay(DELAY_ALERT);
      return;
    }
    DynamicJsonDocument doc(768);
    // Parse JSON object
    DeserializationError error = deserializeJson(doc, client);
    if (error) {
      set_rgb_led("red");
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.c_str());
      store_report("415","Es%20ist%20ein%20Fehler%20bei%20der%20Deserialisierung%20von%20JSON-Objekten%20aufgetreten.");
      delay(DELAY_ALERT);
      return;
    }
    bool operation_done = doc["operation_done"];
    String error_reason = doc["error_reason"];
    boolean deserialized_d_dev_mode = doc["d_dev_mode"];
    d_dev_mode = deserialized_d_dev_mode; 
    int deserialized_d_id = doc["device_id"]; 
    d_id = deserialized_d_id;
    String deserialized_d_hostname = doc["d_name"]; 
    d_hostname = deserialized_d_hostname;
    String deserialized_d_status = doc["d_status"]; 
    d_status = deserialized_d_status;
    String deserialized_d_wifi_ssid = doc["d_wifi_ssid"]; 
    ssid = deserialized_d_wifi_ssid;
    String deserialized_d_wifi_password = doc["d_wifi_password"]; 
    password = deserialized_d_wifi_password;
    long deserialized_d_baudrate = doc["d_baudrate"];
    d_baudrate = deserialized_d_baudrate;
    int deserialized_d_transmission = doc["d_transmission"]; 
    d_transmission = deserialized_d_transmission;
    if (operation_done == true) {
      set_rgb_led("green");
      update_device_informations();
      Serial.println("Asked new configuration informations successfully.");
    } else {
      set_rgb_led("red");
      Serial.println("Automatic Device Configuration Error.");
      Serial.print("Reason: ");
      Serial.println(error_reason);
      store_report("449","Abfrage%20der%20Geräteinformationen%20ist%20fehlgeschlagen."); 
    }
    if (RESTART_CONFIGURATION == true) {
      set_rgb_led("blue");
      Serial.println("Restarting System to release Configuration");
      delay(DELAY_ALERT);
      ESP.restart();
    }
    delay(DELAY_ALERT);
  }
}

void load_settings() {
  EEPROM.begin(512);
  d_dev_mode = EEPROM.read(1);
  d_status = eepromReadString(2);
  d_id = eepromReadLong(10);
  d_pin = eepromReadString(20);
  d_hostname = eepromReadString(40);
  d_baudrate = eepromReadLong(70);
  d_transmission = eepromReadLong(90);
  ssid = eepromReadString(110);
  password = eepromReadString(150);
  a_token = eepromReadString(190);
}

void post_device_informations() {
  Serial.println("[Loading Device-Informations]");
  Serial.print("Developer-Mode: ");
  Serial.println(d_dev_mode);
  Serial.println("Device-Status: " + d_status);
  Serial.print("Device-ID: ");
  Serial.println(d_id);
  Serial.print("Hostname: ");
  Serial.println(d_hostname);
  Serial.print("Baudrate: ");
  Serial.println(d_baudrate);
  Serial.print("Transmission: ");
  Serial.println(d_transmission);
  Serial.println("SSID: " + ssid);
}

void update_device_informations() {
  Serial.println("[Updating Device Informations]");
  EEPROM.write(1, d_dev_mode);
  eepromWriteString(2, d_status);
  eepromWriteLong(10, d_id);
  eepromWriteString(20, d_pin);
  eepromWriteString(40, d_hostname);
  eepromWriteLong(70, d_baudrate);
  eepromWriteLong(90, d_transmission);
  eepromWriteString(110, ssid);
  eepromWriteString(150, password);
  eepromWriteString(190, a_token);
  Serial.println("Updated EEPROM successfully.");
  post_device_informations();
}

void eepromWriteString(char add,String data)
{
  int _size = data.length();
  int i;
  for(i=0;i<_size;i++)
  {
    EEPROM.write(add+i,data[i]);
  }
  EEPROM.write(add+_size,'\0');   
  EEPROM.commit();
}


String eepromReadString(char add)
{
  int i;
  char data[100]; //Max 100 Bytes
  int len=0;
  unsigned char k;
  k=EEPROM.read(add);
  while(k != '\0' && len<500)   
  {    
    k=EEPROM.read(add+len);
    data[len]=k;
    len++;
  }
  data[len]='\0';
  return String(data);
}

void eepromWriteInt(int adr, int wert) {
  byte low, high;
  low=wert&0xFF;
  high=(wert>>8)&0xFF;
  EEPROM.write(adr, low); 
  EEPROM.write(adr+1, high);
  return;
} 

int eepromReadInt(int adr) {
  byte low, high;
  low=EEPROM.read(adr);
  high=EEPROM.read(adr+1);
  return low + ((high << 8)&0xFF00);
}

void eepromWriteLong(int address, long number)
{ 
  EEPROM.write(address, (number >> 24) & 0xFF);
  EEPROM.write(address + 1, (number >> 16) & 0xFF);
  EEPROM.write(address + 2, (number >> 8) & 0xFF);
  EEPROM.write(address + 3, number & 0xFF);
}

long eepromReadLong(int address)
{
  return ((long)EEPROM.read(address) << 24) +
         ((long)EEPROM.read(address + 1) << 16) +
         ((long)EEPROM.read(address + 2) << 8) +
         (long)EEPROM.read(address + 3);
}

void device_status() {
  Serial.println("[Device Status]");
  if (d_status == "offline") {
    Serial.println("Currently your device is set offline");
    Serial.println("Please update your device configuration and your device too");
    while(true) {
      set_rgb_led("red");
      delay(LED_BLINK);
      set_rgb_led("off");
      delay(LED_BLINK);
    }
  } else {
    Serial.println("Device Status is set online");
    set_rgb_led("green");
  }
}

void device_components() {
  if (d_dev_mode == true) {
    tone(BUZZER, 100, DURATION_SETUP);
    delay(DURATION_SETUP);
    set_rgb_led("red");
    delay(DURATION_SETUP);
    set_rgb_led("green");
    delay(DURATION_SETUP);
    set_rgb_led("blue");
    delay(DURATION_SETUP);
    set_rgb_led("off");
    delay(DURATION_SETUP);
  }    
}
