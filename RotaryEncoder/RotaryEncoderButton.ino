/*
 R. J. Tidey 2017/02/22
 Rotary Encoder Button - battery based
 Web software update service included
 WifiManager can be used to config wifi network
 
 */
#define ESP8266
#include "BaseConfig.h"
#include <ESP8266HTTPClient.h>
#include <WiFiUdp.h>
#include <ESP8266mDNS.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <RotaryEncoderArray.h>

int timeInterval = 10;
unsigned long noChangeTimeout = 30000;
unsigned long elapsedTime;
unsigned long lastChangeTime;

#define POWER_HOLD_PIN 13
#define SLEEP_MASK 12
#define dfltROTARY_PIN1 5
#define dfltROTARY_PIN2 4

//MQTT comment out MQTT define to disable
#define MQTT
#define MQTT_RETRIES 5
#ifdef MQTT
	WiFiClient mClient;
	PubSubClient mqttClient(mClient);
#endif

HTTPClient cClient;
WiFiUDP Udp;

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

char GETServerUser[32] = "user";
char GETServerPassword[32] = "password";
String actionString;


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
  load config
*/
void loadConfig() {
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
			Serial.println("Retrying ");
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
				makeActionString(encoder);
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
void setupStart() {
	if(POWER_HOLD_PIN >= 0) {
		digitalWrite(POWER_HOLD_PIN, 0);
		pinMode(POWER_HOLD_PIN, OUTPUT);
	}
	#ifdef MQTT
		mqttClient.setServer(mqtt_server, mqtt_port);
	#endif
}

void extraHandlers() {
	server.on("/status", rotaryStatus);
	server.on("/setPosition", webSetRotaryPosition);
}

void setupEnd() {
	rotaryEncoderInit(1);
	loadConfig();
	managePositions(1);
	if(SLEEP_MASK >= 0) {
		pinMode(SLEEP_MASK, INPUT_PULLUP);
		sleepMask = digitalRead(SLEEP_MASK);
	}
	Serial.println("sleepMask:" + String(sleepMask));
	lastChangeTime = millis();
}

/*
  Main loop
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
