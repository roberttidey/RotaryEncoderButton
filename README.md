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
- Uses BaseSupport library at https://github.com/roberttidey/BaseSupport
- Edit passwords etc in BaseConfig.h
- Uncomment FASTCONNECT in BaseConfig as required
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
