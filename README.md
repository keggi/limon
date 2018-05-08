# limon

This project is a quick example of how to
* Connect a capacitive soil moisture sensor to an Arduino Uno
* Sensor used here is from DFR Robotics https://www.dfrobot.com/wiki/index.php/Capacitive_Soil_Moisture_Sensor_SKU:SEN0193
* Basic HTTP stack on the Arduino with minimal ability to encode/decode JSON
* Ability to register with the server and save tokens to EEPROM
* Simple MYSQL backed PHP API to register devices, issue them tokens to save to EEPROM
* Basic JS charting to show timeseries of moisture levels

Device Registration
* Boot up ID from EEPROM
* Send up the hard coded secret w/ ID and request an access token
* Server generates a token, or registers the device with its own ID if its new or invalid ID
* Device saves that token to RAM and uses it for subsequent requests until power-off
