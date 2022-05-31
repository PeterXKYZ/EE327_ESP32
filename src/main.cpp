#include <Arduino.h>


// Camera includes -------------------------------------
#include "esp_camera.h"
#include "camera_pins.h"
// -----------------------------------------------------


// WiFi includes ---------------------------------------
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "esp_wpa2.h"
#include "secure_wifi.h"
// -----------------------------------------------------


// Connectivity ----------------------------------------
#include <ArduinoWebsockets.h>
#include <esp_now.h>
// -----------------------------------------------------


using namespace websockets;

// WiFi and server -------------------------------------
const char* ssid = "eduroam";
const char* path = "/cam";
const char* host = "10.105.195.71";
const size_t port = 420;
size_t wifi_counter = 0;
// -----------------------------------------------------


// camera frame rates ----------------------------------
const size_t frame_rate = 40;
const size_t interval = 1000 / frame_rate;
unsigned long prev_time = 0;
// -----------------------------------------------------

bool cam_on = false;
bool take_photo = false;

// button stuff ---------------------------------------
const int buttonPin = 15;
int pressed_curr = 0;
int pressed_prev = 0;
unsigned long button_time = 0;
// 5 sec delay between consecutive button presses for anti-spam
const size_t button_interval = 5000;  
// -----------------------------------------------------


// LED indication system -------------------------------
const int wifiLED = 13;
const int wsLED = 12;
// -----------------------------------------------------


// ESP Now ---------------------------------------------
// ESP32 MAC address: AC-67-B2-38-2E-8C
uint8_t broadcastAddress[] = {0xAC, 0x67, 0xB2, 0x38, 0x2E, 0x8C};    
esp_now_peer_info_t peerInfo;

void OnDataSent(const uint8_t* mac_addr, esp_now_send_status_t status) {
  Serial.print("Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
} 
// -----------------------------------------------------

void SendSound(int num) {
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t*) &num, sizeof(num));
  Serial.println(result == ESP_OK ? "Successful send" : "Failed send");
}


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
  else if (strcmp(msg_data, "s1") == 0) {
    SendSound(1);
  }
  else if (strcmp(msg_data, "s2") == 0) {
    SendSound(2);
  }
  else if (strcmp(msg_data, "s3") == 0) {
    SendSound(3);
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


void send_photo(WebsocketsClient& a_client) {
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    return;
  }

  a_client.sendBinary((char*) fb->buf, fb->len);
  esp_camera_fb_return(fb);
}


WebsocketsClient client;
void setup() {
  Serial.begin(115200);
  client.close();
  wifi_counter = 0;

  // LEDs setup ------------------------------------------
  pinMode(wifiLED, OUTPUT);
  pinMode(wsLED, OUTPUT);
  digitalWrite(wifiLED, LOW);
  digitalWrite(wsLED, LOW);
  // -----------------------------------------------------

  // Connect to WiFi -------------------------------------
  WiFi.mode(WIFI_STA);
  esp_wifi_sta_wpa2_ent_set_identity((uint8_t*) EAP_ANONYMOUS_IDENTITY, strlen(EAP_ANONYMOUS_IDENTITY)); //provide identity
  esp_wifi_sta_wpa2_ent_set_username((uint8_t*) EAP_IDENTITY, strlen(EAP_IDENTITY)); //provide username
  esp_wifi_sta_wpa2_ent_set_password((uint8_t*) EAP_PASSWORD, strlen(EAP_PASSWORD)); //provide password
  esp_wifi_sta_wpa2_ent_enable();

  WiFi.begin(ssid);
  while (WiFi.status() != WL_CONNECTED && wifi_counter < 50) {
    Serial.print(".");
    digitalWrite(wifiLED, LOW);
    delay(500);
    digitalWrite(wifiLED, HIGH);
    delay(500); 
    ++wifi_counter;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    digitalWrite(wifiLED, HIGH);
  }
  else {
    Serial.println("WiFi not connected");
    digitalWrite(wifiLED, LOW);
  }
  // -----------------------------------------------------


  // ESP NOW ---------------------------------------------
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
  }

  esp_now_register_send_cb(OnDataSent);
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
  }
  // -----------------------------------------------------


  // Setup Callbacks--------------------------
    client.onMessage(OnMessageCallback);
    client.onEvent(onEventsCallback);
  // -----------------------------------------


  // Connect to server------------------------------------
  if (WiFi.status() == WL_CONNECTED) {
    if (client.connect(host, port, path)) {
      Serial.println("photo Host connected"); 
      digitalWrite(wsLED, HIGH);
    } 
    else {
      Serial.println("photo Host connection failed");
      digitalWrite(wsLED, LOW);
    }
  }
  // -----------------------------------------------------


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


  // button stuff -----------------------------------
  pinMode(buttonPin, INPUT);
  // ------------------------------------------------
}

void loop() {
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    digitalWrite(wifiLED, LOW);
    delay(500);
    digitalWrite(wifiLED, HIGH);
  }

  if (!client.available()) {
    digitalWrite(wsLED, LOW);
    if (client.connect(host, port, path)) {
      Serial.println("photo Host connected"); 
      digitalWrite(wsLED, HIGH);
    } 
  }

  client.poll();
  unsigned long curr_time = millis();

  // button stuff -----------------------------------
  pressed_curr = digitalRead(buttonPin);
  if (pressed_curr > pressed_prev && curr_time - button_time > button_interval) {
    button_time = curr_time;

    Serial.println("Button pressed!");
    client.send("button");
  }
  pressed_prev = pressed_curr;
  // ------------------------------------------------

  if (curr_time - prev_time > interval && cam_on) {
    prev_time = curr_time;
    
    // Take a photo -----------------------------------
    send_photo(client);
    // ------------------------------------------------
  }

  if (take_photo) {
    // Take a photo -----------------------------------
    send_photo(client);
    // ------------------------------------------------
    take_photo = false;
  }
}