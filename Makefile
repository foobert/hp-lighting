.PHONY: upload compile

compile:
	arduino-cli compile --fqbn esp8266:esp8266:d1_mini .

upload:
	arduino-cli upload --fqbn esp8266:esp8266:d1_mini --port /dev/ttyUSB0 .
