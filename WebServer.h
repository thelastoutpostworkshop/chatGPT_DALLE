// Web server Functions
//
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <esp32-hal-adc.h>
#include "secrets.h"

WebServer server(80);

// Web server host name
const char *hostName = "dallee_round";

void initWebServer()
{
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    Serial.println("");

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(100);
        Serial.print(".");
    }

    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    if (MDNS.begin(hostName))
    {
        Serial.println("MDNS responder started");
    }

    server.begin();
    Serial.println("HTTP server started");
}
