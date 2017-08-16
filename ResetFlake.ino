// 
// ResetFlake - it resets your flaky internet connectin.
//
// Written for ESP8266 (WeMos D1 Mini) 
// with Relay Shield, or anything switched via pin D1.
//

#include <ArduinoJson.h>
#include <FS.h>
#include <Time.h>
#include <TimeLib.h>

#include <ESP8266WiFi.h> // default contrib v1.0.0
// #include <WiFiClient.h>
#include <ESP8266WebServer.h>

// See https://github.com/dancol90/ESP8266Ping
#include <ESP8266Ping.h>


//
// -- Configuration
//

const char * ssid = "mywifi";      // overridden in "/config.json"
const char * passwd = "password";

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
// -- Advanced configuration
//

// 
// Define this to make all the timeouts shorter for debugging.
//
// #define IMPATIENT_DEBUG_MODE 1

// Default interval at which to ping servers
#ifdef IMPATIENT_DEBUG_MODE
#define DEFAULT_POLL_S (15)
#else
#define DEFAULT_POLL_S (90)
#endif

//
// Time to wait after a reset
// Initial wait time is MS + INCREMENT
//
// CenturyLink's docs recommend waiting, what, 2 minutes?
//
// VDSL2 takes literally forever (OK, really about 4 minutes) to renegotiate and
// authenticate.
//
#define RETRY_WAIT_S (240)

#ifdef IMPATIENT_DEBUG_MODE
#define RETRY_WAIT_INCREMENT_S (45)
#else
#define RETRY_WAIT_INCREMENT_S (5)
#endif

// Time to stay off when power cycling
#define STAY_OFF_MS (5000)

// Additional time to stay off on subsequent retries
#define STAY_OFF_INCREMENT (1000)

//
// Ping config: A reset takes >90s, and then everything downstream
// of the modem has to reconfigure. Sometimes the network recovers.
// How long do we keep trying a failed ping before we give up and
// reset the internets? Let's be very patient, and wait for at least
// the length of a pop song.
//
#define PING_RETRY_MS (180000)

//
// Time to wait between pings, when the first one fails
//
#define PING_DELAY_MS (2000)

//
// -- Internal state
//

///
/// Internal reset counter -- this does not keep counting past 5-ish.
///
/// How many consecutive times have we tried a reset and failed?
/// 
/// (Should be less than 5 or so. Past that we stop counting and therefore
/// also stop increasing wait times etc.)
///
int resetCount = 0;

#define HISTORY_LENGTH (6)

typedef enum SystemStatusEnum
{
  STATUS_UNKNOWN, // 0, not used
  STATUS_OK,
  STATUS_DOWN,
  STATUS_RESETTING,
  STATUS_RESET_WAIT
} SystemStatus;

///
/// Stats
///
class Stats
{
  class OutageHistoryElement
  {
  public:
    time_t start;
    unsigned long length;
    OutageHistoryElement()
    {
      start = now();
      length = 0;
    }
    
    const OutageHistoryElement& operator=(const OutageHistoryElement & other)
    {
      start = other.start;
      length = other.length;
      return *this;
    }
  };
  
private:
  ///
  /// Current status
  ///
  SystemStatus _systemStatus;
  
  time_t _programStartTime;
  
  ///
  /// Time of the last successful ping
  ///
  time_t _lastSuccessTime;

  ///
  /// Time the current outage started, or 0 if everything is fine
  ///
  time_t _outageStartTime;
  
  ///
  /// Time the power switch was last cycled
  ///
  time_t _lastResetTime;

  ///
  /// Count of network resets since boot. Yeah it'll eventually wrap, but
  /// if it takes >90s to do a reset, how long will that take?
  ///
  /// There may be multiple resets during a long outage so this is not the most
  /// useful statistic.
  ///
  unsigned long _resetCount;
  
  ///
  /// Exponentially weighted average ping time in ms.
  /// We don't expect to see ping times > 1s, mostly because
  /// ESP8266Ping times out in 1s. Maybe this is configurable?
  ///
  int _avgPing;

  ///
  /// ms
  ///
  int _maxPing;
  
  ///
  /// Last ping time in ms
  ///
  int _ping;

  ///
  /// Longest outage we've seen, in seconds
  ///
  unsigned long _maxOutageSec;

  ///
  /// Exponentially weighted average outage time
  ///
  unsigned long _avgOutageSec;
  
  OutageHistoryElement _outageHistory[HISTORY_LENGTH];
  
public:  
  Stats()
  {
    _systemStatus = STATUS_UNKNOWN;
    _programStartTime = now();
    _lastResetTime = _programStartTime; // as far as we know.
    _lastSuccessTime = now(); // set to now because also start of outage if currently down
    _outageStartTime = 0; // no current outage
    _resetCount = 0;
    _avgPing = 1;
    _maxPing = 1;
    _ping = 0;
    _maxOutageSec = 0;
    _avgOutageSec = 0;
  }

  void printToSerial()
  {
    Serial.print(asJson());
  }

  ///
  /// Get current stats as json
  ///
  String asJson()
  {
    String json = "{\n";
    
    json += "\"status\":";
    switch (_systemStatus)
    {
    case STATUS_OK:         json += "\"OK\"";         break;
    case STATUS_DOWN:       json += "\"DOWN\"";       break;
    case STATUS_RESETTING:  json += "\"RESETTING\"";  break;
    case STATUS_RESET_WAIT: json += "\"RESET_WAIT\""; break;
    
    default:
    case STATUS_UNKNOWN:
      json += "\"UNKNOWN\"";
      break;
    }
    json += ",\n";

    json += "\"uptime\":";
    json += getSystemUptimeSec();
    json += ",\n";

    json += "\"outageDuration\":";
    json += getOutageDuration();
    json += ",\n";

    json += "\"linkUp\":";
    json += getUptimeSec();
    json += ",\n";

    json += "\"outageMax\":";
    json += getMaxOutageSec();
    json += ",\n";

    json += "\"outageAvg\":";
    json += getAvgOutageSec();
    json += ",\n";

    json += "\"resetCount\":";
    json += getResetCount();
    json += ",\n";
    
    json += "\"ping\":";
    json += getPingMs();
    json += ",\n";
    
    json += "\"pingAvg\":";
    json += getAvgPingMs();
    json += ",\n";
    
    json += "\"pingMax\":";
    json += getMaxPingMs();
    json += ",\n";
    
    // Outage history!
    json += "\"outageHistory\": [\n";
    int i = 0;
    while (true)
    {
      OutageHistoryElement el = _outageHistory[i];
      json += " {\n";
      json += "  \"ago\":";
      json += now() - el.start;
      json += ",\n";
      json += "  \"length\":";
      json += el.length;
      json += "\n";
      
      if (i < (HISTORY_LENGTH - 1))
      {
        json += " },\n";
      }
      else
      {
        json += " }\n";
        break;
      }
      i++;
    }
    json += "]\n";

    json += "\n}\n";

    return json;
  }
  
  ///
  /// Current outage duration or -1 if no current outage...
  ///
  long getOutageDuration()
  {
    if (_outageStartTime == 0)
      return -1; // no outage
    
    return now() - _outageStartTime;
  }

  long getSystemUptimeSec()
  {
    return now() - _programStartTime;
  }

  long getUptimeSec()
  {
    if (_systemStatus != STATUS_OK)
      return 0;  // we're down *now*
      
    return now() - _lastResetTime;
  }
  
  unsigned long getMaxOutageSec()
  {
    return _maxOutageSec;
  }
    
  unsigned long getAvgOutageSec()
  {
    return _avgOutageSec;
  }

  unsigned long getResetCount()
  {
    return _resetCount;
  }
  
  int getPingMs()
  {
    return _ping;
  }

  int getAvgPingMs()
  {
    return _avgPing;
  }
    
  int getMaxPingMs()
  {
    return _maxPing;
  }

  ///
  /// Register that a reset occurred
  ///
  void logReset()
  {
    ++ _resetCount;
    _systemStatus = STATUS_RESETTING;
    
    // Log stats about uptime
    time_t nowTime = now();

    // TODO: collect historical uptime stats?
    // long uptime = nowTime - _lastResetTime;
    
    // Assume here that the outage starts at the last successful ping
    _outageStartTime = _lastSuccessTime;
    _lastResetTime = nowTime;
  }
  
  ///
  /// After resetting, we wait a bit before we try to ping again
  ///
  void logResetWait()
  {
    _systemStatus = STATUS_RESET_WAIT;
  }
  
  void logPingFail()
  {
    // We don't ping while resetting, so we can safely assume that the reset
    // is over:
    _systemStatus = STATUS_DOWN;
    
    // TODO: report ping fail time for impatient user (me!)
  }

  ///
  /// Called when an outage ends (with a successful ping)
  /// to update max outage statistics.
  /// Call logOutageStart() to start an outage.
  ///
  /// If there is not currently an outage, does nothing.
  ///
  void logOutageEnd(time_t when)
  {
    _systemStatus = STATUS_OK;
    
    if (0 == _outageStartTime) // no current outage?
      return;

    long outageDuration = when - _outageStartTime;
    
    Serial.print("Logging end of outage, duration: ");
    Serial.println(outageDuration);

    if (outageDuration <= 0)
    {
      Serial.println("outage duration < 1s, is that possible?");
    }

    if ((unsigned long)outageDuration > _maxOutageSec)
      _maxOutageSec = outageDuration;

    _avgOutageSec = _updateMovingAverage(_avgOutageSec, outageDuration);
    
    // Push all history back one here
    
    for (int i = 0; i < (HISTORY_LENGTH - 1); i++)
    {
      _outageHistory[i] = _outageHistory[i + 1];
    }
    OutageHistoryElement & hist = _outageHistory[HISTORY_LENGTH - 1];
    hist.start = _outageStartTime;
    hist.length = outageDuration;
    
    _outageStartTime = 0; // Reset current outage status
  }

  ///
  /// Logs the ping time, and also marks the end of an outage
  /// if we currently have one.
  ///
  void logPingTime(int pingMs)
  {
    _ping = pingMs;
    _lastSuccessTime = now();
    
    logOutageEnd(_lastSuccessTime);

    // We expect avgPing to be < 1000ms so this is safe?
    _avgPing = _updateMovingAverage(_avgPing, pingMs);

    if (pingMs > _maxPing)
      _maxPing = pingMs;
  }

  //
  // -- Helpers
  //
protected:
  ///
  /// Approximates the exponential moving average value, using 
  /// integer math.
  ///
  /// \return new average value
  ///
  long _updateMovingAverage(long oldVal, long newVal)
  {
    // Not sure how accurate time-decayed math is when rounded to
    // the nearest int every time. :)
    
    long avg = (oldVal * 950) + (newVal * 50); // 95% old, 5% new, * 1000
    avg = (avg + 500) / 1000;
    return avg;
  }

  ///
  /// Print uptime in seconds to serial
  ///
  void _printUptime(long uptime)
  {
    int days = uptime / 86400L;
    int hours = hour((time_t)uptime);
    int minutes = minute((time_t)uptime);
    int seconds = second((time_t)uptime);
    Serial.print(days);
    Serial.print("d ");
    Serial.print(hours);
    Serial.print(":");
    Serial.print(minutes);
    Serial.print(":");
    Serial.print(seconds);
  }
};

Stats stats;
ESP8266WebServer server(80);

void setup() 
{
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(relayPin, OUTPUT);

  Serial.begin(115200);
  
  while(!Serial) {} // Wait for console log

  Serial.println("Hello, world!");

  // The ESP8266 LED HIGH = off
  digitalWrite(LED_BUILTIN, HIGH);
  digitalWrite(relayPin, LOW);

  // Serve web pages and get config from the filesystem.
  if (!SPIFFS.begin()) {
    Serial.println("SPIFFS init failed");
  }

  // Read json config if found
  File configFile = SPIFFS.open("/config.json", "r");
  if (!configFile)
  {
    Serial.println("ERROR: config.json not found; using defaults");
  }
  else
  {
    // Warning: parse() copies the entire file into memory.
    DynamicJsonBuffer jsonBuffer; // or Static<> or Dynamic
    JsonObject & root =  jsonBuffer.parse(configFile);
    configFile.close();
    
    if (!root.success())
    {
      Serial.println("Could not parse config.json; using defaults");
    }
    else
    {
      // root.printTo(Serial);
      ssid = root["ssid"];
      passwd = root["password"];
    }
  }

  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA); // defaults to BOTH, AP + Station, which is not what we want
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

  server.on("/hello", getHello);
  server.on("/stats", getStats);
  server.on("/", getRedirectToIndex);

  server.serveStatic("/", SPIFFS, "/web/"); // "max-age=86400"
  server.begin();  
}

void loop() 
{
  int sleepytime = wakeUp();
  Serial.print("zzz... (");
  Serial.print(sleepytime);
  Serial.println("s)");
  delayAndServe(sleepytime * 1000);
}

// -- Web handlers --

void getRedirectToIndex()
{
  server.sendHeader("Location", String("/index.html"), true);
  server.send( 302, "text/plain", "");
}

void getHello()
{
  server.send( 200, "text/plain", String("Hello, world"));
}

void getStats()
{
  server.send( 200, "application/json", stats.asJson());
}

// -- Main ping program --

int wakeUp()
{
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

  stats.printToSerial();
  
  if (resetCount <= 0)
    return DEFAULT_POLL_S;
  else
    return RETRY_WAIT_S + (resetCount * RETRY_WAIT_INCREMENT_S);
}

bool doPing()
{
  // The net is up if we can ping our upstream IP (or something nearby), 
  // and also DNS is working. 
  //
  // Reference config: Centurylink's DNS sometimes fails until we
  // reset the modem, even though IP routing still works. :(

  // If we can't get both our pings within a set time, it failed
  unsigned long startTime = millis();

  if (!persistentPing(pingIp, startTime))
    return false;
    
  // And now by hostname

  Serial.print("Ping host: ");
  Serial.println(pingHost);
  
  IPAddress byNameIp;
  if (!WiFi.hostByName(pingHost, byNameIp))
    return false;

  if (!persistentPing(byNameIp, startTime))
    return false;
  
  return true;
}

///
/// Ping the address repeatedly until one successful ping happens
/// or we reach PING_RETRY_MS, assuming we started at time startMillis
///
bool persistentPing(IPAddress dest, long startMillis)
{
  Serial.print("Pinging ");
  Serial.print(dest);
  Serial.print("..");

  while (true)
  {
    if (Ping.ping(dest, 1)) 
    {
      int avg_time_ms = Ping.averageTime();
      Serial.print(avg_time_ms);
      Serial.println("ms");
      stats.logPingTime(avg_time_ms);
      
      return true;
    }

    stats.logPingFail(); // If we got here, ping failed. Noooo!
    
    if ((millis() - startMillis) > PING_RETRY_MS) // timed out?
    {
      Serial.println("fail. :(");
      return false;
    }
    else
    {
      Serial.print(".");
      delayAndServe(1000);
    }
  }
}

void doReset()
{
  stats.logReset();
  
  Serial.print("Resetting...");
  digitalWrite(relayPin, HIGH); // energize to disconnect: using normally-closed contacts
  delayAndServe(STAY_OFF_MS + (resetCount * STAY_OFF_INCREMENT));
  digitalWrite(relayPin, LOW);
  Serial.println("done");
  
  stats.logResetWait(); // Now we wait, and wait, and wait.
}

///
/// Delay, but also serve web clients
///
void delayAndServe(long milliseconds)
{
  long ticks = milliseconds / 125L;
  int leftover = milliseconds % 125;
  
  while (ticks > 0)
  {
    --ticks;
    delay(125);
    server.handleClient();
  }
  delay(leftover);
}
