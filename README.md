## ResetFlake

An Arduion Sketch that monitor WiFi and resets a modem or router when 
the internet is not reachable. For the WeMos D1 Mini or compatible board. 

Should be fairly portable to any ESP8266 setup, with some configuration.
Note that I am using the NC (normally closed) pins on a relay attached
to D1. You could just as easily use a MOSFET, reverse the HIGH/LOW state 
of D1, and switch the ground lead of the DC power. Etc. Or whatever.

### Disclaimer

I'm switching the +12V supply. If you electrocute yourself trying to 
switch mains power, don't come crying to me.

### Requires

  * ESP8266Wifi (contrib 1.0.0 tested)
  * ESP8266WebServer (1.0.0 tested)
  * ESP8266Ping (https://github.com/dancol90/ESP8266Ping)
  * Arduino ESP8266 filesystem uploader plugin, or any other SPIFFS data writing method
  * Time by Michael Margolis (1.5.0 tested)
  
### Getting Started

#### 1. Edit configuration

Configure WiFi. You will need to set a few configuration variables at the top of the file before
it will work. Specify your WiFi network and password, unless
your wifi password is "password".

Choose an IP and hostname to ping. I recommend choosing hosts that that are directly
upstream on your ISP. (I like to use the primary DNS resolver.) Or you can use
the old standby google, but then your modem will reset if the route to google
goes down, even if the rest of the internet is working fine. So don't do that.

#### 2. Upload the SPIFFS image

Stats are retrieved from an web page. I'm not sure what happens if you leave your
memory uninitialized, but in my experience SPIFFS is unhappy if you skip this step. :)

I use the [ESP8266 Sketch Data Upload](https://github.com/esp8266/arduino-esp8266fs-plugin)
plugin to upload the static web files.

#### 3. Compile/upload the sketch and run it.

I assume you know how to do this. :)

#### 4. Browse to stats

Find the IP address of the device and open it in a browser window for some simple
stats. Sometimes you can find this in your router, or you can look at the serial
monitor output. Or hack the script to use a static IP address. We're into the
"do it your way" part of things now.

Stats are available as the default page. REST apps can browse to http://<device>/stats
to get all stats as JSON. There is also a handy "Hello, world." rest service
located at "/hello".

### License

Licensed under the WTFPL.
