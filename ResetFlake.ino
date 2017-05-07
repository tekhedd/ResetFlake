
// 
// The internet-down-reset module
//
// Written for ESP8266 (WeMos D1 Mini) 
// with Relay Shield, or anything switched via pin D1.
//

#include <ESP8266WiFi.h> // default contrib v1.0.0

// See https://github.com/dancol90/ESP8266Ping
#include <ESP8266Ping.h>

//
// -- Configuration
//
// TODO: web config 

const char * ssid = "bitrotIOS";
const char * passwd = "goo12345";

// Use both host name and IP to check that dns and routing are working. 
// Do not 100% trust hostname-based ping, because the default router can 
// return its own IP for "magic autoconfig", resulting in a successful ping
// but not to the host you think you reached. :o
//
// That said, we *do* need to confirm that DNS is working, so we *also*
// do a host-based ping. You can't win!
//
// I use CenturyLinkTM's DNS server name/IP, because am only interested in
// resetting the fragile C1000A modem when my upstream link dies.
// 
const char * pingHost = "resolver1.centurylink.net";
IPAddress pingIp(205, 171, 3, 26);

int relayPin = D1;

// 
// Define this to make all the timeouts shorter for debugging.
//
#define IMPATIENT_DEBUG_MODE 1

// Default interval at which to ping servers
#ifdef IMPATIENT_DEBUG_MODE
#define DEFAULT_POLL_MS (15000)
#else
#define DEFAULT_POLL_MS (60000)
#endif

//
// Time to wait after a reset
// Initial wait time is MS + INCREMENT
//
// My CenturyLinkTM C1000A + Netgear R7000 combo is
// not ready after 60s--typically takes about 75. 
// Wait 105s (75+30) to have 30s of headroom.
//
#define RETRY_WAIT_MS (75000)

#ifdef IMPATIENT_DEBUG_MODE
#define RETRY_WAIT_INCREMENT (30000)
#else
#define RETRY_WAIT_INCREMENT (5000)
#endif

// Time to stay off when power cycling
#define STAY_OFF_MS (5000)

// Additional time to stay off on subsequent retries
#define STAY_OFF_INCREMENT (1000)

//
// -- Internal state
//

// How many consecutive times have we tried a reset and failed?
int resetCount = 0;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(relayPin, OUTPUT);

  Serial.begin(115200);
  
  while(!Serial) {} // Wait for console log

  // The ESP8266 LED HIGH = off
  digitalWrite(LED_BUILTIN, HIGH);
  digitalWrite(relayPin, LOW);

  Serial.print("Connecting to ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, passwd);

  // Wait for WIFI and blink while attempting
  int state = LOW;
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
    if (state == HIGH)
      state = LOW;
    else
      state = HIGH;
      
    digitalWrite(LED_BUILTIN, state);
  }
  Serial.println("");
  Serial.print("WiFi connected: ");
  Serial.println(WiFi.localIP());
  digitalWrite(LED_BUILTIN, HIGH);
}

void loop() {
  // Turn the led on while checking, and turn it off on success.
  // If the light stays on, we are in trouble. :o
  
  digitalWrite(LED_BUILTIN, LOW);
  bool pingStatus = doPing();
  if (pingStatus)
  {
    digitalWrite(LED_BUILTIN, HIGH); // success, turn off LED!
    resetCount = 0;
  }
  else
  {
    ++ resetCount;
    if (resetCount > 5) // limit max poll delay if we're offline
      resetCount = 5;
      
    doReset();
  }

  int sleepytime;
  if (resetCount <= 0)
    sleepytime = DEFAULT_POLL_MS;
  else
    sleepytime = RETRY_WAIT_MS + (resetCount * 30000);
    
  Serial.print("zzz... (");
  Serial.print(sleepytime / 1000);
  Serial.print("s)");
  delay(sleepytime);
  Serial.println("wakeup!");
}

bool doPing()
{
  // The net is up if we can ping our upstream IP (or something nearby), 
  // and also DNS is working. Centurylink's DNS sometimes fails until we
  // reset the modem, even though IP routing still works. :(

  Serial.print("Pinging host: ");
  Serial.print(pingHost);
  Serial.print("...");

  if (!Ping.ping(pingHost, 1)) {
    Serial.println("fail. :(");
    return false;
  }
  int avg_time_ms = Ping.averageTime();
  Serial.print(avg_time_ms);
  Serial.println("ms");
  
  Serial.print("Pinging ip: ");
  Serial.print(pingIp);
  Serial.print("...");
  if (!Ping.ping(pingIp, 1)) {
    Serial.println("fail. :(");
    return false;
  }
  avg_time_ms = Ping.averageTime();
  Serial.print(avg_time_ms);
  Serial.println("ms");
  
  return true;
}

void doReset()
{
  Serial.print("Resetting...");
  digitalWrite(relayPin, HIGH); // energize to disconnect: using normally-closed contacts
  delay(STAY_OFF_MS + (resetCount * STAY_OFF_INCREMENT));
  digitalWrite(relayPin, LOW);
  Serial.println("done");
}

