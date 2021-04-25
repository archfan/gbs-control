#if defined(ESP8266) || defined(ESP32)
#ifndef PERSWIFIMANAGER_H
#define PERSWIFIMANAGER_H

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#elif defined(ESP32)
#include <WiFi.h>
#include <AsyncTCP.h>
#endif
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>

#define WIFI_CONNECT_TIMEOUT 45

class PersWiFiManager {

  public:

    typedef std::function<void(void)> WiFiChangeHandlerFunction;

    PersWiFiManager(AsyncWebServer& s, DNSServer& d);

    bool attemptConnection(const String& ssid = "", const String& pass = "");

    void setupWiFiHandlers();

    bool begin(const String& ssid = "", const String& pass = "");

    void resetSettings();

    String getApSsid();

    String getSsid();

    void setApCredentials(const String& apSsid, const String& apPass = "");

    void setConnectNonBlock(bool b);

    void handleWiFi();

    void startApMode();

    void onConnect(WiFiChangeHandlerFunction fn);

    void onAp(WiFiChangeHandlerFunction fn);

  private:
    AsyncWebServer * _server;
    DNSServer * _dnsServer;
    String _apSsid, _apPass;

    bool _connectNonBlock;
    unsigned long _connectStartTime;
    bool _freshConnectionAttempt;

    WiFiChangeHandlerFunction _connectHandler;
    WiFiChangeHandlerFunction _apHandler;

};//class

#endif
#endif
