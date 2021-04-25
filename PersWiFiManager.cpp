/* PersWiFiManager
   version 5.0.0
   https://r-downing.github.io/PersWiFiManager/
*/

#if defined(ESP8266) || defined(ESP32)
#include "PersWiFiManager.h"

#if defined(ESP32)
#include <esp_wifi.h>
#endif

extern const char* device_hostname_full;
extern const char* device_hostname_partial;

PersWiFiManager::PersWiFiManager(AsyncWebServer& s, DNSServer& d) {

  _server = &s;
  _dnsServer = &d;
  _apPass = "";
  _freshConnectionAttempt = false;
} //PersWiFiManager

bool PersWiFiManager::attemptConnection(const String& ssid, const String& pass) {
  //attempt to connect to wifi
  WiFi.mode(WIFI_STA);
  WiFi.hostname(device_hostname_partial);
  if (ssid.length()) {
    resetSettings(); // To avoid issues (experience from WiFiManager)
    if (pass.length()) WiFi.begin(ssid.c_str(), pass.c_str());
    else WiFi.begin(ssid.c_str());
  } else {
    if((getSsid() == "") && (WiFi.status() != WL_CONNECTED)) { // No saved credentials, so skip trying to connect
      _connectStartTime = millis();
      _freshConnectionAttempt = true;
      return false;
    } else {
      WiFi.begin();
    }
  }

  //if in nonblock mode, skip this loop
  _connectStartTime = millis();// + 1;
  while (!_connectNonBlock && _connectStartTime) {
    handleWiFi();
    delay(10);
  }

  return (WiFi.status() == WL_CONNECTED);

} //attemptConnection

void PersWiFiManager::handleWiFi() {
  if (!_connectStartTime) return;

  if (WiFi.status() == WL_CONNECTED) {
    _connectStartTime = 0;
    if (_connectHandler) _connectHandler();
    return;
  }

  //if failed or no saved SSID or no WiFi credentials were found or not connected and time is up
  if ((WiFi.status() == WL_CONNECT_FAILED) || _freshConnectionAttempt || ((WiFi.status() != WL_CONNECTED) && ((millis() - _connectStartTime) > (1000 * WIFI_CONNECT_TIMEOUT)))) {
    startApMode();
    _connectStartTime = 0; //reset connect start time
    _freshConnectionAttempt = false;
  }

} //handleWiFi

void PersWiFiManager::startApMode(){
  //start AP mode
  IPAddress apIP(192, 168, 4, 1);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  _apPass.length() ? WiFi.softAP(getApSsid().c_str(), _apPass.c_str(), 11) : WiFi.softAP(getApSsid().c_str());

  _dnsServer->stop();
  _dnsServer->setTTL(300);
  _dnsServer->start(53, "*", apIP);

  if (_apHandler) _apHandler();  
}//startApMode

void PersWiFiManager::setConnectNonBlock(bool b) {
  _connectNonBlock = b;
} //setConnectNonBlock

void PersWiFiManager::setupWiFiHandlers() {
  _server->on("/wifi/list", HTTP_GET, [](AsyncWebServerRequest* request) {
    //scan for wifi networks
    int n = WiFi.scanComplete();
    String s = "";
    if (n == -2) {
      WiFi.scanNetworks(true);
    }
    else if (n) {
    //build array of indices
    int ix[n];
    for (int i = 0; i < n; i++) ix[i] = i;

    //sort by signal strength
    for (int i = 0; i < n; i++) for (int j = 1; j < n - i; j++) if (WiFi.RSSI(ix[j]) > WiFi.RSSI(ix[j - 1])) std::swap(ix[j], ix[j - 1]);
    //remove duplicates
    for (int i = 0; i < n; i++) for (int j = i + 1; j < n; j++) if (WiFi.SSID(ix[i]).equals(WiFi.SSID(ix[j])) && WiFi.encryptionType(ix[i]) == WiFi.encryptionType(ix[j])) ix[j] = -1;

    s.reserve(2050);
    //build plain text string of wifi info
    //format [signal%]:[encrypted 0 or 1]:SSID
    for (int i = 0; i < n && s.length() < 2000; i++) { //check s.length to limit memory usage
      if (ix[i] != -1) {
#if defined(ESP8266)
        s += String(i ? "\n" : "") + ((constrain(WiFi.RSSI(ix[i]), -100, -50) + 100) * 2) + ","
             + ((WiFi.encryptionType(ix[i]) == ENC_TYPE_NONE) ? 0 : 1) + "," + WiFi.SSID(ix[i]);
#elif defined(ESP32)
        s += String(i ? "\n" : "") + ((constrain(WiFi.RSSI(ix[i]), -100, -50) + 100) * 2) + ","
             + ((WiFi.encryptionType(ix[i]) == WIFI_AUTH_OPEN) ? 0 : 1) + "," + WiFi.SSID(ix[i]);
#endif
    }

      }
      WiFi.scanDelete();
    }

#if defined(ESP8266)
	ESP.wdtDisable();
	ESP.reset();
#elif defined(ESP32)
    ESP.restart();
#endif
	delay(2000);
  });

}//setupWiFiHandlers

bool PersWiFiManager::begin(const String& ssid, const String& pass) {
#if defined(ESP32)
    WiFi.mode(WIFI_STA);  // ESP32 needs this before setupWiFiHandlers(). Might be good for ESP8266 too?
#endif
  setupWiFiHandlers();
  return attemptConnection(ssid, pass); //switched order of these two for return
} //begin

void PersWiFiManager::resetSettings() {
#if defined(ESP8266)
  WiFi.disconnect();
#elif defined(ESP32)
  wifi_mode_t m = WiFi.getMode();
  if(!(m & WIFI_MODE_STA)) WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, true);
  if(!(m & WIFI_MODE_STA)) WiFi.mode(m);
#endif
} // resetSettings

String PersWiFiManager::getApSsid() {
  return _apSsid.length() ? _apSsid : "gbscontrol";
} //getApSsid

String PersWiFiManager::getSsid() {
#if defined(ESP8266)
  return WiFi.SSID();
#elif defined(ESP32)
  wifi_config_t conf;
  esp_wifi_get_config(WIFI_IF_STA, &conf);  // load wifi settings to struct comf
  const char *SSID = reinterpret_cast<const char*>(conf.sta.ssid);
  return String(SSID);
#endif
} //getSsid

void PersWiFiManager::setApCredentials(const String& apSsid, const String& apPass) {
  if (apSsid.length()) _apSsid = apSsid;
  if (apPass.length() >= 8) _apPass = apPass;
} //setApCredentials

void PersWiFiManager::onConnect(WiFiChangeHandlerFunction fn) {
  _connectHandler = fn;
}

void PersWiFiManager::onAp(WiFiChangeHandlerFunction fn) {
  _apHandler = fn;
}
#endif
