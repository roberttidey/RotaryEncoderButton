/*
 R. J. Tidey 2017/02/22
 Rotary Encoder Button - battery based
 Web software update service included
 WifiManager can be used to config wifi network
 
 */
#define ESP8266

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DNSServer.h>
#include <WiFiManager.h>
#include <RotaryEncoderArray.h>
#include "FS.h"

//put -1 s at end
int unusedPins[11] = {0,2,12,15,16,-1,-1,-1,-1,-1,-1};

/*
Wifi Manager Web set up
If WM_NAME defined then use WebManager
*/
#define WM_NAME "RotaryEncoderSetup"
#define WM_PASSWORD "password"
#ifdef WM_NAME
	WiFiManager wifiManager;
#endif
//uncomment to use a static IP
//#define WM_STATIC_IP 192,168,0,100
//#define WM_STATIC_GATEWAY 192,168,0,1

int timeInterval = 10;
#define WIFI_CHECK_TIMEOUT 30000
unsigned long noChangeTimeout = 30000;
unsigned long elapsedTime;
unsigned long wifiCheckTime;
unsigned long lastChangeTime;

#define POWER_HOLD_PIN 13
#define SLEEP_MASK 12
#define dfltROTARY_PIN1 5
#define dfltROTARY_PIN2 4

#define AP_AUTHID "12345678"

//For update service
String host = "esp8266-rotaryEncoder";
const char* update_path = "/firmware";
const char* update_username = "admin";
const char* update_password = "password";

//MQTT comment out MQTT define to disable
#define MQTT
#define mqtt_server "192.168.0.100"
#define mqtt_port 1883
#define mqtt_user "homeassistant"
#define mqtt_password "password"
#define MQTT_RETRIES 5
#ifdef MQTT
	WiFiClient mClient;
	PubSubClient mqttClient(mClient);
#endif

//AP definitions
#define AP_SSID "ssid"
#define AP_PASSWORD "password"
#define AP_MAX_WAIT 10
String macAddr;

#define AP_PORT 80

ESP8266WebServer server(AP_PORT);
ESP8266HTTPUpdateServer httpUpdater;
HTTPClient cClient;
WiFiUDP Udp;

#define CONFIG_RETRIES 10
#define CONFIG_FILE "/rotaryEncoderConfig.txt"
#define POSITIONS_FILE "/rotaryEncoderPositions.txt"
// create a file of this name to do a 1 shot link pair operation
#define LWRF_INIT "/linkInit.txt"
#define ACTION_NULL 0
#define ACTION_GETURL 1
#define ACTION_UDP 2
#define ACTION_MQTT 3
#define ACTION_LWRF 4

int sleepMask = 1;

// set ups and actions for encoders
int rotaryPosition[MAX_ENCODERS];
int lastRotaryPosition[MAX_ENCODERS];
int rotaryDirection[MAX_ENCODERS];
int changeAction[MAX_ENCODERS];
int actionInterval[MAX_ENCODERS];
int changeCount[MAX_ENCODERS];
int buttonPulse[MAX_ENCODERS];
int lightwaveToggle[MAX_ENCODERS];
unsigned long actionTime[MAX_ENCODERS];
String changeActionStrings[MAX_ENCODERS];
String changePar1[MAX_ENCODERS];
String changePar2[MAX_ENCODERS];
String changePar3[MAX_ENCODERS];
String changePar4[MAX_ENCODERS];
String changePar5[MAX_ENCODERS];
char tmpString[32];

char GETServerUser[] = {"username"};
char GETServerPassword[] = {"password"};
String actionString;

//holds the current upload
File fsUploadFile;

void ICACHE_RAM_ATTR  delaymSec(unsigned long mSec) {
	unsigned long ms = mSec;
	while(ms > 100) {
		delay(100);
		ms -= 100;
		ESP.wdtFeed();
	}
	delay(ms);
	ESP.wdtFeed();
	yield();
}

void ICACHE_RAM_ATTR  delayuSec(unsigned long uSec) {
	unsigned long us = uSec;
	while(us > 100000) {
		delay(100);
		us -= 100000;
		ESP.wdtFeed();
	}
	delayMicroseconds(us);
	ESP.wdtFeed();
	yield();
}

void unusedIO() {
	int i;
	
	for(i=0;i<11;i++) {
		if(unusedPins[i] < 0) {
			break;
		} else if(unusedPins[i] != 16) {
			pinMode(unusedPins[i],INPUT_PULLUP);
		} else {
			pinMode(16,INPUT_PULLDOWN_16);
		}
	}
}

void initFS() {
	if(!SPIFFS.begin()) {
		Serial.println(F("No SIFFS found. Format it"));
		if(SPIFFS.format()) {
			SPIFFS.begin();
		} else {
			Serial.println(F("No SIFFS found. Format it"));
		}
	} else {
		Serial.println(F("SPIFFS file list"));
		Dir dir = SPIFFS.openDir("/");
		while (dir.next()) {
			Serial.print(dir.fileName());
			Serial.print(F(" - "));
			Serial.println(dir.fileSize());
		}
	}
}

String getContentType(String filename){
  if(server.hasArg("download")) return "application/octet-stream";
  else if(filename.endsWith(".htm")) return "text/html";
  else if(filename.endsWith(".html")) return "text/html";
  else if(filename.endsWith(".css")) return "text/css";
  else if(filename.endsWith(".js")) return "application/javascript";
  else if(filename.endsWith(".png")) return "image/png";
  else if(filename.endsWith(".gif")) return "image/gif";
  else if(filename.endsWith(".jpg")) return "image/jpeg";
  else if(filename.endsWith(".ico")) return "image/x-icon";
  else if(filename.endsWith(".xml")) return "text/xml";
  else if(filename.endsWith(".pdf")) return "application/x-pdf";
  else if(filename.endsWith(".zip")) return "application/x-zip";
  else if(filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

bool handleFileRead(String path){
  Serial.printf_P(PSTR("handleFileRead: %s\r\n"), path.c_str());
  if(path.endsWith("/")) path += "index.htm";
  String contentType = getContentType(path);
  String pathWithGz = path + ".gz";
  if(SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)){
    if(SPIFFS.exists(pathWithGz))
      path += ".gz";
    File file = SPIFFS.open(path, "r");
    size_t sent = server.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
}

void handleFileUpload(){
  if(server.uri() != "/edit") return;
  HTTPUpload& upload = server.upload();
  if(upload.status == UPLOAD_FILE_START){
    String filename = upload.filename;
    if(!filename.startsWith("/")) filename = "/"+filename;
    Serial.printf_P(PSTR("handleFileUpload Name: %s\r\n"), filename.c_str());
    fsUploadFile = SPIFFS.open(filename, "w");
    filename = String();
  } else if(upload.status == UPLOAD_FILE_WRITE){
    Serial.printf_P(PSTR("handleFileUpload Data: %d\r\n"), upload.currentSize);
    if(fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize);
  } else if(upload.status == UPLOAD_FILE_END){
    if(fsUploadFile)
      fsUploadFile.close();
    Serial.printf_P(PSTR("handleFileUpload Size: %d\r\n"), upload.totalSize);
  }
}

void handleFileDelete(){
  if(server.args() == 0) return server.send(500, "text/plain", "BAD ARGS");
  String path = server.arg(0);
  Serial.printf_P(PSTR("handleFileDelete: %s\r\n"),path.c_str());
  if(path == "/")
    return server.send(500, "text/plain", "BAD PATH");
  if(!SPIFFS.exists(path))
    return server.send(404, "text/plain", "FileNotFound");
  SPIFFS.remove(path);
  server.send(200, "text/plain", "");
  path = String();
}

void handleFileCreate(){
  if(server.args() == 0)
    return server.send(500, "text/plain", "BAD ARGS");
  String path = server.arg(0);
  Serial.printf_P(PSTR("handleFileCreate: %s\r\n"),path.c_str());
  if(path == "/")
    return server.send(500, "text/plain", "BAD PATH");
  if(SPIFFS.exists(path))
    return server.send(500, "text/plain", "FILE EXISTS");
  File file = SPIFFS.open(path, "w");
  if(file)
    file.close();
  else
    return server.send(500, "text/plain", "CREATE FAILED");
  server.send(200, "text/plain", "");
  path = String();
}

void handleFileList() {
  if(!server.hasArg("dir")) {server.send(500, "text/plain", "BAD ARGS"); return;}
  
  String path = server.arg("dir");
  Serial.printf_P(PSTR("handleFileList: %s\r\n"),path.c_str());
  Dir dir = SPIFFS.openDir(path);
  path = String();

  String output = "[";
  while(dir.next()){
    File entry = dir.openFile("r");
    if (output != "[") output += ',';
    bool isDir = false;
    output += "{\"type\":\"";
    output += (isDir)?"dir":"file";
    output += "\",\"name\":\"";
    output += String(entry.name()).substring(1);
    output += "\"}";
    entry.close();
  }
  output += "]";
  server.send(200, "text/json", output);
}

void handleMinimalUpload() {
  char temp[700];

  snprintf ( temp, 700,
    "<!DOCTYPE html>\
    <html>\
      <head>\
        <title>ESP8266 Upload</title>\
        <meta charset=\"utf-8\">\
        <meta http-equiv=\"X-UA-Compatible\" content=\"IE=edge\">\
        <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\
      </head>\
      <body>\
        <form action=\"/edit\" method=\"post\" enctype=\"multipart/form-data\">\
          <input type=\"file\" name=\"data\">\
          <input type=\"text\" name=\"path\" value=\"/\">\
          <button>Upload</button>\
         </form>\
      </body>\
    </html>"
  );
  server.send ( 200, "text/html", temp );
}

void handleSpiffsFormat() {
	SPIFFS.format();
	server.send(200, "text/json", "format complete");
}

void addEncoder(int encoder, String encoderPins) {
	int parNum[6];
	int i;
	int lastIndex = 0;
	int index = 0;
	String par;
	for(i=0; i<14; i++) {
		index = encoderPins.indexOf(',', lastIndex);
		if(index > 0) {
			par = encoderPins.substring(lastIndex, index);
			lastIndex = index+1;
		}
		else {
			par = encoderPins.substring(lastIndex);
			if(i < 5) return;
		}
		switch(i) {
			case 0: parNum[0] = par.toInt(); break;
			case 1: parNum[1] = par.toInt(); break;
			case 2: parNum[2] = par.toInt(); break;
			case 3: parNum[3] = par.toInt(); break;
			case 4: parNum[4] = par.toInt(); break;
			case 5: parNum[5] = par.toInt(); break;
			case 6: changeAction[encoder] = par.toInt(); break;
			case 7: actionInterval[encoder] = par.toInt(); break;
			case 8: changeActionStrings[encoder] = par; break;
			case 9: changePar1[encoder] = par; break;
			case 10: changePar2[encoder] = par; break;
			case 11: changePar3[encoder] = par; break;
			case 12: changePar4[encoder] = par; break;
			case 13: changePar5[encoder] = par; break;
		}
	}
	if(parNum[0] >= 0) {
		setRotaryEncoderPins(encoder, parNum[0], parNum[1], parNum[2]);
		setRotaryLimits(encoder, parNum[3], parNum[4]);
		setRotaryPosition(encoder, parNum[5]);
		lastRotaryPosition[encoder] = parNum[5];
		actionTime[encoder] = 0;
		changeCount[encoder] = 0;
		buttonPulse[encoder] = 0;
		lightwaveToggle[encoder] = 0;
	}
}

/*
  Get config
*/
void getConfig() {
	String line = "";
	int config = 0;
	File f = SPIFFS.open(CONFIG_FILE, "r");
	if(f) {
		while(f.available()) {
			line =f.readStringUntil('\n');
			line.replace("\r","");
			if(line.length() > 0 && line.charAt(0) != '#') {
				switch(config) {
					case 0: host = line;break;
					case 1: noChangeTimeout = line.toInt();break;
					case 2: addEncoder(0, line); break;
					case 3: addEncoder(1, line); break;
					case 4: addEncoder(2, line);
						Serial.println(F("Config loaded from file OK"));
						break;
				}
				config++;
			}
		}
		f.close();
		//enforce minimum no change of 10 seconds
		if(noChangeTimeout < 10000) noChangeTimeout = 10000;
		Serial.println("Config loaded");
		Serial.print(F("host:"));Serial.println(host);
		Serial.print(F("noChangeTimeout:"));Serial.println(noChangeTimeout);
	} else {
		Serial.println(String(CONFIG_FILE) + " not found. Use default encoder");
		setRotaryEncoderPins(0, dfltROTARY_PIN1, dfltROTARY_PIN2, -1);
	}
}

void rotaryStatus() {
	String response;
	int i;
	
	response = "<BR>elapsedTime:" + String(elapsedTime);
	response += "<BR>sleepMask:" + String(sleepMask);
	for(i=0; i<MAX_ENCODERS ;i++) {
		if(getEncoderPin1(i) >= 0) {
			response += "<BR>rotaryPosition:" + String(i) + "=" + String(rotaryPosition[i]);
			response += "<BR>rotaryDirection:" + String(i) + "=" + String(rotaryDirection[i]);
			response += "<BR>changeCount:" + String(i) + "=" + String(changeCount[i]);
		}
	}
	response += "<BR>actionString:" + actionString;
	response += "<BR>";
	server.send(200, "text/html", response);
}

/*
  Connect to local wifi with retries
  If check is set then test the connection and re-establish if timed out
*/
int wifiConnect(int check) {
	if(check) {
		if(WiFi.status() != WL_CONNECTED) {
			if((elapsedTime - wifiCheckTime) * timeInterval > WIFI_CHECK_TIMEOUT) {
				Serial.println("Wifi connection timed out. Try to relink");
			} else {
				return 1;
			}
		} else {
			wifiCheckTime = elapsedTime;
			return 0;
		}
	}
	wifiCheckTime = elapsedTime;
#ifdef WM_NAME
	Serial.println("Set up managed Web");
#ifdef WM_STATIC_IP
	wifiManager.setSTAStaticIPConfig(IPAddress(WM_STATIC_IP), IPAddress(WM_STATIC_GATEWAY), IPAddress(255,255,255,0));
#endif
	if(check == 0) {
		wifiManager.setConfigPortalTimeout(180);
		wifiManager.autoConnect(WM_NAME, WM_PASSWORD);
	} else {
		WiFi.begin();
	}
#else
	Serial.println("Set up manual Web");
	int retries = 0;
	Serial.print("Connecting to AP");
	#ifdef AP_IP
		IPAddress addr1(AP_IP);
		IPAddress addr2(AP_DNS);
		IPAddress addr3(AP_GATEWAY);
		IPAddress addr4(AP_SUBNET);
		WiFi.config(addr1, addr2, addr3, addr4);
	#endif
	WiFi.begin(AP_SSID, AP_PASSWORD);
	while (WiFi.status() != WL_CONNECTED && retries < AP_MAX_WAIT) {
		delaymSec(1000);
		Serial.print(".");
		retries++;
	}
	Serial.println("");
	if(retries < AP_MAX_WAIT) {
		Serial.print("WiFi connected ip ");
		Serial.print(WiFi.localIP());
		Serial.printf(":%d mac %s\r\n", AP_PORT, WiFi.macAddress().c_str());
		return 1;
	} else {
		Serial.println("WiFi connection attempt failed"); 
		return 0;
	} 
#endif
}

/*
  Establish MQTT connection for publishing to Home assistant
*/
#ifdef MQTT
void mqttConnect() {
	// Loop until we're reconnected
	int retries = 0;
	while (!mqttClient.connected()) {
		Serial.print("Attempting MQTT connection...");
		// Attempt to connect
		// If you do not want to use a username and password, change next line to
		// if (mqttClient.connect("ESP8266mqttClient")) {
		if (mqttClient.connect("ESP8266Client", mqtt_user, mqtt_password)) {
			Serial.println("connected");
		} else {
			Serial.print("failed, rc=");
			Serial.print(mqttClient.state());
			delay(5000);
			retries++;
			if(retries > MQTT_RETRIES) {
				wifiConnect(1);
				retries = 0;
			}
		}
	}
}
#endif

/*
Make lightave insert string
*/
String makeLightwaveFunction(int encoder) {
	if(SPIFFS.exists(LWRF_INIT)) {
		//use link pairing command
		SPIFFS.remove(LWRF_INIT);
		return "Fp*";
	}
	if(buttonPulse[encoder] == 1) {
		lightwaveToggle[encoder] = (lightwaveToggle[encoder] + 1) & 1;
		return changePar3[encoder] + "F" + String(lightwaveToggle[encoder]);
	}
	if(rotaryPosition[encoder] == 0) {
		return changePar3[encoder] + "F0";
	}
	return changePar3[encoder] + "FdP" + String(rotaryPosition[encoder]);
}

/*
Make action string by substituting parameters
*/
void makeActionString(int encoder) {
	int index;
	int sub;
	int changed;
	String subs = "$e$p$d$b$l$x$y$z$u$v$t$c";
	String insert;
	actionString = changeActionStrings[encoder];
	while(true) {
		changed = 0;
		for(sub = 0; sub < subs.length() - 1; sub +=2) {
			index = actionString.indexOf(subs.substring(sub, sub + 2));
			if(index >= 0) {
				changed = 1;
				switch(sub) {
					case 0: insert = String(encoder); break; //$e
					case 2: insert = String(rotaryPosition[encoder]); break; //$p
					case 4: insert = String(rotaryDirection[encoder]); break; //$d
					case 6: insert = String(buttonPulse[encoder]); break; //$b
					case 8: insert = makeLightwaveFunction(encoder); break; //$l
					case 10: insert = changePar1[encoder]; break; //$x
					case 12: insert = changePar2[encoder]; break; //$y
					case 14: insert = changePar3[encoder]; break; //$z
					case 16: insert = changePar4[encoder]; break; //$u
					case 18: insert = changePar5[encoder]; break; //$v
					case 20: insert = String(changeCount[encoder]); break; //$t
					case 22: insert = ","; break; //$c
				}
				if(changed) {
					actionString = actionString.substring(0, index) + insert + actionString.substring(index+2);
				} else {
					break;
				}
			}
		}
		if(!changed) break;
	}
}

/*
 Access a URL
*/
void getFromURL(String url, int retryCount, char* user, char* password) {
	int retries = retryCount;
	int responseOK = 0;
	int httpCode;
	
	Serial.println("get from " + url);
	
	while(retries > 0) {
		if(user) cClient.setAuthorization(user, password);
		cClient.begin(url);
		httpCode = cClient.GET();
		if (httpCode > 0) {
			if (httpCode == HTTP_CODE_OK) {
				String payload = cClient.getString();
				Serial.println(payload);
				responseOK = 1;
			}
		} else {
			Serial.printf("[HTTP] POST... failed, error: %s\n", cClient.errorToString(httpCode).c_str());
		}
		cClient.end();
		if(responseOK)
			break;
		else
			Serial.println("Retrying EIOT report");
		retries--;
	}
	Serial.println();
	Serial.println("Connection closed");
}

/*
  positions
*/
void managePositions(int readP) {
	File f = SPIFFS.open(POSITIONS_FILE, "r");
	int i;
	if(readP){
		if(f) {
			for(i = 0; i< MAX_ENCODERS; i++) {
				lastRotaryPosition[i] = strtoul(f.readStringUntil('\n').c_str(), NULL, 10);
				setRotaryPosition(i, lastRotaryPosition[i]);
			}
			f.close();
		}
	} else {
		f = SPIFFS.open(POSITIONS_FILE, "w");
		for(i = 0; i< MAX_ENCODERS; i++) {
			f.print(String(lastRotaryPosition[i]) + "\n");
		}
		f.close();
	}
}

/*
 Action encoder change
*/
void encoderChange(int encoder) {
	switch(changeAction[encoder]) {
		case ACTION_NULL: //do nothing but print action string
			makeActionString(encoder);
			Serial.println(actionString);
			break;
		case ACTION_GETURL:
			makeActionString(encoder);
			getFromURL(actionString, 10, GETServerUser, GETServerPassword);
			break;
		case ACTION_UDP:
			int udpPort;
			udpPort = changePar2[encoder].toInt();
			strncpy(tmpString, changePar1[encoder].c_str(), 32);
			Udp.beginPacket(tmpString, udpPort);
			makeActionString(encoder);
			Udp.write(actionString.c_str(), actionString.length());
			Udp.endPacket();
			break;
		case ACTION_MQTT:
			#ifdef MQTT
				if (!mqttClient.connected()) {
					mqttConnect();
				}
				mqttClient.loop();
				strncpy(tmpString, changePar1[encoder].c_str(), 32);
				mqttClient.publish(tmpString, actionString.c_str(), true);
			#endif
			break;
	}
	actionTime[encoder] = millis();
	changeCount[encoder]++;
}

void webSetRotaryPosition() {
	int encoder;
	int position;
	if (server.arg("auth") != AP_AUTHID) {
		Serial.println("Unauthorized");
		server.send(401, "text/html", "Unauthorized");
	} else {
		encoder = server.arg("encoder").toInt();
		position = server.arg("position").toInt();
		if(encoder >=0 && encoder < MAX_ENCODERS) {
			setRotaryPosition(encoder, position);
			server.send(200, "text/html", "Encoder position set to " + String(position));
		} else {
			server.send(401, "text/html", "Bad encoder number");
		}
	}
}

/*
  Set up basic wifi, collect config from flash/server, initiate update server
*/
void setup() {
	unusedIO();
	Serial.begin(115200);
	if(POWER_HOLD_PIN >= 0) {
		digitalWrite(POWER_HOLD_PIN, 0);
		pinMode(POWER_HOLD_PIN, OUTPUT);
	}
	Serial.println("Set up Web update service");
	wifiConnect(0);
	macAddr = WiFi.macAddress();
	macAddr.replace(":","");
	Serial.println(macAddr);
	initFS();

	//Update service
	MDNS.begin(host.c_str());
	httpUpdater.setup(&server, update_path, update_username, update_password);
	Serial.println(F("Set up web server"));
	//Simple upload
	server.on("/upload", handleMinimalUpload);
	server.on("/format", handleSpiffsFormat);
	server.on("/list", HTTP_GET, handleFileList);
	//load editor
	server.on("/edit", HTTP_GET, [](){
	if(!handleFileRead("/edit.htm")) server.send(404, "text/plain", "FileNotFound");});
	//create file
	server.on("/edit", HTTP_PUT, handleFileCreate);
	//delete file
	server.on("/edit", HTTP_DELETE, handleFileDelete);
	//first callback is called after the request has ended with all parsed arguments
	//second callback handles file uploads at that location
	server.on("/edit", HTTP_POST, [](){ server.send(200, "text/plain", ""); }, handleFileUpload);
	//called when the url is not defined here
	//use it to load content from SPIFFS
	server.onNotFound([](){if(!handleFileRead(server.uri())) server.send(404, "text/plain", "FileNotFound");});
	server.on("/status", rotaryStatus);
	server.on("/setPosition", webSetRotaryPosition);
	server.begin();
	#ifdef MQTT
		mqttClient.setServer(mqtt_server, mqtt_port);
	#endif
	MDNS.addService("http", "tcp", 80);
	if(SLEEP_MASK >= 0) {
		pinMode(SLEEP_MASK, INPUT_PULLUP);
		sleepMask = digitalRead(SLEEP_MASK);
	}
	rotaryEncoderInit(1);
	getConfig();
	managePositions(1);
	lastChangeTime = millis();
	Serial.println("Set up complete");
}


/*
  Main loop to publish PIR as required
*/
int encoder;
int changed;
void loop() {
	if((sleepMask == 1) && (millis() > (lastChangeTime  + noChangeTimeout))) {
		WiFi.mode(WIFI_OFF);
		delaymSec(10);
		WiFi.forceSleepBegin();
		delaymSec(1000);
		if(POWER_HOLD_PIN >=0) pinMode(POWER_HOLD_PIN, INPUT);
		ESP.deepSleep(0);
	}
	changed =0;
	for(encoder = 0; encoder < MAX_ENCODERS; encoder++) {
		if(getEncoderPin1(encoder) >= 0) {
			rotaryPosition[encoder] = getRotaryPosition(encoder);
			buttonPulse[encoder] = getRotaryButtonPulse(encoder);
			if((lastRotaryPosition[encoder] != rotaryPosition[encoder]) || (buttonPulse[encoder] > 0)) {
				if((millis() > (actionTime[encoder] + actionInterval[encoder])) || actionTime[encoder] == 0) {
					rotaryDirection[encoder] = getRotaryDirection(encoder);
					Serial.println("Rotary position = " + String(rotaryPosition[encoder]));
					Serial.println("Rotary direction = " + String(rotaryDirection[encoder]));
					Serial.println("Button pulse = " + String(buttonPulse[encoder]));
					lastRotaryPosition[encoder] = rotaryPosition[encoder];
					encoderChange(encoder);
					lastChangeTime = millis();
					changed = 1;
				}
			}
		}
	}
	if(changed) managePositions(0);
	delaymSec(timeInterval);
	elapsedTime++;
	server.handleClient();
	wifiConnect(1);
}
