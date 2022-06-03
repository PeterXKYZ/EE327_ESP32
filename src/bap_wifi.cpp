#include <bap_wifi.h>

bool CheckConnection(const int& ledPin, bool debug) {
  size_t wifi_counter = 0;

  while (wifi_counter < 30) {
    if (WiFi.status() == WL_CONNECTED) {

      if (debug) {
        Serial.println("WiFi connected");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
      }
      
      digitalWrite(ledPin, HIGH);
      return true;
    }

    Serial.print(".");
    digitalWrite(ledPin, HIGH);
    delay(500);
    digitalWrite(ledPin, LOW);
    delay(500); 
    ++wifi_counter;
  }

  Serial.println("WiFi not connected");
  return false;
}