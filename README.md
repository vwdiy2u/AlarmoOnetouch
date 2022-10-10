# Alarmo OneTouch
This is a simple to use single touch button with status indicator to control the Home Assistant Alarmo Alarm System without using a phone/tablet touch screen.

Alarmo works great with Home Assistant Alarm Panel Card, but it needs to be displayed in a Smartphone or Tablet where Arming button options are made available as well as the keypad to disarm the alarm system. It may not be feasible sometimes (in my case) to place the touch screen devices next to the doorway. This has led to possible false alarm triggering due to opening the door without knowing that it was not disarmed. With Alarmo OneTouch, the illuminated button will indicate the arming status, and with a touch of the button, Alarmo can also be controlled with ease.

### Single button to arm and disarm Alarmo

The LED on the button itself indicates the current status of the alarm e.g Arming, Armed Away / Armed Night / Armed Home, Pending, Disarmed, Triggered or Offline. A buzzer produces short beeps during Arming & Pending, and continuous beep when the alarm status is Triggered.

Blink the LED & beeps according to status of alarmo/state:
* very fast alternate blink - MQTT connection error status
* fast blink - Pending or Arming status
* short beep every 5s if exit / entry delay (Arming/Pending) status
* slow blink – Armed status (armed_away / armed_home / armed_night)
* LED off – Disarmed status (single fast blink every 5s if connected)
* LED on – Triggered status
* short beep if valid command sent

The button reads user clicks to determine the number of press count. The press count is then being mapped with the command to be sent.
* 1 press - Arm Away 
* 2 presses - Arm Night 
* 3 presses - Arm Home 
* 4 presses - Disarm


### Wireless connection to Alarmo

On power up, the device will look for the preset AP SSID to connect. If it failed to connect, it will enable AP mode and a host (Laptop or Smartphone) can then connect to the device and configure its connection parameters in its web configuration portal.
Alarmo OneTouch connects to Home Assistant through Wi-Fi and using MQTT. MQTT needs to be enabled in Alarmo General Configuration. The device subscribe and listens to the State topic.

On a valid button action, it publishes the specific commands to the Command topic and indicate successful transmission with a short beep. The Command payload used are as per default setting. To make it simple, code is not required to be sent together with the command.


### Automatic network reconnection
The device will auto reconnect if it is disconnected from the AP or MQTT server. If the device is moved away permanently from the earlier connected AP, the device can be setup again to connect to the new Access Point.
