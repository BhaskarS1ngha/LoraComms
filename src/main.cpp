#include <Arduino.h>
#include <Ra01S.h>

#include <ESP8266WiFi.h>
#include <string.h>
#include <LittleFS.h>
#include <ESP8266mDNS.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <EEPROM.h>
#include <DNSServer.h>

#define RF_FREQUENCY 433000000 // Hz  center frequency
// #define RF_FREQUENCY     866000000 // Hz  center frequency
//  #define RF_FREQUENCY    915000000 // Hz  center frequency
#define TX_OUTPUT_POWER 2       // dBm tx output power (0-25)
#define LORA_BANDWIDTH 4        // bandwidth
                                // 2: 31.25Khz
                                // 3: 62.5Khz
                                // 4: 125Khz
                                // 5: 250KHZ
                                // 6: 500Khz
#define LORA_SPREADING_FACTOR 7 // spreading factor [SF5..SF12]
#define LORA_CODINGRATE 1       // [1: 4/5,
                                //  2: 4/6,
                                //  3: 4/7,
                                //  4: 4/8]

#define LORA_PREAMBLE_LENGTH 8 // Same for Tx and Rx
#define LORA_PAYLOADLENGTH 0   // 0: Variable length packet (explicit header)
                               // 1..255  Fixed length packet (implicit header)
const char *ssid = "node2";    // defines the node number
DNSServer dnsServer;
AsyncWebServer server(80);
FSInfo fs_info;
File f;

String id = "User2"; // defines username
int i;

SX126x lora(15, 0, 16, 4, 2);

void updateDb(char *name, char *text)
{
  char *resultString = (char *)malloc(strlen(name) + strlen(text) + 4);

  // combine strings
  sprintf(resultString, "%s: %s|", name, text);

  // append string to file
  f = LittleFS.open("/messages.txt", "a");
  f.write(resultString, strlen(resultString));
  f.close();
}

void receiveWrapper(uint8_t *dataP, int len)
{
  uint8_t rxLen = lora.Receive(dataP, 255);
  char rcvBuff[len] = {'\0'};
  char rcvUser[5] = {'\0'};
  // Serial.println((char *)dataP);
  if (rxLen > 0)
  {
    for (int i = 0; i < 5; i++)
    {
      rcvUser[i] = dataP[i];
      Serial.print(rcvUser[i]);
    }
    Serial.print(": ");
    for (int i = 5; i < rxLen; i++)
    {
      if (dataP[i] > 0x19 && dataP[i] < 0x7F)
      {
        rcvBuff[i - 5] = dataP[i];
        Serial.print(rcvBuff[i - 5]);
      }
    }
    updateDb(rcvUser, rcvBuff);
  }
}

void txWrapper(String buff, uint8_t mode)
{
  String tmp = String(id);
  char cid[6];
  id.toCharArray(cid, 6);
  tmp.concat(buff);
  int len = tmp.length() + 1;
  int tlen = buff.length() + 1;
  char data[len];
  char text[tlen];
  buff.toCharArray(text, tlen);
  tmp.toCharArray(data, len);
  uint8_t *txData = (uint8_t *)data;
  Serial.print(cid);
  Serial.print(": ");
  Serial.println(buff);

  // Serial.println(data);
  if (lora.Send(txData, len, SX126x_TXMODE_SYNC))
  {
    Serial.println("Send success");
    updateDb(cid, text);
  }
  else
  {
    Serial.println("Send fail");
  }
}

void setup()
{
  // server
  IPAddress apIP(192, 168, 0, 1);
  IPAddress netMask(255, 255, 255, 0);

  // initialize wifi-ap
  WiFi.mode(WIFI_STA);
  WiFi.softAPConfig(apIP, apIP, netMask);
  WiFi.softAP(ssid);

  // mount filesystem etc.
  EEPROM.begin(4096);
  LittleFS.begin();

  // create file for text messages if it doesn't exist yet
  f = LittleFS.open("/messages.txt", "a");
  f.close();

  // route traffic to index.html file
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(LittleFS, "/index.html", "text/html"); });

  // route to /styles.css file
  server.on("/styles.css", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(LittleFS, "/styles.css", "text/css"); });

  // route to /scripts.js file
  server.on("/scripts.js", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(LittleFS, "/scripts.js", "text/js"); });

  server.on("/sendText", HTTP_POST, [](AsyncWebServerRequest *request)
            {
    int params_count = request->params();

        char name[5];
        char *text = NULL;
        id.toCharArray(name,5);

        for (int i = 0; i < params_count; i++)
        {
            AsyncWebParameter *p = request->getParam(i);

            char *paramName = (char *)p->name().c_str();
            char *paramValue = (char *)p->value().c_str();

            // replace some characters
            for (unsigned int i = 0; i < strlen(paramValue); i++)
            {
                // replace pipe with /
                // because we use pipe as separator between messages.
                // if anyone used the pipe symbol in their messages, it would
                // mess up everything.
                if (paramValue[i] == '|')
                {
                    paramValue[i] = '/';
                }
            }

            // test if input only has spaces
            bool onlySpaces = true;

            if (strlen(paramValue) == 0)
            {
                onlySpaces = false;
            }

            for (unsigned int i = 0; i < strlen(paramValue); i++)
            {
                if (paramValue[i] != ' ')
                {
                    onlySpaces = false;
                    break;
                }
            }
            if (strcmp(paramName, "text") == 0 && strlen(paramValue) > 0 && !onlySpaces)
            {
                text = paramValue;
            }
        }

        if (text == NULL)
        {
            // illegal input -> abort
            request->redirect("/");
            return;
        }

        txWrapper(String(text),SX126x_TXMODE_SYNC);
         request->redirect("/"); });

  server.on("/showText", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(LittleFS, "/messages.txt", "text/html"); });

  // route to show timestamp of last write
  server.on("/lastWrite", HTTP_GET, [](AsyncWebServerRequest *request)
            {
        // open file
        f = LittleFS.open("/messages.txt", "r");

        // get last write time
        time_t lastWriteTime = f.getLastWrite();

        // convert to string
        char *lastWriteString = ctime(&lastWriteTime);

        // close file
        f.close();

        // send response
        request->send(200, "text/plain", lastWriteString); });

  // route to clear message-file
  server.on("/clear", HTTP_GET, [](AsyncWebServerRequest *request)
            {
        f = LittleFS.open("/messages.txt", "w");
        f.close();
        request->redirect("/"); });

  // route rest of traffic to index (triggers captive portal popup)
  server.onNotFound([](AsyncWebServerRequest *request)
                    { request->redirect("/"); });

  // start the webserver
  server.begin();

  // LORA
  Serial.begin(9600);

  // lora.DebugPrint(true);

  int16_t ret = lora.begin(RF_FREQUENCY,     // frequency in Hz
                           TX_OUTPUT_POWER); // tx power in dBm
  if (ret != ERR_NONE)
    while (1)
    {
      delay(1);
    }

  lora.LoRaConfig(LORA_SPREADING_FACTOR,
                  LORA_BANDWIDTH,
                  LORA_CODINGRATE,
                  LORA_PREAMBLE_LENGTH,
                  LORA_PAYLOADLENGTH,
                  true,   // crcOn
                  false); // invertIrq
}

void loop()
{
  i = 0;
  uint8_t rxData[255];
  String buff;
  receiveWrapper(rxData, 255);
  if (Serial.available())
  {
    buff = Serial.readStringUntil('\n');
    while (buff[i] != '\0')
    {
      // Serial.print(buff[i]);
      i++;
    }
    Serial.println();
    txWrapper(buff, SX126x_TXMODE_SYNC);
  }
}