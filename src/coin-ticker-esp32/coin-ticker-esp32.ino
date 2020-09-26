#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "heltec.h"

const char * rootCACertificate = \
"-----BEGIN CERTIFICATE-----\n"\
"MIIDdzCCAl+gAwIBAgIEAgAAuTANBgkqhkiG9w0BAQUFADBaMQswCQYDVQQGEwJJ\n"\
"RTESMBAGA1UEChMJQmFsdGltb3JlMRMwEQYDVQQLEwpDeWJlclRydXN0MSIwIAYD\n"\
"VQQDExlCYWx0aW1vcmUgQ3liZXJUcnVzdCBSb290MB4XDTAwMDUxMjE4NDYwMFoX\n"\
"DTI1MDUxMjIzNTkwMFowWjELMAkGA1UEBhMCSUUxEjAQBgNVBAoTCUJhbHRpbW9y\n"\
"ZTETMBEGA1UECxMKQ3liZXJUcnVzdDEiMCAGA1UEAxMZQmFsdGltb3JlIEN5YmVy\n"\
"VHJ1c3QgUm9vdDCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAKMEuyKr\n"\
"mD1X6CZymrV51Cni4eiVgLGw41uOKymaZN+hXe2wCQVt2yguzmKiYv60iNoS6zjr\n"\
"IZ3AQSsBUnuId9Mcj8e6uYi1agnnc+gRQKfRzMpijS3ljwumUNKoUMMo6vWrJYeK\n"\
"mpYcqWe4PwzV9/lSEy/CG9VwcPCPwBLKBsua4dnKM3p31vjsufFoREJIE9LAwqSu\n"\
"XmD+tqYF/LTdB1kC1FkYmGP1pWPgkAx9XbIGevOF6uvUA65ehD5f/xXtabz5OTZy\n"\
"dc93Uk3zyZAsuT3lySNTPx8kmCFcB5kpvcY67Oduhjprl3RjM71oGDHweI12v/ye\n"\
"jl0qhqdNkNwnGjkCAwEAAaNFMEMwHQYDVR0OBBYEFOWdWTCCR1jMrPoIVDaGezq1\n"\
"BE3wMBIGA1UdEwEB/wQIMAYBAf8CAQMwDgYDVR0PAQH/BAQDAgEGMA0GCSqGSIb3\n"\
"DQEBBQUAA4IBAQCFDF2O5G9RaEIFoN27TyclhAO992T9Ldcw46QQF+vaKSm2eT92\n"\
"9hkTI7gQCvlYpNRhcL0EYWoSihfVCr3FvDB81ukMJY2GQE/szKN+OMY3EU/t3Wgx\n"\
"jkzSswF07r51XgdIGn9w/xZchMB5hbgF/X++ZRGjD8ACtPhSNzkE1akxehi/oCr0\n"\
"Epn3o0WC4zxe9Z2etciefC7IpJ5OCBRLbf1wbWsaY71k5h+3zvDyny67G7fyUIhz\n"\
"ksLi4xaNmjICq44Y3ekQEe5+NauQrz4wlHrQMz2nZQ/1/I6eYs9HRCwBXbsdtTLS\n"\
"R9I4LtD+gdwyah617jzV/OeBHRnDJELqYzmp\n"\
"-----END CERTIFICATE-----\n";

void setClock() {
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");

    Serial.print(F("Waiting for NTP time sync: "));
    time_t nowSecs = time(nullptr);
    while (nowSecs < 8 * 3600 * 2) {
        delay(500);
        Serial.print(F("."));
        yield();
        nowSecs = time(nullptr);
    }

    Serial.println();
    struct tm timeinfo;
    gmtime_r( & nowSecs, & timeinfo);
    Serial.print(F("Current time: "));
    Serial.print(asctime( & timeinfo));
}

WiFiMulti WiFiMulti;
DynamicJsonDocument doc(4096);

void setup() {

    Serial.begin(115200);
    // Serial.setDebugOutput(true);

    Serial.println();
    Serial.println();
    Serial.println();

    WiFi.mode(WIFI_STA);
    WiFiMulti.addAP("YOUR_SSID", "YOUR_PASSWORD");

    // wait for WiFi connection
    Serial.print("Waiting for WiFi to connect...");
    while ((WiFiMulti.run() != WL_CONNECTED)) {
        Serial.print(".");
    }
    Serial.println(" connected");

    setClock();

    Heltec.begin(true /*DisplayEnable Enable*/ , true /*LoRa Disable*/ , true /*Serial Enable*/ );
    Heltec.display -> flipScreenVertically();
    Heltec.display -> setFont(ArialMT_Plain_10);
    Heltec.display -> setTextAlignment(TEXT_ALIGN_LEFT);
    Heltec.display -> clear();
}

void drawPrice(double btcPrice, double ethPrice, double panPrice, double bestPrice) {
    Heltec.display -> clear();
    Heltec.display -> setTextAlignment(TEXT_ALIGN_LEFT);
    Heltec.display -> drawString(0, 0, "  BTC: " + String(btcPrice, 2) + " EUR");
    Heltec.display -> drawString(0, 13, "  ETH: " + String(ethPrice, 2) + " EUR");
    Heltec.display -> drawString(0, 26, "  PAN: " + String(panPrice, 4) + " EUR");
    Heltec.display -> drawString(0, 39, "BEST: " + String(bestPrice, 4) + " EUR");
    Heltec.display -> setTextAlignment(TEXT_ALIGN_RIGHT);
    Heltec.display -> drawString(128, 52, "bitpanda");
    Heltec.display -> display();
}

void loop() {
    WiFiClientSecure * client = new WiFiClientSecure;
    if (client) {
        client -> setCACert(rootCACertificate);

        {
            // Add a scoping block for HTTPClient https to make sure it is destroyed before WiFiClientSecure *client is 
            HTTPClient https;

            Serial.print("[HTTPS] begin...\n");
            if (https.begin( * client, "https://api.bitpanda.com/v1/ticker")) { // HTTPS
                Serial.print("[HTTPS] GET...\n");
                // start connection and send HTTP header
                int httpCode = https.GET();

                // httpCode will be negative on error
                if (httpCode > 0) {
                    // HTTP header has been send and Server response header has been handled
                    Serial.printf("[HTTPS] GET... code: %d\n", httpCode);

                    // file found at server
                    if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
                        String payload = https.getString();
                        Serial.println(payload);

                        DeserializationError error = deserializeJson(doc, payload);
                        if (error) {
                            Serial.print(F("deserializeJson() failed with code "));
                            Serial.println(error.c_str());
                        }

                        drawPrice(doc["BTC"]["EUR"], doc["ETH"]["EUR"], doc["PAN"]["EUR"], doc["BEST"]["EUR"]);
                    }
                } else {
                    Serial.printf("[HTTPS] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
                }

                https.end();
            } else {
                Serial.printf("[HTTPS] Unable to connect\n");
            }

            // End extra scoping block
        }

        delete client;
    } else {
        Serial.println("Unable to create client");
    }

    Serial.println();
    Serial.println("Waiting 60s before the next round...");
    delay(60000);
}
