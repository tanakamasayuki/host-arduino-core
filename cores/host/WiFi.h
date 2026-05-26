#ifndef HOST_ARDUINO_WIFI_H
#define HOST_ARDUINO_WIFI_H

// Minimal stub of the Arduino/ESP32 WiFi facade for host builds.
// The host is presumed to already have network access, so begin()
// just reports success and localIP() returns 127.0.0.1.

#include <stdint.h>

#include "IPAddress.h"
#include "WString.h"
#include "WiFiClient.h"
#include "WiFiServer.h"
#include "WiFiUdp.h"

enum WiFiMode_t
{
    WIFI_OFF = 0,
    WIFI_STA = 1,
    WIFI_AP = 2,
    WIFI_AP_STA = 3
};

enum wl_status_t
{
    WL_NO_SHIELD = 255,
    WL_IDLE_STATUS = 0,
    WL_NO_SSID_AVAIL = 1,
    WL_SCAN_COMPLETED = 2,
    WL_CONNECTED = 3,
    WL_CONNECT_FAILED = 4,
    WL_CONNECTION_LOST = 5,
    WL_DISCONNECTED = 6
};

class WiFiClass
{
public:
    wl_status_t begin(const char *ssid = nullptr, const char * = nullptr)
    {
        ssid_ = ssid ? ssid : "";
        status_ = WL_CONNECTED;
        return status_;
    }
    bool mode(WiFiMode_t m)
    {
        mode_ = m;
        if (m == WIFI_OFF) status_ = WL_IDLE_STATUS;
        return true;
    }
    bool disconnect(bool = false)
    {
        status_ = WL_DISCONNECTED;
        ssid_ = "";
        return true;
    }
    wl_status_t status() { return status_; }
    IPAddress localIP() { return status_ == WL_CONNECTED ? IPAddress(127, 0, 0, 1) : IPAddress(0, 0, 0, 0); }
    IPAddress softAPIP() { return IPAddress(127, 0, 0, 1); }
    const char *SSID() { return ssid_.c_str(); }
    int32_t RSSI() { return 0; }

private:
    wl_status_t status_ = WL_IDLE_STATUS;
    WiFiMode_t mode_ = WIFI_STA;
    String ssid_;
};

extern WiFiClass WiFi;

#endif
