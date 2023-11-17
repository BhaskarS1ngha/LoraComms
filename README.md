# Off Grid Lora based text communication device
An ESP8266 based device that allows you to send and receive text messages over LoRa. The device is designed to be used in areas without cellular coverage or internet access. It uses the AiThinker RA-01 Lora Module as the transponder.
\
The ESP8266 sets up a WiFi access point that you can connect to with your phone or computer. Once connected you can send and receive text messages to and from other devices using LoRa. The devices must be on the same LoRa frequency and must be running the same firmware with minor changes to the `ssid` and `id` variables