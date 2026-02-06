#include <Arduino.h>

// Встроенный LED на ESP32-S3 обычно на GPIO 48
// Если у вас другой пин или внешний LED, измените здесь
#define LED_PIN 48

void setup() {
  // Инициализируем Serial для отладки
  Serial.begin(115200);
  delay(1000);

  Serial.println("ESP32-S3 Started!");
  Serial.println("LED Blink Test");

  // Настраиваем пин светодиода
  pinMode(LED_PIN, OUTPUT);
}

void loop() {
  digitalWrite(LED_PIN, HIGH);
  Serial.println("LED ON");
  delay(500);

  digitalWrite(LED_PIN, LOW);
  Serial.println("LED OFF");
  delay(500);
}
