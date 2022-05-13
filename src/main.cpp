#include <Arduino.h>
#include "esp_camera.h"
#include "camera_pins.h"
#include <WiFi.h>
#include <ArduinoWebsockets.h>

#include <WiFiClientSecure.h>
#include "esp_wpa2.h"
#include "secure_wifi.h"

using namespace websockets;

// const char* ssid = "Device-Northwestern";
const char* ssid = "eduroam";
const char* path = "/cam";
const char* host = "10.105.195.71";
const size_t port = 420;

const size_t frame_rate = 40;
const size_t interval = 1000 / frame_rate;
unsigned long prev_time = 0;

bool cam_on = false;
bool take_photo = false;

// WebSocket callbacks ---------------------------------
void OnMessageCallback(WebsocketsMessage msg) {
  Serial.print("Got message: ");
  const char* msg_data = msg.c_str();
  Serial.println(msg_data);

  if (strcmp(msg_data, "on") == 0) {
    cam_on = true;
  }
  else if (strcmp(msg_data, "off") == 0) {
    cam_on = false;
  }
  else if (strcmp(msg_data, "photo") == 0) {
    take_photo = true;
  }
}

void onEventsCallback(WebsocketsEvent event, String data) {
  if (event == WebsocketsEvent::ConnectionOpened) {
    Serial.println("Connection Opened");
  }
  else if (event == WebsocketsEvent::ConnectionClosed) {
    Serial.println("Connection Closed");
    esp_restart();
  }
  else if (event == WebsocketsEvent::GotPing) {
    Serial.println("Got a Ping!");
  }
  else if (event == WebsocketsEvent::GotPong) {
    Serial.println("Got a Pong!");
  }
}
// -----------------------------------------------------

WebsocketsClient client;
WiFiClientSecure wifi;
void setup() {
  Serial.begin(115200);
  client.close();

  // Camera config -------------------------------------
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  // init with high specs to pre-allocate larger buffers
  // FRAMESIZE_UXGA (1600 x 1200)
  // FRAMESIZE_QVGA (320 x 240)
  // FRAMESIZE_CIF (352 x 288)
  // FRAMESIZE_VGA (640 x 480)
  // FRAMESIZE_SVGA (800 x 600)
  // FRAMESIZE_XGA (1024 x 768)
  // FRAMESIZE_SXGA (1280 x 1024)
  if(psramFound()){
    config.frame_size = FRAMESIZE_CIF;
    config.jpeg_quality = 24;  //0-63 lower number means higher quality
    config.fb_count = 1;
  } else {
    config.frame_size = FRAMESIZE_CIF;
    config.jpeg_quality = 32;  //0-63 lower number means higher quality
    config.fb_count = 1;
  }
  // ------------------------------------------------

  // Connect to WiFi --------------------------------
  WiFi.mode(WIFI_STA);
  esp_wifi_sta_wpa2_ent_set_identity((uint8_t*) EAP_ANONYMOUS_IDENTITY, strlen(EAP_ANONYMOUS_IDENTITY)); //provide identity
  esp_wifi_sta_wpa2_ent_set_username((uint8_t*) EAP_IDENTITY, strlen(EAP_IDENTITY)); //provide username
  esp_wifi_sta_wpa2_ent_set_password((uint8_t*) EAP_PASSWORD, strlen(EAP_PASSWORD)); //provide password
  esp_wifi_sta_wpa2_ent_enable();

  WiFi.begin(ssid);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  // ------------------------------------------------

  // Connect to server-------------------------------
  if (client.connect(host, port, path)) {
    Serial.println("photo Host connected"); 
  } 
  else {
    Serial.println("photo Host connection failed");
    while(1) {}
  }
  // ------------------------------------------------

  // Setup Callbacks---------------------------------
    client.onMessage(OnMessageCallback);
    client.onEvent(onEventsCallback);
  // ------------------------------------------------

  // Init camera ------------------------------------
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }
  // ------------------------------------------------

  // Camera settings --------------------------------
  sensor_t* s = esp_camera_sensor_get();
  s->set_contrast(s, 2);
  s->set_saturation(s, 0);
  // ------------------------------------------------
}

void loop() {
  client.poll();
  unsigned long curr_time = millis();

  if (curr_time - prev_time > interval && cam_on) {
    prev_time = curr_time;
    
    if (!client.available()) {
      return;
    }

    // Take a photo -----------------------------------
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      return;
    }

#ifdef DEBUG
    Serial.println("Camera capture sucessful!");
#endif

    client.sendBinary((char*) fb->buf, fb->len);
    esp_camera_fb_return(fb);
    // ------------------------------------------------
  }

  if (take_photo)  {
    // Take a photo -----------------------------------
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      return;
    }

#ifdef DEBUG
    Serial.println("Camera capture sucessful!");
#endif

    client.sendBinary((char*) fb->buf, fb->len);
    esp_camera_fb_return(fb);
    // ------------------------------------------------

    take_photo = false;
  }
}