#include "WifiSetup.h"
#include "App.h"

#include <Arduino.h>

void setup() {
  setup_wifi();
  setup_app();
}
void loop() {
  loop_wifi();
  loop_app();
}
