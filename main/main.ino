#include <WiFiManager.h>
#include <Preferences.h>
#include <WiFiClientSecure.h>
#include <WiFi.h>
#include <MQTTClient.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Adafruit_SHT31.h>
#include <TimeLib.h>
#include <LittleFS.h>
#include "secrets.h"

WiFiManager wifiManager;
Preferences preferences;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");
Adafruit_SHT31 sht31 = Adafruit_SHT31();
WiFiClientSecure net = WiFiClientSecure();
MQTTClient client = MQTTClient(256);


const char* uniqueIdentifierKey = "uniqueID";
char uniqueIdentifierValue[36];

void storeStringVariable(const char* filename, const String& data) {
  File file = LittleFS.open(filename, "w");
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }

  if (file.print(data)) {
    Serial.println("Data stored successfully");
  } else {
    Serial.println("Failed to store data");
  }

  file.close();
}

String retrieveStringVariable(const char* filename) {
  String data;
  File file = LittleFS.open(filename, "r");
  if (!file) {
    Serial.println("Failed to open file for reading");
    return "";
  }

  while (file.available()) {
    data += (char)file.read();
  }

  file.close();
  return data;
}

void setup() {
  Serial.begin(115200);

  // Initialize LittleFS
  if (!LittleFS.begin(true)) {
    Serial.println("Failed to initialize LittleFS");
    while (1);
  }

  // For testing
  // wifiManager.resetSettings();

  WiFiManagerParameter uniqueIdentifier("UID", "Copy SHT31-D sensor ID from app", "", 36);
  wifiManager.addParameter(&uniqueIdentifier);
  
  // Open the preferences storage with a namespace of "settings" and read-write permissions
  preferences.begin("settings", false);
  
  String sensorId = retrieveStringVariable("/data.txt");
  
  // Check if the unique identifier is available in preferences
  if (sensorId.length() != 36) {
    // If not available, start WiFiManager configuration portal
    Serial.println("Configuring WiFi...");
    bool res;
    res = wifiManager.autoConnect("SHT31-D-SetupAP"); // Start WiFiManager and create an AP named "ESP32-SetupAP"
    if(!res) {
        Serial.println("Failed to connect");
    } else {
        //if you get here you have connected to the WiFi    
        Serial.println("connected...yeey :)");
    }
    
    // Retrieve entered WiFi credentials and unique identifier from WiFiManager
    String ssid = WiFi.SSID();
    String password = WiFi.psk();
    strcpy(uniqueIdentifierValue, uniqueIdentifier.getValue());
    
    // Store WiFi credentials and unique identifier in preferences
    preferences.putString("ssid", ssid);
    preferences.putString("password", password);
    preferences.putString(uniqueIdentifierKey, String(uniqueIdentifierValue));
    storeStringVariable("/data.txt", uniqueIdentifierValue);
  } else {
    // If WiFi is already configured, retrieve stored WiFi credentials and unique identifier from preferences
    String ssid = preferences.getString("ssid");
    String password = preferences.getString("password");
    
    // Connect to WiFi
    Serial.println("Connecting to WiFi...");
    WiFi.begin(ssid.c_str(), password.c_str());
    while (WiFi.status() != WL_CONNECTED) {
      delay(1000);
      Serial.println("Connecting to WiFi...");
    }
    Serial.println("Connected to WiFi.");
  }
  
  // Initialize NTP client
  timeClient.begin();
  
  // Synchronize time
  while (!timeClient.update()) {
    timeClient.forceUpdate();
  }

  // Initialize sht31-d sensor
  while (!sht31.begin(0x44)) {
    Serial.println("Couldn't find SHT31-D");
    while(1) delay(1);
  }
  
  // Get and print UTC time
  Serial.print("Setup UTC time: ");
  Serial.println(timeClient.getFormattedTime());

  // Configure WiFiClientSecure to use the AWS IoT device credentials
  net.setCACert(AWS_CERT_CA);
  net.setCertificate(AWS_CERT_CRT);
  net.setPrivateKey(AWS_CERT_PRIVATE);

  // Connect to the MQTT broker on the AWS endpoint we defined earlier
  client.begin(AWS_IOT_ENDPOINT, 8883, net);

  String thingName = "sensor-" + sensorId;
  Serial.println("Connecting to AWS IOT thing name: ");
  Serial.println(thingName.c_str());

  while (!client.connect(thingName.c_str(), false)) {
    delay(100);
  }

  if(!client.connected()){
    Serial.println("AWS IoT Timeout!");
    return;
  }
}

void loop() {
  // Get and print UTC time
  Serial.print("Current UTC time: ");
  // Get UTC time as timestamp
  unsigned long utcTime = timeClient.getEpochTime();
   // Format the UTC time as a string
  char formattedTime[30];
  snprintf(formattedTime, sizeof(formattedTime), "%04d-%02d-%02dT%02d:%02d:%02dZ",
           year(utcTime), month(utcTime), day(utcTime),
           hour(utcTime), minute(utcTime), second(utcTime));
  Serial.println(formattedTime);

  float temperatureCelsius = sht31.readTemperature();
  float relativeHumidity = sht31.readHumidity();

  Serial.print("Temperature (C): ");
  Serial.println(temperatureCelsius);
  Serial.print("Relative Humidity (%RH): ");
  Serial.println(relativeHumidity);

  String sensorId = retrieveStringVariable("/data.txt");

  StaticJsonDocument<200> doc;
  JsonObject state = doc.createNestedObject("state");
  JsonObject reported = state.createNestedObject("reported");

  reported["sensorId"] = sensorId;
  reported["time"] = formattedTime;
  reported["temperature"] = temperatureCelsius;
  reported["relativeHumidity"] = relativeHumidity;
  char jsonBuffer[512];
  serializeJson(doc, jsonBuffer);

  String thingName = "sensor-" + sensorId;
  String publishTopic = "$aws/things/" + thingName + "/shadow/name/readings/update";

  client.publish(publishTopic, jsonBuffer);

  client.loop();

  delay(10*1000);
}
