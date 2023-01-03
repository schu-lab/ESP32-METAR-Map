// Author: Simon Chu
// Date: 2022-01-02
// Based on Rick Gouin METAR Weather Map.

#include <ESP8266WiFi.h>
#include <FastLED.h>
#include <vector>
using namespace std;

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define FASTLED_ESP8266_RAW_PIN_ORDER

#define NUM_AIRPORTS 27 // This is really the number of LEDs
#define WIND_THRESHOLD 20 // Maximum windspeed for green, otherwise the LED turns yellow
#define LOOP_INTERVAL 5000 // ms - interval between brightness updates and lightning strikes
#define DO_LIGHTNING false // Lightning uses more power, but is cool.
#define DO_WINDS true // color LEDs for high winds
#define REQUEST_INTERVAL 60000 // How often we update. In practice LOOP_INTERVAL is added. In ms (15 min is 900000)

// IOT Connection
const char ssid[] = ""; // your network SSID
const char pass[] = ""; // your network password

// Define the array of LEDs
CRGB leds[NUM_AIRPORTS];
#define DATA_PIN    15 // Data Pin
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB
#define BRIGHTNESS  20 // 20 - 30 recommended.

std::vector<unsigned short int> lightningLeds;
std::vector<String> airports({
  "NULL", // 0  - Order of LEDs, starting with 1 should be KKIC; use VFR, WVFR, MVFR, IFR, LIFR for key; NULL for no airport
  "LIFR", // 1  - LIFR
  "IFR",  // 2  - IFR
  "MVFR", // 3  - MVFR
  "WVFR", // 4  - WVFR
  "VFR",  // 5  - VFR 
  "KAVX", // 6  - AVALON/CATALINA
  "KTOA", // 7  - TORRANCE
  "KLGB", // 8  - LONG BEACH
  "KSLI", // 9  - LOS ALAMITOS
  "KSNA", // 10 - SANTA ANA/WAYNE
  "KFUL", // 11 - FULLERTON
  "KHHR", // 12 - HAWTHORNE
  "KLAX", // 13 - LOS ANGELES INTERNATIONAL
  "KSMO", // 14 - SANTA MONICA
  "KVNY", // 15 - VAN NUYS
  "KWHP", // 16 - WHITEMAN
  "KBUR", // 17 - BURBANK/BOB HOPE
  "KEMT", // 18 - SAN GABRIEL
  "KPOC", // 19 - LA VERNE/BRACKETT
  "KCCB", // 20 - UPLAND/CABLE
  "KONT", // 21 - ONTARIO
  "KCNO", // 22 - CHINO
  "KAJO", // 23 - CORONA
  "KRAL", // 24 - RIVERSIDE
  "KNFG", // 25 - CAMP PENDLETON
  "KOKB", // 26 - OCEANSIDE
});

#define DEBUG false

#define READ_TIMEOUT 15 // Cancel query if no data received (seconds)
#define WIFI_TIMEOUT 60 // in seconds
#define RETRY_TIMEOUT 15000 // in ms

#define SERVER "www.aviationweather.gov"
#define BASE_URI "/adds/dataserver_current/httpparam?dataSource=metars&requestType=retrieve&format=xml&hoursBeforeNow=3&mostRecentForEachStation=true&stationString="

boolean ledStatus = true; // used so leds only indicate connection status on first boot, or after failure
int loops = -1;

int status = WL_IDLE_STATUS;

// SSD1306 Display Setup
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3D ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Initialize Specific Airport 
#define selectedAirport "KSNA"   // Choose your airport

void setup() {
  //Initialize serial and wait for port to open:
  delay(50);
  Serial.begin(9600);

  pinMode(LED_BUILTIN, OUTPUT); // Onboard LED
  digitalWrite(LED_BUILTIN, LOW);

  // Initialize LEDs
  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_AIRPORTS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);
  
  // Initialize with the I2C addr 0x3D
	display.begin(SSD1306_SWITCHCAPVCC, 0x3D);  

  // Display Intro
  display_intro();
}

void loop() {
  digitalWrite(LED_BUILTIN, LOW); // ON if we're awake

  int c;
  loops++;
  Serial.print("Loop: ");
  Serial.println(loops);
  unsigned int loopThreshold = 1;
  if (DO_LIGHTNING) loopThreshold = REQUEST_INTERVAL / LOOP_INTERVAL;

  // Connect to WiFi. We always want a wifi connection for the ESP8266
  if (WiFi.status() != WL_CONNECTED) {
    if (ledStatus) fill_solid(leds, NUM_AIRPORTS, CRGB::Orange); // indicate status with LEDs, but only on first run or error
    FastLED.show();
    WiFi.mode(WIFI_STA);
    WiFi.hostname("LED Sectional " + WiFi.macAddress());
    //wifi_set_sleep_type(LIGHT_SLEEP_T); // use light sleep mode for all delays
    Serial.print("WiFi connecting..");
    WiFi.begin(ssid, pass);
    // Wait up to 1 minute for connection...
    for (c = 0; (c < WIFI_TIMEOUT) && (WiFi.status() != WL_CONNECTED); c++) {
      Serial.write('.');
      delay(1000);
    }
    if (c >= WIFI_TIMEOUT) { // If it didn't connect within WIFI_TIMEOUT
      Serial.println("Failed. Will retry...");
      fill_solid(leds, NUM_AIRPORTS, CRGB::Orange);
      FastLED.show();
      ledStatus = true;
      return;
    }
    Serial.println("OK");
    if (ledStatus) fill_solid(leds, NUM_AIRPORTS, CRGB::Purple); // indicate status with LEDs
    FastLED.show();
    ledStatus = false;    
  }

  // Do some lightning
  if (DO_LIGHTNING && lightningLeds.size() > 0) {
    std::vector<CRGB> lightning(lightningLeds.size());
    for (unsigned short int i = 0; i < lightningLeds.size(); ++i) {
      unsigned short int currentLed = lightningLeds[i];
      lightning[i] = leds[currentLed]; // temporarily store original color
      leds[currentLed] = CRGB::White; // set to white briefly
      Serial.print("Lightning on LED: ");
      Serial.println(currentLed);
    }
    delay(25); // extra delay seems necessary with light sensor
    FastLED.show();
    delay(25);
    for (unsigned short int i = 0; i < lightningLeds.size(); ++i) {
      unsigned short int currentLed = lightningLeds[i];
      leds[currentLed] = lightning[i]; // restore original color
    }
    FastLED.show();
  }

  if (loops >= loopThreshold || loops == 0) {
    loops = 0;
    if (DEBUG) {
      fill_gradient_RGB(leds, NUM_AIRPORTS, CRGB::Red, CRGB::Blue); // Just let us know we're running
      FastLED.show();
    }

    Serial.println("Getting METARs ...");
    if (getMetars()) {
      Serial.println("Refreshing LEDs.");
      FastLED.show();
      if ((DO_LIGHTNING && lightningLeds.size() > 0)) {
        Serial.println("There is lightning, so no long sleep.");
        digitalWrite(LED_BUILTIN, HIGH);
        delay(LOOP_INTERVAL); // pause during the interval
      } else {
        Serial.print("No lightning; Going into sleep for: ");
        Serial.println(REQUEST_INTERVAL);
        digitalWrite(LED_BUILTIN, HIGH);
        delay(REQUEST_INTERVAL);
      }
    } else {
      digitalWrite(LED_BUILTIN, HIGH);
      delay(RETRY_TIMEOUT); // try again if unsuccessful
    }
  } else {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(LOOP_INTERVAL); // pause during the interval
  }

  // Display Fun Message
  display_message();
  delay(10000);

  // Display Intro
  display_intro();
  delay(10000);

  // Clear the Display Buffer
  display.clearDisplay();
  display.display();
}

bool getMetars(){
  lightningLeds.clear(); // clear out existing lightning LEDs since they're global
  fill_solid(leds, NUM_AIRPORTS, CRGB::Black); // Set everything to black just in case there is no report
  uint32_t t;
  char c;
  boolean readingAirport = false;
  boolean readingCondition = false;
  boolean readingObs = false;
  boolean readingLat = false;
  boolean readingLong = false;
  boolean readingTemp = false;
  boolean readingDew = false;
  boolean readingDir = false;
  boolean readingWind = false;
  boolean readingGusts = false;
  boolean readingVisibility = false;
  boolean readingWxstring = false;

  std::vector<unsigned short int> led;
  String currentAirport = "";
  String currentCondition = "";
  String currentObs = "";
  String currentLine = "";
  String currentLat = "";
  String currentLong = "";
  String currentTemp = "";
  String currentDew = "";
  String currentDir = "";
  String currentWind = "";
  String currentGusts = "";
  String currentVisibility = "";
  String currentWxstring = "";
  String airportString = "";
  bool firstAirport = true;
  for (int i = 0; i < NUM_AIRPORTS; i++) {
    if (airports[i] != "NULL" && airports[i] != "VFR" && airports[i] != "MVFR" && airports[i] != "WVFR" && airports[i] != "IFR" && airports[i] != "LIFR") {
      if (firstAirport) {
        firstAirport = false;
        airportString = airports[i];
      } else airportString = airportString + "," + airports[i];
    }
  }

  BearSSL::WiFiClientSecure client;
  client.setInsecure();
  Serial.println("\nStarting connection to server...");
  // if you get a connection, report back via serial:
  if (!client.connect(SERVER, 443)) {
    Serial.println("Connection failed!");
    client.stop();
    return false;
  } else {
    Serial.println("Connected ...");
    Serial.print("GET ");
    Serial.print(BASE_URI);
    Serial.print(airportString);
    Serial.println(" HTTP/1.1");
    Serial.print("Host: ");
    Serial.println(SERVER);
    Serial.println("User-Agent: LED Map Client");
    Serial.println("Connection: close");
    Serial.println();
    // Make a HTTP request, and print it to console:
    client.print("GET ");
    client.print(BASE_URI);
    client.print(airportString);
    client.println(" HTTP/1.1");
    client.print("Host: ");
    client.println(SERVER);
    client.println("User-Agent: LED Sectional Client");
    client.println("Connection: close");
    client.println();
    client.flush();
    t = millis(); // start time
    FastLED.clear();

    Serial.print("Getting data");
  
    while (!client.connected()) {
      if ((millis() - t) >= (READ_TIMEOUT * 1000)) {
        Serial.println("---Timeout---");
        client.stop();
        return false;
      }
      Serial.print(".");
      delay(1000);
    }

    Serial.println();

    while (client.connected()) {
      if ((c = client.read()) >= 0) {
        yield(); // Otherwise the WiFi stack can crash
        currentLine += c;
        if (c == '\n') currentLine = "";
        if (currentLine.endsWith("<station_id>")) { // start paying attention
          if (!led.empty()) { // we assume we are recording results at each change in airport
            for (vector<unsigned short int>::iterator it = led.begin(); it != led.end(); ++it) {
              doColor(currentAirport, *it, currentObs, currentLat.toFloat(), currentLong.toFloat(), currentTemp.toInt(),
               currentDew.toInt(), currentDir.toInt(), currentWind.toInt(), currentGusts.toInt(), currentVisibility.toInt(), 
               currentCondition, currentWxstring);
            }
            led.clear();
          }
          currentAirport = ""; // Reset everything when the airport changes
          readingAirport = true;
          currentCondition = "";
          currentObs = "";
          currentLat = "";
          currentLong = "";
          currentTemp = "";
          currentDew = "";
          currentDir = "";
          currentWind = "";
          currentGusts = "";
          currentVisibility = "";
          currentWxstring = "";
        } else if (readingAirport) {
          if (!currentLine.endsWith("<")) {
            currentAirport += c;
          } else {
            readingAirport = false;
            for (unsigned short int i = 0; i < NUM_AIRPORTS; i++) {
              if (airports[i] == currentAirport) {
                led.push_back(i);
              }
            }
          }
        } else if (currentLine.endsWith("<observation_time>")) {
          readingObs = true;
        } else if (readingObs) {
          if (!currentLine.endsWith("<")) {
            currentObs += c;
          } else {
            readingObs = false;
          }                    
        } else if (currentLine.endsWith("<latitude>")) {
          readingLat = true;
        } else if (readingLat) {
          if (!currentLine.endsWith("<")) {
            currentLat += c;
          } else {
            readingLat = false;
          }                          
        } else if (currentLine.endsWith("<longitude>")) {
          readingLong = true;
        } else if (readingLong) {
          if (!currentLine.endsWith("<")) {
            currentLong += c;
          } else {
            readingLong = false;
          }                  
        } else if (currentLine.endsWith("<temp_c>")) {
          readingTemp = true;
        } else if (readingTemp) {
          if (!currentLine.endsWith("<")) {
            currentTemp += c;
          } else {
            readingTemp = false;
          }              
        } else if (currentLine.endsWith("<dewpoint_c>")) {
          readingDew = true;
        } else if (readingDew) {
          if (!currentLine.endsWith("<")) {
            currentDew += c;
          } else {
            readingDew = false;
          }          
        } else if (currentLine.endsWith("<wind_dir_degrees>")) {
          readingDir = true;
        } else if (readingDir) {
          if (!currentLine.endsWith("<")) {
            currentDir += c;
          } else {
            readingDir = false;
          }
        } else if (currentLine.endsWith("<wind_speed_kt>")) {
          readingWind = true;
        } else if (readingWind) {
          if (!currentLine.endsWith("<")) {
            currentWind += c;
          } else {
            readingWind = false;
          }
        } else if (currentLine.endsWith("<wind_gust_kt>")) {
          readingGusts = true;
        } else if (readingGusts) {
          if (!currentLine.endsWith("<")) {
            currentGusts += c;
          } else {
            readingGusts = false;
          }
        } else if (currentLine.endsWith("<visibility_statute_mi>")) {
          readingVisibility = true;
        } else if (readingVisibility) {
          if (!currentLine.endsWith("<")) {
            currentVisibility += c;
          } else {
            readingVisibility = false;
          }
        } else if (currentLine.endsWith("<flight_category>")) {
          readingCondition = true;
        } else if (readingCondition) {
          if (!currentLine.endsWith("<")) {
            currentCondition += c;
          } else {
            readingCondition = false;
          }
        } else if (currentLine.endsWith("<wx_string>")) {
          readingWxstring = true;
        } else if (readingWxstring) {
          if (!currentLine.endsWith("<")) {
            currentWxstring += c;
          } else {
            readingWxstring = false;
          }
        }
        t = millis(); // Reset timeout clock
      } else if ((millis() - t) >= (READ_TIMEOUT * 1000)) {
        Serial.println("---Timeout---");
        fill_solid(leds, NUM_AIRPORTS, CRGB::Cyan); // indicate status with LEDs
        FastLED.show();
        ledStatus = true;
        client.stop();
        return false;
      }
    }
  }
  
  // need to doColor this for the last airport
  for (vector<unsigned short int>::iterator it = led.begin(); it != led.end(); ++it) {
    doColor(currentAirport, *it, currentObs, currentLat.toFloat(), currentLong.toFloat(), currentTemp.toInt(), 
    currentDew.toInt(), currentDir.toInt(), currentWind.toInt(), currentGusts.toInt(), currentVisibility.toInt(), 
    currentCondition, currentWxstring);
  }
  
  led.clear();

  // Do the key LEDs now if they exist
  for (int i = 0; i < (NUM_AIRPORTS); i++) {
    // Use this opportunity to set colors for LEDs in our key then build the request string
    if (airports[i] == "VFR") leds[i] = CRGB::Green;
    else if (airports[i] == "WVFR") leds[i] = CRGB::Yellow;
    else if (airports[i] == "MVFR") leds[i] = CRGB::Blue;
    else if (airports[i] == "IFR") leds[i] = CRGB::Red;
    else if (airports[i] == "LIFR") leds[i] = CRGB::Magenta;
  }

  client.stop();
  return true;
}

void doColor(String identifier, unsigned short int led, String obs, float latitude, float longitude, 
int temp, int dew, int dir, int wind, int gusts, int visibility, String condition, String wxstring) {
  CRGB color;
  Serial.print(identifier);
  Serial.print(": ");
  Serial.print(condition);
  Serial.print(" ");
  Serial.print(dir);
  Serial.print(" ");
  Serial.print(wind);
  Serial.print("G ");
  Serial.print(gusts);
  Serial.print("kts LED ");
  Serial.print(led);
  Serial.print(" WX: ");
  Serial.println(wxstring);
  if (wxstring.indexOf("TS") != -1) {
    Serial.println("... found lightning!");
    lightningLeds.push_back(led);
  }
  if (condition == "LIFR" || identifier == "LIFR") color = CRGB::Magenta;
  else if (condition == "IFR") color = CRGB::Red;
  else if (condition == "MVFR") color = CRGB::Blue;
  else if (condition == "VFR") {
    if ((wind > WIND_THRESHOLD || gusts > WIND_THRESHOLD) && DO_WINDS) {
      color = CRGB::Yellow;
    } else {
      color = CRGB::Green;
    }
  } else color = CRGB::Black;

  if (identifier == selectedAirport) {
    Serial.print("Selected Airport: ");
    Serial.println(identifier);
    Serial.print("Flight Catagory: ");
    Serial.println(condition);

    // Display Selected Airport
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(BLACK, WHITE);
    display.setCursor(0, 0);
    display.println(" METAR:              ");
    display.setTextColor(WHITE, BLACK);
    display.print("Station ID: ");
    display.println(identifier);
    display.println(obs);
    display.print("Wind Direct: ");
    display.print(dir);
    display.println(" deg");
    display.print("Wind Speed:  ");
    display.print(wind);
    display.println(" kts");
    display.print("Visibility:  ");
    display.print(visibility);
    display.println(" mi");
    display.print("Temp/Dew: ");
    display.print(temp);
    display.print(" C / ");
    display.print(dew);
    display.println(" %");
    display.print("Coord: ");
    display.print(latitude);
    display.print(", ");
    display.println(longitude);
    display.display();
    Serial.println("Weather Display ON");
    }
  leds[led] = color;
}

// Initial display when powered ON
void display_intro() {
  display.clearDisplay();
	display.setTextSize(1);
	display.setTextColor(WHITE);
	display.setCursor(0,24);
	display.println("Simon's Los Angeles");
  display.println("VFR Sectional Map");
	display.display();
  Serial.println("Introduction Display ON");
}

// Display message
void display_message() {
  display.clearDisplay();
  display.setTextSize(1);
	display.setCursor(0, 0);
  display.setTextColor(WHITE, BLACK);
  display.println("Message:");
  display.setTextSize(2);
  display.println();
  display.println("Let's Fly!");
  display.setTextSize(1);
  display.println("I got a Mini 2...");
	display.display();
  Serial.println("Message Display ON");
}
