A program to monitor wifi and reset a modem or router when 
the internet is not reachable, for the WeMos D1 Mini or compatible board. 

Should be fairly portable to any ESP8266 setup, with some configuration.
Note that I am using the NC (normally closed) pins on a relay attached
to D1. You could just as easily use a MOSFET, reverse the HIGH/LOW state 
of D1, and switch the ground lead of the DC power. Etc. Or whatever.

I'm switching the +12V supply. If you electrocute yourself trying to 
switch mains power, don't come crying to me.

Requires
  * ESP8266Wifi (tested against contrib 1.0.0 release)
  * ESP8266Ping (https://github.com/dancol90/ESP8266Ping)

The code is fairly self explanatory. You will need to edit the code to specify
your WiFi network and password, and choose an IP and hostname that are close
upstream on your ISP. (I like to use the primary DNS resolver.) Or you can use
the old standby google, but then your modem will reset if the route to google
goes down, even if the rest of the internet is working fine. So don't do that.

Licensed under the WTFPL.
