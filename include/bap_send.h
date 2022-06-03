#pragma once

#include <ArduinoWebsockets.h>
#include <esp_camera.h>
#include <esp_now.h>

void SendPhoto(websockets::WebsocketsClient& a_client);

void SendSound(const uint8_t* macAddress, int num);

void onEventsCallback(websockets::WebsocketsEvent event, String data);
