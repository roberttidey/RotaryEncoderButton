# Battery rotary encoder action button for ESP8266

See schematic for hook up.
Instructable TBA

## Features
- Rotary encoder actions web/mqtt/lightwaverf
- Push button supports on/off toggle
- WifiManager for initial set up
- OTA for updates
- File browser for maintenance

## Basic Web interface (basic.htm)
### Commands are sent to ip/
- status
- upload
- edit
- firmware
- setPosition?auth=authNo&encoder=encoderIndex&position=encoderPosition

### Config
- Edit dfPlayer.ino
	- Manual Wifi set up (Comment out WM_NAME)
		- AP_SSID Local network ssid
		- AP_PASSWORD 
		- AP_IP If static IP to be used
	- Wifi Manager set up
		- WM_NAME (ssid of wifi manager network)
		- WM_PASSWORD (password to connect)
		- On start up connect to WM_NAME network and browse to 192.168.4.1 to set up wifi
	- AP_PORT to access web services default 80
	- update_username user for updating firmware
	- update_password
- Edit rotaryEncoderConfig.txt
	
### Libraries
- ESP8266WiFi
- ESP8266HTTPClient
- ESP8266WebServer
- WiFiUdp
- ESP8266mDNS
- ESP8266HTTPUpdateServer
- PubSubClient
- ArduinoJson // not currently used
- DNSServer
- WiFiManager
- RotaryEncoderArray // handles up to 3 encoders
- FS

### Install procedure
- Normal arduino esp8266 compile and upload
- A simple built in file uploader (/upload) should then be used to upload the base datafiles to SPIFF
  edit.htm.gz
  index.html
  favicon*.png
  graphs.js.gz
  rotaryEncoderConfig.txt
- The /edit operation is then supported to update further
	
### Lightwaverf supported
- Uses UDP to send lighting controls to Lightwaverf link
- ESP8266 Must be paired with link first.
	- Place a file called initLink.txt in filing system.
	- Rotate control to send a pairing command
	- Acknowledge on Lightwaverf link
	- initLink.txt is automatically deleted
	- repeat if necessary
