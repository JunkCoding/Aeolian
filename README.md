# axrgb
## Network synchronised RGB LED display.

This project allows synchronisation of many disconnected light strings to a single master device. 

The interface allows an end user to view tasks, amend wireless settings and manage the firmware. 

The lights have several control modes, allowing them to be forced on/off at fixed times or turned on/off when, according to the local sunrise/sunset, it is dark.

# Before compiling
On windows, configure environment variables for connecting your ESP32 to your IOT network. I have included an example script in the tools directory.
Essentially, you will need to run:<br />
setx WIFI_SSID "&lt;Your SSID"&gt;<br>
setx WIFI_PASS "&lt;password&gt;"<br />
setx MQTT_SERVER "mqtt://&lt;mqtt server&gt;/"<br />
setx MQTT_USER "&lt;mqtt username&gt;"<br />
setx MQTT_PASS "&lt;mqtt password&gt;"<br />
<br />On Unix type systems, you can add then environment variables with:<br>
export WIFI_SSID="&lt;Your SSID"&gt;<br>
export WIFI_PASS="&lt;password&gt;"<br />
export MQTT_SERVER="mqtt://&lt;mqtt server&gt;/"<br />
export MQTT_USER="&lt;mqtt username&gt;"<br />
export MQTT_PASS="&lt;mqtt password&gt;"<br />

These will be picked up during the compilation and will allow the device to connect without further configuration required.<br />

# Working:
A unit can be a master or slave (two masters is possible, but unknown results).
Synchronisation of patterns and palettes.
Some pattern idiosynchrasies are synced.
Lights on/off after sunrise/sunset according to locale.
If master is not available, lights will run autonomously.
Remotely viewing logs.
Remotely viewing tasks CPU/Memory usage.
Firmware management.

# Not Working:
Web interface to add/remove themes (modifying existing works but is not saved)
Web interface to modify available patterns and minor tweaks (modifying existing works but is not saved)
Web interface to modify schedule (nothing works, was a placeholder to help inspire ideas)
Sending commands to the console does nothing.

# Notes.
I have used some FastLED functions, "as is", and modified others. I had a reason for it at the time, but no idea what that was now (I think it was for allowing better synchronisation). I am pretty sure espfs could be replaced, the one included has been modified from the original. A lot of the code started off as something else and was slowly manipulated into being what it is today, so I take full responsibility for any, "what the fuck!?", exclamations anyone might have browsing the code.

# Screenshots.
# Console
![Console](https://github.com/JunkCoding/axrgb/blob/main/screenshots/console1.png)

# Tasks (running processes)
![Tasks](https://github.com/JunkCoding/axrgb/blob/main/screenshots/tasks.png)
# Wireless configuration
![WiFi](https://github.com/JunkCoding/axrgb/blob/main/screenshots/WiFi.png)

# Main Status Screen
![Status](https://github.com/JunkCoding/axrgb/blob/main/screenshots/status.png)
# Zones
There are currently 6 zones, 4 of which are used by the light display. Essentially, you can split the lights into four zones, such as the four sides of a building. Only a few, maybe two patterns use zones at the moment; 'blocky' and 'rachels'. Rachels is broken and is meant to display a different pattern in each zone whilst 'blocky' displays a different colour in each zone and rotates them around.
