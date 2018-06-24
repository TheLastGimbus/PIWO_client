#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

//////
int deviceID = 0;
//////

WiFiClient client;
IPAddress localIP = IPAddress(192, 168, 1, (201 + deviceID));
IPAddress serverIP = IPAddress(192, 168, 1, 200);
int serverPort = 3000;

const char* wifiName = "Broadcom";
const char* wifiPass = "";

unsigned long long lastMessage = 0;

ADC_MODE(ADC_VCC);

#define rPin 16
#define gPin 14
#define bPin 12
#define wPin 13

int red = 0;
int green = 0;
int blue = 0;
int white = 0;

void connectToWifi(const char* name, const char* password){
	WiFi.mode(WIFI_STA);
	WiFi.begin(name, password);
	unsigned long long waiting = millis();
	while (!WiFi.isConnected()) {
		digitalWrite(2, 0);
		delay(500);
		digitalWrite(2, 1);
		delay(500);
		Serial.println("Connecting to WiFi...");
		if((millis() - waiting) > 180000){
			Serial.println("I've waited too long for WiFi! Restarting...");
			delay(1000);
			ESP.restart();
		}
	}
	Serial.println("Connected to wifi!");
	WiFi.config(localIP, WiFi.gatewayIP(), WiFi.subnetMask());
}

void connectToServer(IPAddress ip, int port){
	while(!client.connect(ip, port)){
		digitalWrite(2, 0);
		delay(750);
		digitalWrite(2, 1);
		Serial.println("Connecting to server...");
		if(!WiFi.isConnected()){
			connectToWifi(wifiName, wifiPass);
		}
		ArduinoOTA.handle();
	}
	Serial.println("Connected to server!");
}

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

void serialCommand(String com){
	if(com.indexOf("status") == 0){
		Serial.println("=====================");
		Serial.println("DeviceID (client): " + String(deviceID));
		Serial.println("Vcc: " + String(((float)ESP.getVcc()) / 1024));
		Serial.println("WiFi name: " + String(wifiName));
		Serial.println("WiFi pass: " + String(wifiPass));
		Serial.println("Local IP: " + WiFi.localIP().toString());
		Serial.println("Server IP: " + serverIP.toString());
		Serial.println("Server port: " + String(serverPort));
		Serial.println("Connected to server: " + String(client.connected()));
		Serial.println("Red: " + String(red));
		Serial.println("Green: " + String(green));
		Serial.println("Blue: " + String(blue));
		Serial.println("White: " + String(white));
		Serial.println("=====================");
	}
	if(com.indexOf("#") == 0){
		setLed(com);
	}
}

void clientCommand(String com){
	if(com.indexOf("OK?") == 0){
		client.print("OK~");
	}
	if(com.indexOf("VOL?") == 0){
		float vccVolt = ((float)ESP.getVcc()) / 1024;
		client.print(String(vccVolt) + "~");
	}
	if(com.indexOf("#") == 0){
		setLed(com);
	}
}

void setup() {
	Serial.begin(115200);
	pinMode(2, OUTPUT);
	pinMode(rPin, OUTPUT);
	pinMode(gPin, OUTPUT);
	pinMode(bPin, OUTPUT);
	pinMode(wPin, OUTPUT);
	digitalWrite(2, 1);

	connectToWifi(wifiName, wifiPass);
	connectToServer(serverIP, serverPort);
	lastMessage = millis();

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
}

void loop() {
	if(!WiFi.isConnected()){
		connectToWifi(wifiName, wifiPass);
	}
	if(!client.connected()){
		connectToServer(serverIP, serverPort);
	}

	ArduinoOTA.handle();

	while(Serial.available()){
		serialCommand(Serial.readStringUntil('\n'));
	}

	while(client.available()){
		clientCommand(client.readStringUntil('~'));
		lastMessage = millis();
	}

	if((millis() - lastMessage) > 10000){
		lastMessage = millis();
		if(!okRequest()){
			client.stop();
			client.stopAll();
			connectToServer(serverIP, serverPort);
		}
	}
}
