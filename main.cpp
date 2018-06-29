#include <ESP8266WiFi.h>

//libraries for OTA
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

/// id's are counted like you will read text, left to right then up to down
/// this is where is the module, so when it's first pixel (counting from 0):
int deviceID = 0;
/// it's id is 0
/// id is nothing so important here, it's just used for static ip
/// but it's very important for one device to have one iniqie id
/// so it will be placed correctly in correct window

WiFiClient client;
IPAddress localIP = IPAddress(192, 168, 1, (101 + deviceID));
IPAddress serverIP = IPAddress(192, 168, 1, 100);
int serverPort = 5000;

const char* wifiName = "Broadcom";
const char* wifiPass = "";

// this is used for timing when was last time when we had communcation with server
// so client won't ask is server ok if it just had command from it few seconds ago
unsigned long long lastMessage = 0;

ADC_MODE(ADC_VCC);

#define rPin 16
#define gPin 14
#define bPin 12
#define wPin 13

// global values of current analogWrite output
int red = 0;
int green = 0;
int blue = 0;
int white = 0;

void connectToWifi(){
	WiFi.mode(WIFI_STA);
	WiFi.begin(wifiName, wifiPass);
	unsigned long long waiting = millis();
	while (!WiFi.isConnected()) {
		digitalWrite(2, 0);
		delay(500);
		digitalWrite(2, 1);
		delay(500);
		Serial.println("Connecting to WiFi...");
		if((millis() - waiting) > 180000){ // if it waits >3 min, it restarts
			Serial.println("I've waited too long for WiFi! Restarting...");
			delay(1000);
			ESP.restart();
		}
	}
	Serial.println("Connected to wifi!");
	WiFi.config(localIP, WiFi.gatewayIP(), WiFi.subnetMask()); // static ip
	Serial.println("IP: " + String());
}

void connectToServer(){
	client.stop();
	while (!client.connect(serverIP, serverPort)) {
		// what if you lose wifi while infinite server connection loop?
		if(!WiFi.isConnected()){
			connectToWifi();
		}
		digitalWrite(2, 0);
		delay(1000);
		digitalWrite(2, 1);
		Serial.println("Connecting to server...");
		ArduinoOTA.handle(); // don't let lack of server make OTA not working
	}
	Serial.println("Connected to server!");
	digitalWrite(2, 0);
	delay(500);
	digitalWrite(2, 1);
}

String decToHex(byte decValue, byte desiredStringLength = 2) {
  String hexString = String(decValue, HEX);
  while (hexString.length() < desiredStringLength) hexString = "0" + hexString;

  return hexString;
}

// this is function that returns if server responded in surtent time
// because tcp doesn't detect if server turned of or something
// you need to do it manually
bool okRequest(int timeout = 5000){
	client.print("OK?~");
	for(int x = 0; x < timeout; x++){
		if(client.available()){
			String a = client.readStringUntil('~');
			if(a.indexOf("OK") == 0){
				Serial.println("OK request gone good");
				return 1;
			}
		}
		delay(1);
	}
	Serial.println("OK request gone wrong");
	return 0;
}

void setLed(byte r, byte g, byte b){
	red = r;
	green = g;
	blue = b;
	white = (r+b+g) / 3;

	analogWrite(rPin, red);
	analogWrite(gPin, green);
	analogWrite(bPin, blue);
	analogWrite(wPin, white);

	Serial.println("New colors:");
	Serial.println("Red = " + String(red));
	Serial.println("Green = " + String(green));
	Serial.println("Blue = " + String(blue));
}

void setLed(String hex){
	long long hexCol = strtoll(&hex[1], NULL, 16);
	int r = hexCol >> 16;
	int g = hexCol >> 8 & 0xFF;
	int b = hexCol & 0xFF;
	setLed(r, g, b);
}

// input command, output response
// for example we input "OK?", output is "OK"
// and then we can print output however we want
String command(String com){
	String response = "";
	if(com.indexOf("OK?") == 0){
		response += "OK";
	}
	if(com.indexOf("VOL?") == 0){
		float vccVolt = ((float)ESP.getVcc()) / 1024;
		response += String(vccVolt);
	}
	if(com.indexOf("LED?") == 0){
		response += "#" +
		decToHex(red) +
		decToHex(green) +
		decToHex(blue);
	}
	if(com.indexOf("#") == 0){
		setLed(com);
	}
	return response;
}

void setup() {
	Serial.begin(115200);
	pinMode(2, OUTPUT);
	pinMode(rPin, OUTPUT);
	pinMode(gPin, OUTPUT);
	pinMode(bPin, OUTPUT);
	pinMode(wPin, OUTPUT);
	digitalWrite(2, 1);

	connectToWifi();
	connectToServer();

	// OTA setup. OTA let's you upload code through WiFi,
	// even when ESP is runnign code, and it does that even faster than UART
	ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();

	lastMessage = millis();
}

void loop() {
	if(!WiFi.isConnected()){
		connectToWifi();
	}
	if(!client.connected()){
		connectToServer();
	}

	// All OTA work. That's all. Just don't do delays >3s and it will work.
	ArduinoOTA.handle();

	while(Serial.available()){
		Serial.println(command(Serial.readStringUntil('\n')));
	}

	while(client.available()){
		client.print(command(client.readStringUntil('~')) + "~");
		lastMessage = millis();
	}

	if((millis() - lastMessage) > 10000){
		lastMessage = millis();
		if(!okRequest()){
			client.stop();
			connectToServer();
		}
	}
}
