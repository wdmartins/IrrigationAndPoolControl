#include <WiFiManager.h>
#include <Ticker.h>

// Access point to configure Wi-Fi
#define ACCESS_POINT_NAME "ESP8266"
#define ACCESS_POINT_PASS "esp8266"

/*------------------------------------------------------------------------------------*/
/* GPIO Definitions                                                                   */   
/*------------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------------*/
/* Intervals                                                                          */   
/*------------------------------------------------------------------------------------*/
Ticker ledTicker;

/*------------------------------------------------------------------------------------*/
/* Global Variables                                                                   */   
/*------------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------------*/
/* Global Functions                                                                   */   
/*------------------------------------------------------------------------------------*/

// Builtin LED flashing
void ledTick() {
  int state = digitalRead(BUILTIN_LED);
  digitalWrite(BUILTIN_LED, !state);
}

// WiFiManager Configuration CallBack
void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  Serial.println(myWiFiManager->getConfigPortalSSID());
  ledTicker.attach(0.2, ledTick);
}

void setup() {
  Serial.begin(115200);

  // Setup built in LED
  pinMode(BUILTIN_LED, OUTPUT);
  ledTicker.attach(0.6, ledTick);
  
  // Instantiate and setup WiFiManager
  WiFiManager wifiManager;
  wifiManager.setAPCallback(configModeCallback);
  if (!wifiManager.autoConnect(ACCESS_POINT_NAME, ACCESS_POINT_PASS)) {
    Serial.println("Failed to connect and hit timeout");
    ESP.reset();
    delay(1000);  
  }
  Serial.println("Connection Sucessful");
  ledTicker.detach();
  digitalWrite(BUILTIN_LED, LOW);

}

void loop() {
  // put your main code here, to run repeatedly:

}
