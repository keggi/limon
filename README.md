# limon

This project is a quick example of how to
* Connect a capacitive soil moisture sensor to an Arduino Uno
* Basic HTTP stack on the Arduino with minimal ability to encode/decode JSON
* Ability to register with the server and get assigned access tokens + ids
* Simple MYSQL backed PHP API to register devices, store water level readings in the database
* Basic JS charting frontend to show timeseries of moisture levels

Sensor used here is from DFR Robotics https://www.dfrobot.com/wiki/index.php/Capacitive_Soil_Moisture_Sensor_SKU:SEN0193

Client is an Arduino Uno https://store.arduino.cc/usa/arduino-uno-rev3 running attached to an ethernet shield https://store.arduino.cc/usa/arduino-ethernet-shield-2

Backend is just some simple procedural PHP + MYSQL running on any old hosting service

Device Registration
* Boot up ID from EEPROM
* POST the ID and hard coded secret using X-Device-Secret header to request an access token
* Server either issues a token for that ID, or registers the device as new with its own ID + token
* Device saves that token to RAM and uses it for subsequent requests until power-off

Sending Soil Readings
* Every 10 minutes the device wakes up and reads the sensor
* Device reads the sensor using an ADC
* POST the value with its ID, a channel number (to support multiple sensors per client), and token via X-Device-Authorization header
* Server saves it in the database

Displaying Data
* Page requires you to specify the device ID and a token (basically equivalent of a password) to access it
* AJAX request return a JSON array of values from the sensor averaged per hour over the last 28 days
* Simple charting library is used to create a chart

The Arduino’s memory is extrememly limited (a single HTTPS certificate is large than all of its memory) so all processing (HTTP, JSON) must be done streaming with small buffers. Typically anything related to parsing or processing string data operates using global-scoped variables to save memory.

HTTP is handled using a simple state machine
* If its been longer than 10m since the last connection, start a new one
* Close pending connections. Build request string + open connection
* Receive headers and check them as they stream in for HTTP 200 OK and other relevant info
* Stream data into a small buffer
* When connection closes, run simple JSON parser (basically counting parenthesis and checking quotes in a stream with a buffer to string compare for the key) through the buffer to extract specific values for keys we care about
* Save the time we finished so we know when to send the next request
* Loop forever

NOTE: you will have to replace ‘yourdomain’ and other placeholders in the configs file with real values.