/*
  router-beacon - displays current bandwidth utilization for an internet connection using NeoPixel LEDs.
  Requires a router running DD-WRT, which has an available API for fetching these statistics.
  
  @author Jacob Quatier
  
  Additional Libraries: 
  
  - Adafruit Industries for example code and libraries for controlling NeoPixels
      https://github.com/adafruit/Adafruit_NeoPixel
  - RestClient for Arduino (simpler API for making HTTP calls)
      https://github.com/csquared/arduino-restclient
  - RegExp for regular expression support
      https://github.com/nickgammon/Regexp
*/

#include <Ethernet.h>
#include <SPI.h>
#include <RestClient.h>
#include <Regexp.h>
#include <Adafruit_NeoPixel.h>

// configuration
#define REFRESH_SECONDS 5
#define PIN 6
#define ROUTER_IP "YOUR_IP_HERE" //ex. 192.168.1.1
#define ROUTER_INTERFACE "vlan1" // must be set to the WAN interface, change as needed.
#define ROUTER_AUTH "Authorization: Basic YOUR_AUTH_HERE" //ex. dj45n22k948dhnjk=
#define URL_PATH "/fetchif.cgi?" ROUTER_INTERFACE

// state
String response;
String speedResults[15];
int currentMatchCount = 0;

long previousMillis = 0;
long prevBytesRecieved = 0;
long prevBytesTransmitted = 0;
long currBytesRecieved = 0;
long currBytesTransmitted = 0;
long currentSpeed = 0;

RestClient client = RestClient(ROUTER_IP);

Adafruit_NeoPixel neoStrip = Adafruit_NeoPixel(16, PIN, NEO_GRB + NEO_KHZ800);

void setup() {
  // initialize all pixels
  neoStrip.begin();
  neoStrip.show();
  
  // serial for debugging
  Serial.begin(9600);
  
  // connect via DHCP
  Serial.println("connecting to network...");
  client.dhcp();
  Serial.print("connected @ ");
  Serial.println(Ethernet.localIP());
}

void loop() {
  unsigned long currentMillis = millis();
  if(currentMillis - previousMillis > (REFRESH_SECONDS * 1000)) {
    previousMillis = currentMillis;
    response = "";
    client.setHeader(ROUTER_AUTH);
    int statusCode = client.get(URL_PATH, &response);
    if(statusCode == 200) {
      response = response.substring(response.indexOf(ROUTER_INTERFACE)+6);
      Serial.println("router response: ");
      Serial.println(response);
      
      char charBuf[response.length()+1];
      response.toCharArray(charBuf, response.length()+1);
      
      // split string by digits followed by a space
      currentMatchCount = 0;
      MatchState ms (charBuf);
      ms.GlobalMatch ("(%d+) ", processMatch);
      
      // save current / previous state
      currBytesRecieved = truncateBytesNumber(speedResults[0]).toInt();
      currBytesTransmitted = truncateBytesNumber(speedResults[8]).toInt();
      calculateBandwidth(currBytesRecieved, currBytesTransmitted);
      prevBytesRecieved = currBytesRecieved;
      prevBytesTransmitted = currBytesTransmitted;
    } else {
      Serial.print("router returned non-200 status code: ");
      Serial.println(statusCode);
    }
  }
  updateDisplay(currentSpeed);
}

String truncateBytesNumber(String num) {
  if(num.length() > 9) {
    return num.substring(num.length() - 9, num.length());
  }
  return num;
}

void calculateBandwidth(long currBytesRecieved, long currBytesTransmitted) {
  Serial.print("recieved: ");
  Serial.print(currBytesRecieved);
  Serial.print(" transmitted: ");
  Serial.println(currBytesTransmitted);
  
  if(prevBytesRecieved > 0 && prevBytesTransmitted > 0) {
    long recieveKbps = ((currBytesRecieved - prevBytesRecieved) / REFRESH_SECONDS) / 1000;
    Serial.print("RECIEVE KBPS: ");
    Serial.println(recieveKbps);
    
    long transmitKbps = ((currBytesTransmitted - prevBytesTransmitted) / REFRESH_SECONDS) / 1000;
    Serial.print("TRANSMIT KBPS: ");
    Serial.println(transmitKbps);
    
    currentSpeed = recieveKbps;
    
    // show data for transmit if that's the greater use of bandwidth
    if(recieveKbps < transmitKbps) {
      currentSpeed = transmitKbps;
    }
  }
}

void updateDisplay(long value) {
  
  // if over the max bandwidth, show full pixels
  if(value > 2000) {
    value = 2000;
  }
  
  int pixels = map(value, 0, 2000, 0, neoStrip.numPixels());
  Serial.print("pixels: ");
  Serial.println(pixels);
  
  uint16_t i, j;
  int d=0;
  for (d=neoStrip.numPixels()-1; d >= pixels; d--) {
    neoStrip.setPixelColor(d, 0);
    neoStrip.show();
    delay(100);
  }
  
  for(j=0; j<256*1; j++) { // 5 cycles of all colors on wheel
    for(i=0; i< pixels; i++) {
      neoStrip.setPixelColor(i, Wheel(((i * 256 / neoStrip.numPixels()) + j) & 255));
      neoStrip.show();
      delay(2);
    }
  }
}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  if(WheelPos < 85) {
    return neoStrip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
  } else if(WheelPos < 170) {
    WheelPos -= 85;
    return neoStrip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  } else {
    WheelPos -= 170;
    return neoStrip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
}

// called for each match
void processMatch (const char * match,         // matching string
                   const unsigned int length,  // length of matching string
                   const MatchState & ms)      // MatchState
{
  char cap [15];   // store captured values
  
  if(ms.level == 1) {
    ms.GetCapture (cap, 0);
    speedResults[currentMatchCount] = cap;
    currentMatchCount++;
  }

}
