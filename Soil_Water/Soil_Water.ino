#include <Ethernet.h>
#include <EEPROM.h>

#define DATA_BUFFER_SIZE 100

#define HTTP_200_LEN 15

#define HTTP_IDLE 0
#define HTTP_REQUESTED 1
#define HTTP_RXHEADER 2
#define HTTP_RXDATA 3
#define HTTP_CLOSED 4

#define CONN_MANUAL 0
#define CONN_DHCP 1

#define REQUEST_TOKEN 1
#define SAVE_READING 2

#define EEPROM_DEVICE_ID_OFFSET 0

#define POST_INTERVAL_10S 10L * 1000L
#define POST_INTERVAL_10M 600L * 1000L

// Ethernet setup
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 0, 177);
IPAddress myDns(8, 8, 8, 8);
EthernetClient client;
bool conn_type = CONN_DHCP;

// API setup
char server[] = "worklife.grahamkeggi.com";
unsigned long lastConnectionTime = 0;
unsigned long postingInterval = POST_INTERVAL_10S;

// Access Keys - 32 bytes, exchanged as 64 character hex strings
byte secret[] = { 0x6C,0x00,0xD8,0x68,0x1C,0x65,0x4A,0x7F,0x72,0x15,0xE0,0xC1,0xB8,0xE6,0xBB,0x15,0x00,0x32,0x14,0x95,0x2D,0xE2,0xC5,0xDB,0x91,0x03,0x74,0x48,0xE2,0x10,0xEB,0x41 };
byte token[32];

// Device Identifier - 4 byte decimal
unsigned long device_id;

// State machine 0-idle, 1-awaiting data, 2-rx header, 3-rx data, 4-completed
unsigned char http_state = HTTP_IDLE;
unsigned char request_type;

// Data processing buffer
char data_buffer[DATA_BUFFER_SIZE];
unsigned char data_index = 0;
unsigned char JSON_start = 0;
unsigned char JSON_end = 0;

// Sensor data
unsigned int sensor_reading = 543;

void setup() {
  
  // Start the serial port
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  Serial.println("Boot Complete");

  // Start DHCP ethernet, failback to static IP
  delay(1000);
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    // try to congifure using IP address instead of DHCP:
    Ethernet.begin(mac, ip, myDns);
    conn_type = CONN_MANUAL;
  }
  printIPAddress();

  load_device_id();
  Serial.print("Device ID: ");
  Serial.println(device_id);
  request_type = REQUEST_TOKEN;
  
}

void loop() {

  /* ==== Main HTTP state machine ==== */

  // Connect after preset internval, triggers/resets state machine
  if (millis() - lastConnectionTime > postingInterval) {
    if(request_type == SAVE_READING) {
      sensor_reading = analogRead(A0);
      Serial.print("Reading: ");
      Serial.println(sensor_reading);
    }
    if(httpRequest(request_type) == 0) {
      http_state = HTTP_REQUESTED;
      data_index = 0;
    } else {
      http_state = HTTP_IDLE;
    }
  }

  // Read data from the connection
  if (client.available()) {
    if(http_state == HTTP_REQUESTED) {http_state = HTTP_RXHEADER;}

    char c = client.read();

    if(http_state == HTTP_RXHEADER && c == 0x7B) {http_state = HTTP_RXDATA; data_index = 0;}
    
    if(data_index < DATA_BUFFER_SIZE && (http_state == HTTP_RXHEADER || http_state == HTTP_RXDATA)) {
      data_buffer[data_index] = c;
      data_index++;
    }
  }

  // Check the response was HTTP 200 OK before we bother with the rest
  if(http_state == HTTP_RXHEADER && data_index == HTTP_200_LEN) {
    if(parsehttp_response() != 0) {http_state = HTTP_IDLE;}
  }

  // Look for server closing the connetion
  if (!client.connected()) {
    if(http_state == HTTP_RXDATA) {http_state = HTTP_CLOSED;}
    client.stop();
  }

  // Parse the final data
  if(http_state == HTTP_CLOSED) {
    http_state = HTTP_IDLE;
    parsehttp_data();
    Serial.println("Parsing Data...");
    switch (request_type)
    {
      case REQUEST_TOKEN:
        Serial.println("Received Token");
        // set ID
        set_id(parse_json("id"));
        set_token(parse_json("token"));
        
        // Move to running state
        postingInterval = POST_INTERVAL_10M;
        request_type = SAVE_READING;
        break;

      case SAVE_READING:
        // Don't do anything, future will get timestamps or info to retry or use memory rings
        break;
    }
  }
  
  /* ==== Maintainence Functions ==== */
  // If we got a DHCP lease, maintain it
  if (conn_type == CONN_DHCP) { keepDHCPLease(); }
  
}

char httpRequest(unsigned char endpoint) {
  // close any connection before send a new request.
  client.stop();

  // if there's a successful connection:
  if (client.connect(server, 80)) {

    switch (endpoint)
    {
      case REQUEST_TOKEN:
        client.println("POST /api/devices.php HTTP/1.1");
        break;
        
      case SAVE_READING:
        client.println("POST /api/readings.php HTTP/1.1");
        break;
    }
    
    client.println("Host: worklife.grahamkeggi.com");
    client.println("User-Agent: arduino-ethernet");

    switch (endpoint)
    {
      case REQUEST_TOKEN:
        client.print("X-Device-Secret: ");
        client.print("Bearer ");
        for (unsigned char i=0; i < 32; i++){
          if(secret[i] <= 0x0F){ client.print("0"); }
          client.print(secret[i], HEX);
        }
        client.println();             
        break;
        
      case SAVE_READING:
        client.print("X-Device-Authorization: ");
        client.print("Bearer ");
        for (unsigned char i=0; i < 32; i++){
          if(token[i] <= 0x0F){ client.print("0"); }
          client.print(token[i], HEX);
        }
        client.println();        
        break;
    }

    client.println("Connection: close");


    switch (endpoint)
    {
      case REQUEST_TOKEN:
        client.println("Content-Type: application/x-www-form-urlencoded");
        
        client.print("Content-Length: ");
        client.println(3 + numdigits(device_id));
        
        client.println();
        
        client.print("id=");
        client.print(device_id);
        client.println();
        
        break;
        
      case SAVE_READING:
        client.println("Content-Type: application/x-www-form-urlencoded");
        
        client.print("Content-Length: ");
        client.println(3 + numdigits(device_id) + 19 + numdigits(sensor_reading));
        
        client.println();
        
        client.print("id=");
        client.print(device_id);
        client.print("&channel=0");
        client.print("&reading=");
        client.print(sensor_reading);
        client.println();
        break;
    }
        
    client.println();

    // note the time that the connection was made:
    lastConnectionTime = millis();
    return 0;
  }

  Serial.println("connection failed");
  return 1;
  
}

void keepDHCPLease() {

  switch (Ethernet.maintain())
  {
    case 1:
      //renewed fail
      Serial.println("DHCP Error: renewed fail");
      break;

    case 2:
      //renewed success
      Serial.println("DHCP Renewed success");
      printIPAddress();
      break;

    case 3:
      //rebind fail
      Serial.println("DHCP Error: rebind fail");
      break;

    case 4:
      //rebind success
      Serial.println("DHCP Rebind success");
      printIPAddress();
      break;
  }
}

void printIPAddress()
{

  Serial.print("IP: ");

  for (byte thisByte = 0; thisByte < 4; thisByte++) {
    // print the value of each byte of the IP address:
    Serial.print(Ethernet.localIP()[thisByte], DEC);
    Serial.print(".");
  }

  Serial.println();
}

char parsehttp_response() {
  char response[] = "HTTP/1.1 200 OK";
  
  for (int i=0; i < HTTP_200_LEN; i++){
      if(response[i] != data_buffer[i]) {return 1;}
  }

  return 0;
}

void parsehttp_data() {

    bool state = 0;
    JSON_start = 0;
    JSON_end = 0;
    unsigned char paren_count = 0;

   // Extract JSON from the HTTP response
   for (int i=0; i < DATA_BUFFER_SIZE; i++){
      if(data_buffer[i] == 0x7B) {paren_count++;}
      if(data_buffer[i] == 0x7D) {paren_count--;}
      if(state == 0 && paren_count == 1) {JSON_start = i; JSON_end = i; state = 1;}
      if(state == 1 && paren_count == 0) {JSON_end = i; break;}
   }

   // Dump the whole JSON
   for (int i=JSON_start; i <= JSON_end; i++){
      Serial.write(data_buffer[i]);
   }
   Serial.println();

}

String parse_json(String key) {
  unsigned char matches = 0;
  unsigned char value_start = 0;
  unsigned char value_end = 0;
  unsigned char key_length = key.length();
  String value = "";

  // Search for the key, return if not found
  for (int i=JSON_start; i <= JSON_end; i++){
    if(data_buffer[i] == key[matches]) { matches++; }
    else { matches = 0; }

    if(matches == key_length){ value_start = i; break; }
    if(i == JSON_end) { return ""; }
  }

  // Retrun if the value isn't wrapped in "
  if(data_buffer[value_start+1] != 0x22 || data_buffer[value_start+2] != 0x3A || data_buffer[value_start+3] != 0x22) { return ""; }

  value_start += 4;

  for (int i=value_start; i <= JSON_end; i++){
    if(data_buffer[i] == 0x22) { break; }
    value_end = i;
  }
  
  for (int i=value_start; i <= value_end; i++){
    value += String(data_buffer[i]);
  }
  
  return value;
}

void set_token(String token_str) {

  byte value;
  unsigned char index;
  
  for (unsigned char i = 0; i < token_str.length(); i+=2) {
    value = 16 * chartobyte(chartoupper(token_str[i]));
    value += chartobyte(chartoupper(token_str[i+1]));
    index = i/2;
    token[index] = value;
  }

}

void set_id(String id_str) {

  unsigned long new_id = 0;
  
  for (unsigned char i = 0; i < id_str.length(); i++) {
    new_id *= 10;
    new_id += chartobyte(id_str[i]);
  }

  if (new_id != device_id) {
    save_device_id(new_id);
    Serial.print("Device ID Updated: ");
    Serial.println(device_id);
  }

}

// Operates on the Global variable to save memory
void load_device_id() {

  device_id = 0;

  // Stored MSB first
  for (unsigned char i = 0; i < 4; i+=1) {
    device_id = device_id << 8;
    device_id += EEPROM.read(EEPROM_DEVICE_ID_OFFSET+i);
  }
  
}

void save_device_id(unsigned long new_id) {

  unsigned char data;

  // Stored MSB first
  for (unsigned char i = 0; i < 4; i+=1) {
    data = (unsigned char) (new_id >> (8*(3-i)));
    EEPROM.write(EEPROM_DEVICE_ID_OFFSET+i, data);
  }

  load_device_id();

}

// Number helper functions
unsigned char numdigits(unsigned long number) {
  unsigned char digits = 1;
  unsigned long counter;
  counter = number;

  while(counter > 9) {
    counter /= 10;
    digits += 1;
  }

  return digits;

}

// String manipulation helper functions
char chartoupper(char lower) {
  if(lower >= 97 && lower <= 122) { return lower-32; }
  else { return lower; }
}

byte chartobyte(char text) {
  if(text >= 65 && text <= 70) { return text-55; }
  else if(text >= 48 && text <= 57) { return text-48; }
  else { return 16; }
}
