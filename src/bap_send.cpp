#include <bap_send.h>



void SendPhoto(websockets::WebsocketsClient& a_client) {
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("Camera capture failed");
        return;
    }

    a_client.sendBinary((char*) fb->buf, fb->len);
    esp_camera_fb_return(fb);
}


void SendSound(const uint8_t* macAddress, int num) {
  esp_err_t result = esp_now_send(macAddress, (uint8_t*) &num, sizeof(num));
  Serial.println(result == ESP_OK ? "Successful send" : "Failed send");
}