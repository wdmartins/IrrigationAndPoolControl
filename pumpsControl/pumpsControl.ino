#include <WiFiManager.h>
#include <Ticker.h>
#include <ESP8266WebServer.h>
#include <bitset>
#include <time.h>

/*------------------------------------------------------------------------------------*/
/* Constant Definitions                                                                   */   
/*------------------------------------------------------------------------------------*/

// Access point to configure Wi-Fi
const char* ACCESS_POINT_NAME = "ESP8266";
const char* ACCESS_POINT_PASS = "esp8266";
// HTTP Server 
const int HTTP_SERVER_PORT = 80;
// Default values
const std::bitset<7> IRRIGATION_WEEK_DAYS (0x54);
const int ZONE_IRRIGATION_TIME_MINUTES = 20;
const char *START_IRRIGATION_TIME = "02:00:00";
const int POOL_PUMP_RUNING_TIME_HOURS = 6;
const char *START_POOL_TIME = "07:00:00";
const int DELAY_BETWEEN_IRRIGATION_ZONES_MINUTES = 5;
const int IRRIGATION_ZONES = 4;
const char *DAYS_OF_THE_WEEK[] = {"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"};


/*------------------------------------------------------------------------------------*/
/* GPIO Definitions                                                                   */   
/*------------------------------------------------------------------------------------*/


/*------------------------------------------------------------------------------------*/
/* HTTP Server                                                                          */   
/*------------------------------------------------------------------------------------*/
ESP8266WebServer server(HTTP_SERVER_PORT);

void handleNotFound() {
    Serial.println("Request to " + server.uri() + " not handled");
    server.send(404);
}

void handleGetStatus() {
    Serial.println("Request to get status");
    server.send(200);
}

void handleStartPoolPump() {
    Serial.println("Request to start pool pump");
    server.send(200);
}

void handleStopPoolPump() {
    Serial.println("Request to stop pool pump");
    server.send(200);
}

void handleStartIrrigationPump() {
    Serial.println("Request to start irrigation pump");
    server.send(200);
}

void handleStopIrrigationPump() {
    Serial.println("Request to start irrigation pump");
    server.send(200);
}

void handleRainPause() {
    Serial.println("Request to pause irrigation due to rain");
    server.send(200);
}

void handleConfigPoolCycle() {
    Serial.println("Request to config pool cycle");
    server.send(200);
}

void handleConfigIrrigationCycle() {
    Serial.println("Request to config irrigation cycle");
    server.send(200);
}

void startServer () {
  server.begin();
  server.onNotFound(handleNotFound);
  server.on("/status", handleGetStatus);
  server.on("/startPoolPump", handleStartPoolPump);
  server.on("/stopPoolPump", handleStopPoolPump);
  server.on("/startIrrigationPump", handleStartIrrigationPump);
  server.on("/stopIrrigationPump", handleStopIrrigationPump);
  server.on("/rainPause", handleRainPause);
  server.on("/configPoolCycle", handleConfigPoolCycle);
  server.on("/configIrrigationCycle", handleConfigIrrigationCycle);
}

/*------------------------------------------------------------------------------------*/
/* Pumps Actions                                                                      */   
/*------------------------------------------------------------------------------------*/
void startPoolPump () {
  time_t tnow = time(nullptr);
  Serial.print("Starting Pool Pump at "); Serial.println(String(ctime(&tnow)));
}

void stopPoolPump () {
  time_t tnow = time(nullptr);
  Serial.println("Stoping Pool Pump at "); Serial.println(String(ctime(&tnow)));
}

void startIrrigationPump () {
  time_t tnow = time(nullptr);
  Serial.println("Starting Irrigation pump"); Serial.println(String(ctime(&tnow)));
}

void stopIrrigationPump () {
  time_t tnow = time(nullptr);
  Serial.println("Stoping Irrigation pump"); Serial.println(String(ctime(&tnow)));
}
/*------------------------------------------------------------------------------------*/
/* Intervals                                                                          */   
/*------------------------------------------------------------------------------------*/
Ticker ledTicker; // Builtin led ticker
Ticker poolPumpTicker;
Ticker irrigationPumpTicker;

/*------------------------------------------------------------------------------------*/
/* Global Variables                                                                   */   
/*------------------------------------------------------------------------------------*/
std::bitset<7> irrigationWeekDays;
int zoneIrrigationTimeMinutes;
struct tm startIrrigationTime = {0};
int poolPumpRunTimeHours;
struct tm startPoolPumpTime = {0};
int delayBetweenZonesIrrigationMinutes;
int irrigationZone = 0;
struct tm *startTime;
char timebuffer[20];


/*------------------------------------------------------------------------------------*/
/* Global Functions                                                                   */   
/*------------------------------------------------------------------------------------*/
// Initialize Scheduling data
String getDaysOfTheWeek(std::bitset<7> daysOfTheWeek) {
    String daysOfTheWeekString;
    for (std::size_t i = 0; i < daysOfTheWeek.size(); ++i) {
      if (daysOfTheWeek.test(i)) {
        daysOfTheWeekString += DAYS_OF_THE_WEEK[i];
        daysOfTheWeekString += " ";
      }
    }
    return daysOfTheWeekString;
}

void initScheduling() {
  Serial.println("Initializing Scheduling");
  //TODO: Retrieve from persistent data
  
  irrigationWeekDays = IRRIGATION_WEEK_DAYS;
  zoneIrrigationTimeMinutes = ZONE_IRRIGATION_TIME_MINUTES;
  strptime(START_IRRIGATION_TIME, "%T", &startIrrigationTime);
  
  poolPumpRunTimeHours = POOL_PUMP_RUNING_TIME_HOURS;

  strptime(START_POOL_TIME, "%T", &startPoolPumpTime);
  delayBetweenZonesIrrigationMinutes = DELAY_BETWEEN_IRRIGATION_ZONES_MINUTES;

  strftime(timebuffer, 20, "%T", &startPoolPumpTime);
  Serial.print("Pool Pump Start Time: "); Serial.println(timebuffer);
  Serial.print("Pool Pump Running Time: "); Serial.print(poolPumpRunTimeHours); Serial.println(" hours");
  Serial.print("Irrigation Days of the week: "); Serial.println(getDaysOfTheWeek(irrigationWeekDays));
  strftime(timebuffer, 20, "%T", &startIrrigationTime);
  Serial.print("Start Irrigation Time: "); Serial.println(timebuffer);
  Serial.print("Zone Irrigation Time: "); Serial.print(zoneIrrigationTimeMinutes); Serial.println(" minutes");
  Serial.print("Interval between irrigation zones: "); Serial.println(delayBetweenZonesIrrigationMinutes);
  
}

// Initialize Pump Ticker
void initPumpTickers(struct tm now) {
  Serial.println("Initializing pumps tickers");
   
}

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

/*------------------------------------------------------------------------------------*/
/* Setup                                                                              */   
/*------------------------------------------------------------------------------------*/
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

  // Config time
  setenv("TZ", "EST5EDT,M3.2.0/02:00:00,M11.1.0/02:00:00", 1);
  configTime(0, 0, "pool.ntp.org");

  // Get Time (twice to avoid .... 1970)
  time_t now = time(nullptr);
  delay(1000);
  now = time(nullptr);
  startTime = localtime(&now);
  strftime(timebuffer, 20, "%c", startTime);
  Serial.print("Starting Time: ");Serial.println(timebuffer);

  // Initialize pumps tickers
  initScheduling();
  initPumpTickers(*startTime);
  
  // Start http server
  startServer();
}

/*------------------------------------------------------------------------------------*/
/* Loop                                                                               */   
/*------------------------------------------------------------------------------------*/
void loop() {
  server.handleClient();
}
