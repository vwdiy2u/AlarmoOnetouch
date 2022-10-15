/***********************************
 * Alarmo OneTouch Button Firmware *
 ***********************************

https://github.com/vwdiy2u/AlarmoOnetouch

- alarmo_onetouch_button.ino
- Wemos D1 Mini
- Arduino 1.8.16
- Last Updated on 15th October 2022

Publish to: alarmo/command
disarm
arm_home
arm_away
arm_night

Subscribe and listen to: alarmo/state
disarmed
arming
armed_home
armed_away
armed_night
pending
triggered

Blink LED / buzzer beep according to status of subscribed topic
One button click translates user action to command to be published
Simple web interface to show current status and device information
IoTWebConfig for wifi, ntp, mqtt setup and OTA firmware update
Glow/fade led if mqtt not connected

*/

#include "ESP8266WiFi.h"
#include "PubSubClient.h"

#include <IotWebConf.h>
#include "IotWebConfUsing.h" // This loads aliases for easier class names.

//for NTP
#include <WiFiUdp.h>
#include <Ticker.h>

Ticker ticker;
Ticker ticker_buzzer;

// -- Initial name of the Thing. Used e.g. as SSID of the own Access Point.
const char thingName[] = "AlarmoOneTouch";

// -- Initial password to connect to the Thing, when it creates an own Access Point.
const char wifiInitialApPassword[] = "12345678";

#define STRING_LEN 128
#define NUMBER_LEN 32

// -- Configuration specific key. The value should be modified if config structure was changed.
#define CONFIG_VERSION "AlarmoOneTouchR0"

#define BTN_LED D5        //connect to LED on button - active high
#define BTN_PIN D3        //connect to tact switch - active low
#define BUZZER D6         //buzzer - active high

int brightness = 0;
int fadeAmount = 1;

#include "GyverButton.h"
GButton butt1(BTN_PIN);
int button_presscount = 0;
bool button_hasclicks = false;

// -- Method declarations.
void handleRoot();
bool connectAp(const char* apName, const char* password);
void connectWifi(const char* ssid, const char* password);
// -- Callback methods.
void configSaved();
bool formValidator(iotwebconf::WebRequestWrapper* webRequestWrapper);

DNSServer dnsServer;
WebServer server(80);

char ipAddressValue[STRING_LEN];
char gatewayValue[STRING_LEN];
char netmaskValue[STRING_LEN];
char StaticIP_enabledVal[STRING_LEN];
boolean StaticIP_enabled = false;

char mqttServer[60];
char mqttUserName[32];
char mqttUserPassword[32];
char MQTT_enabledVal[STRING_LEN];
boolean MQTT_enabled = false;
#define  mqttPort  1883
char mqtt_topic[32];
boolean MQTT_subscribe_onpower = true;
bool mqttconnected = false;

#define mqtt_retries_max 2                          //number of retries before it stop trying to free reconnect blocking function so that user can enter http interface to setup
int mqtt_retries=0;
unsigned long mqtt_auto_reconnect_timer = 100000;   //timer in ms for when mqtt starts to retry reconnect procedure from mqtt_connection_ok = false state

unsigned long previousMillis2 = 0;
String mac_address_as_uid;
String payload_string = "";
String last_payload_string = "";
String pubAlarmoStateTopicStr = "alarmo/state";         //listen to topic
String subAlarmoCommandTopicStr = "alarmo/command";     //send command to topic
bool disarmed = true;                                   //state is true if disarmed
bool exitentry_beep = false;

IPAddress ipAddress;
IPAddress gateway;
IPAddress netmask;

//MQTT
WiFiClient wifiClient;
#define  MQTT_MSG_SIZE    100
#define MSG_BUFFER_SIZE  (50)
char mqtt_msg[MSG_BUFFER_SIZE];
PubSubClient mqttClient(wifiClient);

unsigned long previousMillis = 0;
unsigned long interval = 10000;     //10 second counter
unsigned long previousMillis3 = 0;  
unsigned long interval3 = 5000;     //5 second counter
unsigned long previousMillis4 = 0;  
unsigned long interval4 = 1;        //10 mili second counter

// Define NTP Client to get time
int cNTP_Update = 0;                      // Counter for Updating the time via NTP
char ntpServer[60];
char ntpUpdateTime[4];
char ntpTimeZone[4];
char ntpDaylight_enabledVal[STRING_LEN];
boolean ntpDaylight_enabled = false;

const int NTP_PACKET_SIZE = 48; 
byte packetBuffer[ NTP_PACKET_SIZE]; 
boolean firstStart = true;                    // On firststart = true, NTP will try to get a valid time
static const uint8_t monthDays[]={31,28,31,30,31,30,31,31,30,31,30,31}; 
#define LEAP_YEAR(Y) ( ((1970+Y)>0) && !((1970+Y)%4) && ( ((1970+Y)%100) || !((1970+Y)%400) ) )
struct  strDateTime {
   byte hour;
   byte minute;
   byte second;
   int year;
   byte month;
   byte day;
   byte wday;   // Weekday
};
strDateTime DateTime;                     // Global DateTime structure, will be refreshed every Second
WiFiUDP UDPNTPClient;                     // NTP Client
unsigned long UnixTimestamp = 0;          // GLOBALTIME  ( Will be set by NTP)
Ticker tkSecond;                          // Second - Timer for Updating Datetime Structure
boolean Refresh = false;                  // For Main Loop, to refresh things 

//Secure OTA update
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
const char* host = "webupdate";
const char* update_path = "/firmware";
const char* update_username = "admin";
const char* update_password = "admin";
ESP8266HTTPUpdateServer httpUpdater;

//Timestamp
#include "time.h"
#include "TimeLib.h"
time_t uptime;
time_t alarmo_state_timestamp;
time_t button_timestamp;
time_t onmqtt_timestamp;
time_t disarmed_timestamp;
time_t arming_timestamp;
time_t armed_home_timestamp;
time_t armed_away_timestamp;
time_t armed_night_timestamp;
time_t pending_timestamp;
time_t triggered_timestamp;

IotWebConf iotWebConf(thingName, &dnsServer, &server, wifiInitialApPassword, CONFIG_VERSION);
// -- You can also use namespace formats e.g.: iotwebconf::ParameterGroup
IotWebConfParameterGroup connGroup = IotWebConfParameterGroup("conn", "Connection parameters");
IotWebConfCheckboxParameter StaticIP_enabledParam = IotWebConfCheckboxParameter("Enable Static IP (DHCP if disabled)", "StaticIP_enabledParam", StaticIP_enabledVal, STRING_LEN);
IotWebConfTextParameter ipAddressParam = IotWebConfTextParameter("IP address", "ipAddress", ipAddressValue, STRING_LEN, "", nullptr, "");
IotWebConfTextParameter gatewayParam = IotWebConfTextParameter("Gateway", "gateway", gatewayValue, STRING_LEN, "", nullptr, "");
IotWebConfTextParameter netmaskParam = IotWebConfTextParameter("Subnet mask", "netmask", netmaskValue, STRING_LEN, "255.255.255.0", nullptr, "255.255.255.0");

IotWebConfParameterGroup mqttGroup = IotWebConfParameterGroup("mqtt", "MQTT parameters");
IotWebConfCheckboxParameter MQTT_enabledParam = IotWebConfCheckboxParameter("Enable MQTT (Reboot required)", "MQTT_enabledParam", MQTT_enabledVal, STRING_LEN);
IotWebConfTextParameter mqttServerParam = IotWebConfTextParameter("MQTT server", "mqttServer", mqttServer, sizeof(mqttServer),"broker.mqtt-dashboard.com");
IotWebConfTextParameter mqttUserNameParam = IotWebConfTextParameter("MQTT username (optional)", "mqttUser", mqttUserName, sizeof(mqttUserName));
IotWebConfTextParameter mqttUserPasswordParam = IotWebConfTextParameter("MQTT password (optional)", "mqttPass", mqttUserPassword, sizeof(mqttUserPassword));
IotWebConfTextParameter mqttTopicParam = IotWebConfTextParameter("MQTT Topic (optional)", "mqttTopic", mqtt_topic, sizeof(mqtt_topic));

IotWebConfParameterGroup ntpGroup = IotWebConfParameterGroup("ntp", "NTP parameters");
IotWebConfTextParameter ntpServerParam = IotWebConfTextParameter("NTP server", "ntpServer", ntpServer, sizeof(ntpServer),"pool.ntp.org");
IotWebConfTextParameter ntpUpdateTimeParam = IotWebConfTextParameter("Update Time (Minutes)", "ntpUpdateTime", ntpUpdateTime, sizeof(ntpUpdateTime),"60");
IotWebConfTextParameter ntpTimeZoneParam = IotWebConfTextParameter("Time Zone (GMT)", "ntpTimeZone", ntpTimeZone, sizeof(ntpTimeZone),"8");
IotWebConfCheckboxParameter ntpDaylight_enabledParam = IotWebConfCheckboxParameter("Daylight saving", "ntpDaylight", ntpDaylight_enabledVal, STRING_LEN);


void tick()
{
  digitalWrite(BTN_LED, !digitalRead(BTN_LED));     // set pin to the opposite state
}

void beep()
{
  digitalWrite(BUZZER, HIGH);     
  ticker_buzzer.attach(0.03, beep_off);
}

void beep_off()
{
  digitalWrite(BUZZER, LOW);     
  ticker_buzzer.detach();
}

void blinkled()
{
  digitalWrite(BTN_LED, HIGH);     
  ticker.attach(0.03, blinkled_off);
}

void blinkled_off()
{
  digitalWrite(BTN_LED, LOW);     
  ticker.detach();
}

void handleRoot()
{
  // -- Let IotWebConf test and handle captive portal requests.
  if (iotWebConf.handleCaptivePortal())
  {
    // -- Captive portal request were already served.
    return;
  }

  IPAddress ip;                    // the IP address of your shield
  ip = WiFi.localIP();
  
  int quality;
  long dBm;
  dBm = WiFi.RSSI();
  
  // dBm to Quality:
  if(dBm <= -100)
      quality = 0;
  else if(dBm >= -50)
      quality = 100;
  else
      quality = 2 * (dBm + 100);
        
  String s = "<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/>";
  s += "<title>";
  s += iotWebConf.getThingName();
  s += "</title></head><body><div>Status page of <b>";
  s += iotWebConf.getThingName();
  s += "</b>.</div>";
  s += "<ul>";
  s += "<li>Connected to ";
  s += WiFi.SSID();
  s += " <font color='#0000FF'>";
  s += quality;
  s += "% [";
  s += dBm;
  s += "dBm]</font>";
  s += "<li>WiFi IP: ";
  s += WiFi.localIP().toString();
  
  if (StaticIP_enabled) {
    s += "  <span style=\"color: Blue\">Static IP</span>";
  } else {
    s += "  DHCP";    
  }
  
  s += "<li>MAC address: "; // Go to the next line.
  s += GetMacAddress();
  s += "<li>Current Date Time (GMT ";
  s += ntpTimeZone;
  s += ") : ";

  if(DateTime.year == 1970) 
    s += "<span style=\"color: Red\">";
  else  
    s += "<span style=\"color: Black\">";
    
  s += DateTime.day;
  s += "-";
  s += DateTime.month;
  s += "-";
  s += DateTime.year;
  s += " ";
  s += DateTime.hour;
  s += ":";
  s += DateTime.minute;
  s += ":";
  s += DateTime.second;
  s += "</span>";
  s += "<li>Uptime: ";
  s += getFormatedDuration(time(nullptr) - uptime);
  s += "<li>Heaps (RAM): ";
  s += String(ESP.getFreeHeap());
  s += "</ul>";
  s += "<ul>";
  s += "<li>MQTT: ";
  if (MQTT_enabled) {
    s += mqttServer;
    s += ":";
    s += mqttPort;
    s += "  [Enabled]";
  
    if (mqttClient.connected()) {
      s += " [Connected]"; 
    }
    else {
      s += "<span style=\"color: Red\"> [Not connected]</span>"; 
    }
  
  } else {
    s += "  <span style=\"color: Blue\">Disabled</span>";    
  }

  s += "<br>Subscribed to topic: ";
  s += pubAlarmoStateTopicStr;
  s += "<br>Publish command to topic: ";
  s += subAlarmoCommandTopicStr;
  s += "<br><br><li>onMqttMessage: \"<b>";
  s += String(last_payload_string);
  s += "\"</b> last triggered ";
  s += getFormatedDuration(time(nullptr) - onmqtt_timestamp);
  s += " ago";
  s += "<li>Button status: ";
  s += button_presscount;
  s += " last triggered ";
  s += getFormatedDuration(time(nullptr) - button_timestamp);
  s += " ago<br>";
  
  s += "<br>Alarmo was disarmed ";
  s += getFormatedDuration(time(nullptr) - disarmed_timestamp);
  s += " ago";
  s += "<br>Alarmo was arming ";
  s += getFormatedDuration(time(nullptr) - arming_timestamp);
  s += " ago";
  s += "<br>Alarmo was armed_home ";
  s += getFormatedDuration(time(nullptr) - armed_home_timestamp);
  s += " ago";
  s += "<br>Alarmo was armed_away ";
  s += getFormatedDuration(time(nullptr) - armed_away_timestamp);
  s += " ago";
  s += "<br>Alarmo was armed_night ";
  s += getFormatedDuration(time(nullptr) - armed_night_timestamp);
  s += " ago";
  s += "<br>Alarmo was pending ";
  s += getFormatedDuration(time(nullptr) - pending_timestamp);
  s += " ago";
  s += "<br>Alarmo was triggered ";
  s += getFormatedDuration(time(nullptr) - triggered_timestamp);
  s += " ago";

  s += "</ul>";
  s += "Go to <a href='config'>configuration</a> page to change settings.";
  s += "<br><a href='reboot'>Reboot Device</a> to reflect configuration changes.";
  s += "<br>Secure OTA <a href='firmware'>Firmware Update</a>.";
  s += "</body></html>\n";

  server.send(200, "text/html", s);
}

void configSaved()
{
  Serial.println("Configuration was updated.");
}

bool formValidator(iotwebconf::WebRequestWrapper* webRequestWrapper)
{
  Serial.println("Validating form.");
  bool valid = true;

if (StaticIP_enabled) {  
  if (!ipAddress.fromString(webRequestWrapper->arg(ipAddressParam.getId())))
  {
    ipAddressParam.errorMessage = "Please provide a valid IP address!";
    valid = false;
  }
  if (!netmask.fromString(webRequestWrapper->arg(netmaskParam.getId())))
  {
    netmaskParam.errorMessage = "Please provide a valid netmask!";
    valid = false;
  }
  if (!gateway.fromString(webRequestWrapper->arg(gatewayParam.getId())))
  {
    gatewayParam.errorMessage = "Please provide a valid gateway address!";
    valid = false;
  }
}

if (MQTT_enabled) {  
  int l = server.arg(mqttServerParam.getId()).length();
  if (l == 0) {
    mqttServerParam.errorMessage = "Please provide the MQTT server address (x.x.x.x)";
    valid = false;
  }
}
  
  return valid;
}

bool connectAp(const char* apName, const char* password)
{
  // -- Custom AP settings
  return WiFi.softAP(apName+mac_address_as_uid, password, 4);
}

void connectWifi(const char* ssid, const char* password)
{
  if (StaticIP_enabled) {
    ipAddress.fromString(String(ipAddressValue));
    netmask.fromString(String(netmaskValue));
    gateway.fromString(String(gatewayValue));

  if (!WiFi.config(ipAddress, gateway, netmask)) {
    Serial.println("STA Failed to configure");
  }
  
  Serial.print("ip: ");
  Serial.println(ipAddress);
  Serial.print("gw: ");
  Serial.println(gateway);
  Serial.print("net: ");
  Serial.println(netmask);
}
  WiFi.begin(ssid, password);
}

void rebootDevice() {

  server.send(200, "text/plain", "Rebooting..");
  delay(1000);
  ESP.restart();
}

void wifiConnected() {

  if(MQTT_enabled)
    mqttReconnect();
 
  Serial.print("wifiConnected: ");
}

void mqttReconnect() {
  
  if (!mqttClient.connected() && (mqtt_retries < mqtt_retries_max)) {
    mqttClient.setServer( mqttServer, mqttPort);
    mqttClient.setCallback(onMqttMessage);
    mqttClient.loop();

    mqtt_retries++;
    Serial.print("Attempting MQTT connection at ");
    Serial.print(mqttServer);
    Serial.print(":");
    Serial.print(mqttPort);
    Serial.print("...");

    if (mqttClient.connect(iotWebConf.getThingName(), mqttUserName, mqttUserPassword)) {

      Serial.println(F("Connected to MQTT"));
      Serial.println(F("Device ready ......"));
      mqtt_retries=0;  //if connected, reset counter

      //Once re-connected to MQTT, re-subscribe to topics
      
      String mqtt_pubTopic_char_ptr_str;
      const char *mqtt_pubTopic_char_ptr_str_full;

      mqtt_pubTopic_char_ptr_str = pubAlarmoStateTopicStr;
      mqtt_pubTopic_char_ptr_str_full = mqtt_pubTopic_char_ptr_str.c_str();
      mqttClient.subscribe(mqtt_pubTopic_char_ptr_str_full);
      mqttClient.loop();
      mqttconnected = true;
      digitalWrite(BTN_LED, LOW); //keep LED off when 1st connected

    }
    else {
      Serial.print("failed, rc=");
      Serial.println(mqttClient.state());
      mqttClient.loop();
      delay(1);
      mqttconnected = false;

      if (mqtt_retries >= mqtt_retries_max) {
          Serial.println("MQTT reconnection disabled!");
          mqttClient.loop();
      }
    }
    
  }
}

void onMqttMessage(char* topic, byte* payload, unsigned int length)  {

  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i=0;i<length;i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  if (length > 0) {

    String mqtt_pubTopic_char_ptr_str;
    const char *mqtt_pubTopic_char_ptr_str_full;

    mqtt_pubTopic_char_ptr_str = pubAlarmoStateTopicStr;
    mqtt_pubTopic_char_ptr_str_full = mqtt_pubTopic_char_ptr_str.c_str();
    if (strcmp(topic,mqtt_pubTopic_char_ptr_str_full)==0){
      
      for (int cnt = 0; cnt <  length; cnt++)
        payload_string = payload_string + (char)payload[cnt];

      last_payload_string = payload_string;   //keep a copy
      
      Serial.print("payload_string: ");
      Serial.println(payload_string);

      onmqtt_timestamp = time(nullptr);

      if (strcmp(payload_string.c_str(),"armed_away")==0){
        Serial.println("Armed Away");
        armed_away_timestamp = time(nullptr);
        ticker.attach(0.6, tick); //blink slow
        disarmed = false;
        exitentry_beep = false;
      }
      else if (strcmp(payload_string.c_str(),"armed_home")==0){
        Serial.println("Armed Home");
        armed_home_timestamp = time(nullptr);
        ticker.attach(0.6, tick); //blink slow
        disarmed = false;
        exitentry_beep = false;
      }
      else if (strcmp(payload_string.c_str(),"armed_night")==0){
        Serial.println("Armed Night");
        armed_night_timestamp = time(nullptr);
        ticker.attach(0.6, tick); //blink slow
        disarmed = false;
        exitentry_beep = false;
      }
      else if (strcmp(payload_string.c_str(),"disarmed")==0){
        Serial.println("Disarmed");
        disarmed_timestamp = time(nullptr);
        ticker.detach();
        digitalWrite(BTN_LED, LOW); //keep LED off when disarmed
        disarmed = true;
        exitentry_beep = false;
        digitalWrite(BUZZER, LOW);    
      }
      else if (strcmp(payload_string.c_str(),"arming")==0){
        Serial.println("Arming");
        arming_timestamp = time(nullptr);
        ticker.attach(0.2, tick); //blink fast
        disarmed = false;
        exitentry_beep = true;
      }
      else if (strcmp(payload_string.c_str(),"pending")==0){
        Serial.println("Pending");
        pending_timestamp = time(nullptr);
        ticker.attach(0.2, tick); //blink fast
        disarmed = false;
        exitentry_beep = true;
      }
      else if (strcmp(payload_string.c_str(),"triggered")==0){
        Serial.println("Triggered");
        triggered_timestamp = time(nullptr);
        ticker.detach();
        digitalWrite(BTN_LED, HIGH); //keep LED on when triggered
        disarmed = false;
        exitentry_beep = false;
        digitalWrite(BUZZER, HIGH);    
      }
      
    }

    payload_string = "";    //clear the payload string to wait for new command

  }
}

//reinitiate NTP connection
void handleNTP() {

    UDPNTPClient.begin(2390);  // Port for NTP receive
    server.send(200, "text/plain", "Retrying NTP connection...");
    NTPRefresh();
    cNTP_Update =0;
}

void NTPRefresh()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    IPAddress timeServerIP; 
    WiFi.hostByName(ntpServer, timeServerIP); 
    Serial.println("sending NTP packet...");
    memset(packetBuffer, 0, NTP_PACKET_SIZE);
    packetBuffer[0] = 0b11100011;   // LI, Version, Mode
    packetBuffer[1] = 0;     // Stratum, or type of clock
    packetBuffer[2] = 6;     // Polling Interval
    packetBuffer[3] = 0xEC;  // Peer Clock Precision
    packetBuffer[12]  = 49;
    packetBuffer[13]  = 0x4E;
    packetBuffer[14]  = 49;
    packetBuffer[15]  = 52;
    UDPNTPClient.beginPacket(timeServerIP, 123); 
    UDPNTPClient.write(packetBuffer, NTP_PACKET_SIZE);
    UDPNTPClient.endPacket();

    delay(1000);
  
    int cb = UDPNTPClient.parsePacket();
    if (!cb) {
      Serial.println("NTP no packet yet");
    }
    else 
    {
      Serial.print("NTP packet received, length=");
      Serial.println(cb);
      UDPNTPClient.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer
      unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
      unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
      unsigned long secsSince1900 = highWord << 16 | lowWord;
      const unsigned long seventyYears = 2208988800UL;
      unsigned long epoch = secsSince1900 - seventyYears;
      UnixTimestamp = epoch;
    }
  }
}

void Second_Tick()
{
  strDateTime tempDateTime;
  cNTP_Update++;
  UnixTimestamp++;
  ConvertUnixTimeStamp(UnixTimestamp +  (atoi(ntpTimeZone)*60*60) , &tempDateTime);
  if (ntpDaylight_enabled) // Sommerzeit beachten
    if (summertime(tempDateTime.year,tempDateTime.month,tempDateTime.day,tempDateTime.hour,0))
    {
      ConvertUnixTimeStamp(UnixTimestamp +  (atoi(ntpTimeZone)*60*60) + 3600, &DateTime);
    }
    else
    {
      DateTime = tempDateTime;
    }
  else
  {
      DateTime = tempDateTime;
  }
  Refresh = true;
}

//
// Summertime calculates the daylight saving for a given date.
//
boolean summertime(int year, byte month, byte day, byte hour, byte tzHours)
// input parameters: "normal time" for year, month, day, hour and tzHours (0=UTC, 1=MEZ)
{
 if (month<3 || month>10) return false; // keine Sommerzeit in Jan, Feb, Nov, Dez
 if (month>3 && month<10) return true; // Sommerzeit in Apr, Mai, Jun, Jul, Aug, Sep
 if (month==3 && (hour + 24 * day)>=(1 + tzHours + 24*(31 - (5 * year /4 + 4) % 7)) || month==10 && (hour + 24 * day)<(1 + tzHours + 24*(31 - (5 * year /4 + 1) % 7)))
   return true;
 else
   return false;
}

void ConvertUnixTimeStamp( unsigned long TimeStamp, struct strDateTime* DateTime)
{
  uint8_t year;
  uint8_t month, monthLength;
  uint32_t time;
  unsigned long days;
  time = (uint32_t)TimeStamp;
  DateTime->second = time % 60;
  time /= 60; // now it is minutes
  DateTime->minute = time % 60;
  time /= 60; // now it is hours
  DateTime->hour = time % 24;
  time /= 24; // now it is days
  DateTime->wday = ((time + 4) % 7) + 1;  // Sunday is day 1 

  year = 0;  
  days = 0;
  while((unsigned)(days += (LEAP_YEAR(year) ? 366 : 365)) <= time) {
  year++;
  }
  DateTime->year = year; // year is offset from 1970 
  
  days -= LEAP_YEAR(year) ? 366 : 365;
  time  -= days; // now it is days in this year, starting at 0

  days=0;
  month=0;
  monthLength=0;
  for (month=0; month<12; month++) {
  if (month==1) { // february
    if (LEAP_YEAR(year)) {
    monthLength=29;
    } else {
    monthLength=28;
    }
  } else {
    monthLength = monthDays[month];
  }
  
  if (time >= monthLength) {
    time -= monthLength;
  } else {
    break;
  }
  }
  DateTime->month = month + 1;  // jan is month 1  
  DateTime->day = time + 1;     // day of month
  DateTime->year += 1970;
  
}

String GetMacasID()
{
  uint8_t mac[6];
  char macStr[18] = {0};
  WiFi.macAddress(mac);
  sprintf(macStr, "%02X%02X%02X", mac[3], mac[4], mac[5]);
  return  String(macStr);
}

String GetMacAddress()
{
  uint8_t mac[6];
  char macStr[18] = {0};
  WiFi.macAddress(mac);
  sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0],  mac[1], mac[2], mac[3], mac[4], mac[5]);
  return  String(macStr);
}

String getFormatedDuration(time_t duration) {

  String str;
  if (duration >= SECS_PER_DAY) {
    str = (duration / SECS_PER_DAY);
    str += ("d ");
    duration = duration % SECS_PER_DAY;
  }
  if (duration >= SECS_PER_HOUR) {
    str += (duration / SECS_PER_HOUR);
    str += ("h ");
    duration = duration % SECS_PER_HOUR;
  }
  if (duration >= SECS_PER_MIN) {
    str += (duration / SECS_PER_MIN);
    str += ("m ");
    duration = duration % SECS_PER_MIN;
  }
  str += (duration);
  str += ("s");
  return str;
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("Starting up...");

  pinMode(BTN_LED, OUTPUT);             //button status blue led active high
  pinMode(BUZZER, OUTPUT);              //buzzer active high
  digitalWrite(BUZZER, HIGH);           //on buzzer on boot

  butt1.setTimeout(400);                //switch timeout

  connGroup.addItem(&StaticIP_enabledParam);  
  connGroup.addItem(&ipAddressParam);
  connGroup.addItem(&gatewayParam);
  connGroup.addItem(&netmaskParam);
  
  mqttGroup.addItem(&MQTT_enabledParam);  
  mqttGroup.addItem(&mqttServerParam);
  mqttGroup.addItem(&mqttUserNameParam);
  mqttGroup.addItem(&mqttUserPasswordParam);
  
  ntpGroup.addItem(&ntpServerParam);
  ntpGroup.addItem(&ntpUpdateTimeParam);
  ntpGroup.addItem(&ntpTimeZoneParam);
  ntpGroup.addItem(&ntpDaylight_enabledParam);  

  //iotWebConf.setStatusPin(STATUS_PIN);
  //iotWebConf.setConfigPin(CONFIG_PIN);
  
  iotWebConf.addParameterGroup(&connGroup);
  iotWebConf.addParameterGroup(&mqttGroup);
  iotWebConf.addParameterGroup(&ntpGroup);

  iotWebConf.setConfigSavedCallback(&configSaved);
  iotWebConf.setFormValidator(&formValidator);
  iotWebConf.setApConnectionHandler(&connectAp);
  iotWebConf.setWifiConnectionHandler(&connectWifi);
  iotWebConf.setWifiConnectionCallback(&wifiConnected);
  iotWebConf.skipApStartup();

  // -- Initializing the configuration.
  iotWebConf.init();

  if (StaticIP_enabledParam.isChecked()) {
    if ((ipAddressValue[0] != '\0') && (gatewayValue[0] != '\0'))
    {
  StaticIP_enabled = true;
  Serial.println("IP: Static IP");
    }
  } else {
  StaticIP_enabled = false;
  Serial.println("IP: DHCP");  
  }

  if (MQTT_enabledParam.isChecked()) {
    MQTT_enabled = true;
    Serial.println("MQTT: Enabled");
    }
  else {
    MQTT_enabled = false;
    Serial.println("MQTT: Disabled");  
  }

  mqtt_auto_reconnect_timer = 10000;      //wait 10sec upon power up to connect

  if (ntpDaylight_enabledParam.isChecked()) {
    ntpDaylight_enabled = true;
    Serial.println("Daylight Saving: Yes");
    }
  else {
    ntpDaylight_enabled = false;
    Serial.println("Daylight Saving: No");  
  }
  
  // -- Set up required URL handlers on the web server.
  server.on("/", handleRoot);
  server.on("/config", []{ iotWebConf.handleConfig(); });
  server.on("/reboot", [] { rebootDevice(); });
  server.on("/ntp",handleNTP);            //reconnect ntp
  server.onNotFound([](){ iotWebConf.handleNotFound(); });

  Serial.println("Ready.");

  tkSecond.attach(1,Second_Tick);
  UDPNTPClient.begin(2390);               // Port for NTP receive

  //OTA
  MDNS.begin(host);
  httpUpdater.setup(&server, update_path, update_username, update_password);
  server.begin();
  MDNS.addService("http", "tcp", 80);
  Serial.printf("HTTPUpdateServer ready!");
  //end OTA

  uptime = time(nullptr);                 //get the startup time stamp as uptime

  mac_address_as_uid = GetMacasID();      //retrieve the unique MAC ID

  digitalWrite(BUZZER, LOW);              //off buzzer upon init
  
}

void loop() {

  String mqtt_subTopic_char_ptr_str;
  const char *mqtt_subTopic_char_ptr_str_full;
  String mqtt_pubTopic_char_ptr_str;
  const char *mqtt_pubTopic_char_ptr_str_full;
  
  // -- doLoop should be called as frequently as possible.
  iotWebConf.doLoop();

  butt1.tick();
  if (butt1.hasClicks())                  
  {  
    button_presscount = butt1.getClicks();
    Serial.println(button_presscount);    
    button_hasclicks = true;
  }
  
  ////////////////////////////////////////////////////////////////////////////////
  // Check MQTT and reconnect
  ////////////////////////////////////////////////////////////////////////////////
  if (MQTT_enabled) {
    mqttReconnect();
    mqttClient.loop();
  }

  ////////////////////////////////////////////////////////////////////////////////
  // MQTT reconnect interval loop - pause mqtt reconnect to release resources for http
  ////////////////////////////////////////////////////////////////////////////////
  unsigned long currentMillis2 = millis();
  if(currentMillis2 - previousMillis2 >=mqtt_auto_reconnect_timer) {   
    //auto reconnection MQTT
    if(!mqttClient.connected() && MQTT_enabled) //if mqtt connection failed, try again after waiting
    {
         Serial.println("Retrying MQTT connection since last timeout...");
         mqtt_retries=0;
    }
    previousMillis2 = currentMillis2;
  }

  ////////////////////////////////////////////////////////////////////////////////
  // 10msec interval loop
  ////////////////////////////////////////////////////////////////////////////////
  unsigned long currentMillis4 = millis();
  if(currentMillis4 - previousMillis4 >=interval4) {   

    //if mqtt not connected fade/glow led by pwm
    if(!mqttconnected) {
      analogWrite(BTN_LED, brightness);
      brightness = brightness + fadeAmount;
    
      if (brightness <= 0 || brightness >= 1023) 
      {
        fadeAmount = -fadeAmount;
      }
    }
    else {  //if mqtt connected, switch from pwm to GPO mode
      pinMode(BTN_LED, OUTPUT);
    }
    previousMillis4 = currentMillis4;  
  }
  ////////////////////////////////////////////////////////////////////////////////
  // 5sec interval loop
  ////////////////////////////////////////////////////////////////////////////////
  unsigned long currentMillis3 = millis();
  if(currentMillis3 - previousMillis3 >=interval3) {   

    //short led blink
    if (mqttClient.connected() && disarmed) //short blink to show online connection if disarmed state
      blinkled();

    //short beep
    if (exitentry_beep) //short beep to indicate exit entry delay
      beep();

    previousMillis3 = currentMillis3;  
  }
  ////////////////////////////////////////////////////////////////////////////////
  // 10sec interval loop
  ////////////////////////////////////////////////////////////////////////////////
  unsigned long currentMillis = millis();
  if(currentMillis - previousMillis >=interval) {   

      //Once online and connected to MQTT, subscribe to topics
    if (mqttClient.connected() && MQTT_subscribe_onpower) {
      mqtt_pubTopic_char_ptr_str = pubAlarmoStateTopicStr;
      mqtt_pubTopic_char_ptr_str_full = mqtt_pubTopic_char_ptr_str.c_str();
      mqttClient.subscribe(mqtt_pubTopic_char_ptr_str_full);
      mqttClient.loop();

      Serial.println(F("Connected to MQTT & subscribed to Topic"));
      MQTT_subscribe_onpower = false;
    }

    previousMillis = currentMillis;

  }

  ////////////////////////////////////////////////////////////////////////////////
  // Update NTP
  ////////////////////////////////////////////////////////////////////////////////
  if (atoi(ntpUpdateTime)  > 0 )
  {
    if (cNTP_Update > 15 && firstStart)
    {
      NTPRefresh();
      cNTP_Update =0;
      firstStart = false;
    }
    else if ( cNTP_Update > (atoi(ntpUpdateTime) * 60) )
    {

      NTPRefresh();
      cNTP_Update =0;
    }
  }

  ////////////////////////////////////////////////////////////////////////////////
  // If button pushed, check the click and publish a command
  ////////////////////////////////////////////////////////////////////////////////
    if(button_hasclicks) {

      switch(button_presscount)
      {
        case 1:
          if (mqttClient.connected()) {
            mqtt_subTopic_char_ptr_str = subAlarmoCommandTopicStr;
            mqtt_subTopic_char_ptr_str_full = mqtt_subTopic_char_ptr_str.c_str();
            mqttClient.publish(mqtt_subTopic_char_ptr_str_full, "arm_away");
            mqttClient.loop();  
            beep(); //confirm with a short beep
          }
        break;
        case 2:
          if (mqttClient.connected()) {
            mqtt_subTopic_char_ptr_str = subAlarmoCommandTopicStr;
            mqtt_subTopic_char_ptr_str_full = mqtt_subTopic_char_ptr_str.c_str();
            mqttClient.publish(mqtt_subTopic_char_ptr_str_full, "arm_night");
            mqttClient.loop();  
            beep(); //confirm with a short beep
          } 
        break;
        case 3:
          if (mqttClient.connected()) {
            mqtt_subTopic_char_ptr_str = subAlarmoCommandTopicStr;
            mqtt_subTopic_char_ptr_str_full = mqtt_subTopic_char_ptr_str.c_str();
            mqttClient.publish(mqtt_subTopic_char_ptr_str_full, "arm_home");
            mqttClient.loop();  
            beep(); //confirm with a short beep
          } 
        break;
        case 4:
          if (mqttClient.connected()) {
            mqtt_subTopic_char_ptr_str = subAlarmoCommandTopicStr;
            mqtt_subTopic_char_ptr_str_full = mqtt_subTopic_char_ptr_str.c_str();
            mqttClient.publish(mqtt_subTopic_char_ptr_str_full, "disarm");
            mqttClient.loop();  
            beep(); //confirm with a short beep
          } 
        break;
        default:
        break;
      }
      button_hasclicks = false;
      button_timestamp = time(nullptr);
    }
  
}
