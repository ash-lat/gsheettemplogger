/*
 * ESP822 temprature logging to Google Sheet
 * CircuitDigest(www.circuitdigest.com)
*/

#include <ESP8266WiFi.h>
#include "HTTPSRedirect.h"
#include "DebugMacros.h"
#include <DHT.h>
#include "NTPClient.h"
#include "WiFiUdp.h"

#define DHTPIN 14                                                           // what digital pin we're connected to
#define DHTTYPE DHT11                                                       // select dht type as DHT 11 or DHT22
DHT dht(DHTPIN, DHTTYPE);

const long utcOffsetInSeconds = 19800;
String weekDays[7]={"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
String months[12]={"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

float h;
float t;
String sheetHumid = "";
String sheetTemp = "";

const char* ssid = "Emerald 24G";                //replace with our wifi ssid
const char* password = "emerald98260";         //replace with your wifi password

const char* host = "script.google.com";
const char *GScriptId = "AKfycbxh2H7fmF5cKeGxFpj0h7k0Z1pNogjkFqdCvIOH42CdvHjugGo"; // Replace with your own google script id
const int httpsPort = 443; //the https port is same

// echo | openssl s_client -connect script.google.com:443 |& openssl x509 -fingerprint -noout
const char* fingerprint = "";

//const uint8_t fingerprint[20] = {};

String url = String("/macros/s/") + GScriptId + "/exec?value=Date";  // Write Date to Google Spreadsheet at cell A1
// Fetch Google Calendar events for 1 week ahead
String url2 = String("/macros/s/") + GScriptId + "/exec?cal";  // Write to Cell A continuosly

//replace with sheet name not with spreadsheet file name taken from google
String payload_base =  "{\"command\": \"appendRow\", \
                    \"sheet_name\": \"TempSheet\", \
                       \"values\": ";
String payload = "";

HTTPSRedirect* client = nullptr;

// used to store the values of free stack and heap before the HTTPSRedirect object is instantiated
// so that they can be written to Google sheets upon instantiation

void setup() {
  delay(1000);
  Serial.begin(115200);
  dht.begin();     //initialise DHT11

  Serial.println();
  Serial.print("Connecting to wifi: ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

}

void loop() {
timeClient.update();
String formattedTime = timeClient.getFormattedTime();
Serial.print("Formatted Time: ");
Serial.println(formattedTime);  
//Serial.println(timeClient.getFormattedTime());
  unsigned long epochTime = timeClient.getEpochTime();
  struct tm *ptm = gmtime ((time_t *)&epochTime); 

int monthDay = ptm->tm_mday;
  int currentMonth = ptm->tm_mon+1;
  int currentYear = ptm->tm_year+1900;
  // Use HTTPSRedirect class to create a new TLS connection
  client = new HTTPSRedirect(httpsPort);
  client->setInsecure();
  String currentDate = String(currentYear) + "-" + String(currentMonth) + "-" + String(monthDay);
  Serial.print("Current date: ");
  Serial.println(currentDate);
  client->setPrintResponseBody(true);
  client->setContentTypeHeader("application/json");
  Serial.print("Connecting to ");
  Serial.println(host);          //try to connect with "script.google.com"

  // Try to connect for a maximum of 5 times then exit
  bool flag = false;
  for (int i = 0; i < 5; i++) {
    int retval = client->connect(host, httpsPort);
    if (retval == 1) {
      flag = true;
      break;
    }
    else
      Serial.println("Connection failed. Retrying...");
  }

  if (!flag) {
    Serial.print("Could not connect to server: ");
    Serial.println(host);
    Serial.println("Exiting...");
    return;
  }
// Finish setup() function in 1s since it will fire watchdog timer and will reset the chip.
//So avoid too many requests in setup()

  Serial.println("\nWrite into cell 'A1'");
  Serial.println("------>");
  // fetch spreadsheet data
  client->GET(url, host);
  
  Serial.println("\nGET: Fetch Google Calendar Data:");
  Serial.println("------>");
  //fetch spreadsheet data
  client->GET(url2, host);

 Serial.println("\nStart Sending Sensor Data to Google Spreadsheet");

  
  // delete HTTPSRedirect object
  delete client;
  client = nullptr;

 float h = dht.readHumidity();                                              // Reading temperature or humidity takes about 250 milliseconds!
 float t = dht.readTemperature();                                           // Read temperature as Celsius (the default)
  if (isnan(h) || isnan(t)) {                                                // Check if any reads failed and exit early (to try again).
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }
  Serial.print("Humidity: ");  Serial.print(h);
  sheetHumid = String(h) + String("%");                                         //convert integer humidity to string humidity
  Serial.print("%  Temperature: ");  Serial.print(t);  Serial.println("°C ");
  sheetTemp = String(t) + String("°C");

  static int error_count = 0;
  static int connect_count = 0;
  const unsigned int MAX_CONNECT = 20;
  static bool flag1 = false;

  payload = payload_base + "\"" + currentDate + "," + formattedTime + "," + sheetTemp + "," + sheetHumid + "\"}";

  if (!flag1) {
    client = new HTTPSRedirect(httpsPort);
    client->setInsecure();
    flag1 = true;
    client->setPrintResponseBody(true);
    client->setContentTypeHeader("application/json");
  }

  if (client != nullptr) {
    if (!client->connected()) {
      client->connect(host, httpsPort);
      client->POST(url2, host, payload, false);
      Serial.print("Sent : ");  Serial.println("Temp and Humid");
    }
  }
  else {
    DPRINTLN("Error creating client object!");
    error_count = 5;
  }

  if (connect_count > MAX_CONNECT) {
    connect_count = 0;
    flag = false;
    delete client;
    return;
  }

//  Serial.println("GET Data from cell 'A1':");
//  if (client->GET(url3, host)) {
//    ++connect_count;
//  }
//  else {
//    ++error_count;
//    DPRINT("Error-count while connecting: ");
//    DPRINTLN(error_count);
//  }

  //Serial.println("POST or SEND Sensor data to Google Spreadsheet:");
  //if (client->POST(url2, host, payload)) {
  //  ;
 // }
  else {
    ++error_count;
    DPRINT("Error-count while connecting: ");
    DPRINTLN(error_count);
  }

  if (error_count > 3) {
    Serial.println("Halting processor...");
    delete client;
    client = nullptr;
    Serial.printf("Final free heap: %u\n", ESP.getFreeHeap());
    Serial.printf("Final stack: %u\n", ESP.getFreeContStack());
    Serial.flush();
    ESP.deepSleep(0);
  }
  
  delay(900000);    // keep delay of minimum 2 seconds as dht allow reading after 2 seconds interval and also for google sheet
}
