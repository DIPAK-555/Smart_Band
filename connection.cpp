#include <HardwareSerial.h>
#include <WiFi.h>
#include <TinyGPS++.h>

//======================= PIN DEFINITIONS =======================//
#define GSM_RX 27  // A7670C RX
#define GSM_TX 26  // A7670C TX
#define GPS_RX 16  // GPS TX to ESP32 RX2
#define GPS_TX 17  // GPS RX to ESP32 TX2

//======================= WIFI CREDENTIALS =======================//
const char* ssid = "IT1";         // Replace with your WiFi name
const char* password = "it1@12345";  // Replace with your WiFi password

//======================= GLOBAL VARIABLES =======================//
HardwareSerial GSM(1);
HardwareSerial GPS(2);
TinyGPSPlus gps;

String fromGSM = "";
bool CALL_END = true;
String msg = "";
String SOS_NUM = "+919609658627";  // 

//======================= SETUP =======================//
void setup() {
  Serial.begin(115200);

  // Initialize serial communications
  GSM.begin(115200, SERIAL_8N1, GSM_RX, GSM_TX);
  GPS.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);

  // Connect to WiFi
  connectToWiFi();

  // Initialize GSM module
  delay(5000);  // Increase delay to allow more time for initialization
  initializeGSM();
  sendReadyMessage();
}

//======================= MAIN LOOP =======================//
void loop() {
  // Handle incoming GSM messages
  if (GSM.available()) {
    char inChar = GSM.read();
    if (inChar == '\n') {
      handleGSMCommand(fromGSM);
      fromGSM = "";
    } else {
      fromGSM += inChar;
    }
  }

  // Your additional WiFi-related code can go here
  // Example: check WiFi status periodically
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected! Attempting to reconnect...");
    connectToWiFi();
  }

  // Process GPS data
  while (GPS.available() > 0) {
    gps.encode(GPS.read());
  }
}

//======================= WIFI CONNECTION =======================//
void connectToWiFi() {
  Serial.println();
  Serial.print("Connecting to: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("");
    Serial.println("Failed to connect to WiFi");
  }
}

//======================= GSM FUNCTIONS =======================//
void initializeGSM() {
  Serial.println("Initializing GSM module...");
  if (!sendCommand("AT", "OK", 2000)) {
    Serial.println("signal is low.");
    return;
  }

  sendCommand("AT+CMGF=1", "OK", 2000);       // SMS Text Mode
  sendCommand("AT+CSMP=17,167,0,0", "OK", 2000); // SMS Parameters
  sendCommand("AT+CPMS=\"SM\",\"ME\",\"SM\"", "OK", 2000); // Message Storage
  sendCommand("AT+SNFS=2", "OK", 2000);       // Audio channel
  sendCommand("AT+CLVL=8", "OK", 2000);       // Volume
  Serial.println("GSM module initialized successfully.");
}

void handleGSMCommand(String cmd) {
  cmd.trim();
  cmd.toLowerCase();

  if (cmd.indexOf("+cmgr:") != -1) {
    // Extract the message content from the SMS
    int startIndex = cmd.indexOf("\n", cmd.indexOf("+CMGR:")) + 1;
    int endIndex = cmd.indexOf("\n", startIndex);
    String messageContent = cmd.substring(startIndex, endIndex);
    messageContent.trim();

    if (messageContent == "send location") {
      sendLocation(false);
    } else if (messageContent == "battery?") {
      if (sendCommand("AT+CBC", "OK", 2000)) {
        int start = msg.indexOf("+CBC:");
        int end = msg.indexOf("\n", start);
        if (start != -1 && end != -1) {
          String batteryInfo = msg.substring(start, end);
          sendSMS(batteryInfo);
        } else {
          sendSMS("Failed to retrieve battery info.");
        }
      }
    } else if (messageContent == "ring") {
      GSM.println("ATA");
    }
  } else if (cmd == "no carrier") {
    CALL_END = true;
  }
  Serial.println("GSM: " + cmd);
}

void sendLocation(bool call) {
  if (gps.location.isUpdated()) {
    String lat = String(gps.location.lat(), 6);
    String lng = String(gps.location.lng(), 6);
    String mapLink = "I'm Here http://maps.google.com/maps?q=" + lat + "," + lng;
    sendSMS(mapLink);
    if (call) {
      GSM.println("ATD" + SOS_NUM + ";");
      CALL_END = false;
    }
  } else {
    sendSMS("Unable to fetch location. Please try again.");
  }
}

void sendReadyMessage() {
  sendSMS("System Ready - WiFi Connected: " + String(WiFi.localIP()));
}

void sendSMS(String msgText) {
  GSM.println("AT+CMGS=\"" + SOS_NUM + "\"");
  delay(1000);
  GSM.print(msgText);
  delay(500);
  GSM.write(26);  // Ctrl+Z
  delay(5000);
}

bool sendCommand(String command, String expected, int timeout) {
  GSM.println(command);
  long int time = millis();
  msg = "";

  while ((millis() - time) < timeout) {
    while (GSM.available()) {
      msg += char(GSM.read());
    }
    if (msg.indexOf(expected) != -1) return true;
  }
  Serial.println("Command: " + command);
  Serial.println("Response: " + msg);
  return false;
}
