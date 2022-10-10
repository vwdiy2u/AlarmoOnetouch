# AlarmoOnetouch
This is a simple to use single touch button with status indicator to control the Home Assistant Alarmo Alarm System without using a phone/tablet touch screen.

Alarmo works great with Home Assistant Alarm Panel Card, but it needs to be displayed on a SmartPhone or Tablet where Arming button options are made available as well as the keypad to disarm the alarm system. It may not be feasible sometimes (in my case) to place the touch screen devices next to the door. This has lead to possible false alarm tiggerring due to opening the door without knowing that it was being disarmed earlier. With Alarmo Onetouch, the illuminated button will indicate the arming status, and with a touch of the button, Alarmo also be controlled.

### Single button to arm and disarm Alarmo

The LED on the button itself indicates the current status of the alarm e.g Arming, Armed Away / Armed Night / Armed Home, Pending, Disarmed. Triggered or Network disconnected.
A buzzer produces short beeps during Arming & Pending, and continuous beep when triggered

Fast blink - Network disconnected

Arming / Pending - Short beep with...

Armed Away / Armed Night / Armed Home - Slow blink

Disarmed & Online - Short blink every 5 seconds

Triggered - 

### Wireless connection to Alarmo

On powerup, the device will look to the preset AP SSID to connect. If it failed to connect, it will enable AP mode and a host (laptop or Smartphone)

Alarmo Onetouch connect to Home Assistant through wifi and using MQTT. MQTT needs to be enabled in Alarmo General Configuration.
The device subscribe and listens to the State topic. 

On a valid button actions, it triggeres transmission of the specific commands to the Command topic and indicated with a short beep. The Command payload used are as per default setting. To make it simple, code is not reqired to be sent togather with the command.

1 press - Arm Away
2 press - Arm Night
3 press - Arm Home
4 press - Disarm

