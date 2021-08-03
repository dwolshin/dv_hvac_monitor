     
install python, then run certs-from-mozilla.py to regenerate certs.ar data file. 

change the default flash layout to: allow 1MB program space, 1MB SPIFFS and 1MB for OTA. In Arduino IDE 1.8.10 with ESP8266 Core v2.6.3 the option is "4MB (FS:1MB, OTA:~1019KB)".
    
add an IDE extension to write the cert file to flash  https://github.com/esp8266/arduino-esp8266fs-plugin/

Make sure you use one of the supported versions of Arduino IDE and have ESP8266 core installed.
Download the tool archive from releases page.
In your Arduino sketchbook directory, create tools directory if it doesn't exist yet. You can find the location of your sketchbook directory in the Arduino IDE at File > Preferences > Sketchbook location.
Unpack the tool into tools directory (the path will look like <sketchbook directory>/tools/ESP8266FS/tool/esp8266fs.jar).
Restart Arduino IDE.
On OS X and Linux based OSs create the tools directory in ~/Documents/Arduino/ and unpack the files there

Usage
Open a sketch (or create a new one and save it).
Go to sketch directory (choose Sketch > Show Sketch Folder).
Create a directory named data and any files you want in the file system there.
Make sure you have selected a board, port, and closed Serial Monitor.
Select Tools > ESP8266 Sketch Data Upload menu item. This should start uploading the files into ESP8266 flash file system. When done, IDE status bar will display SPIFFS Image Uploaded message. Might take a few minutes for large file system sizes.