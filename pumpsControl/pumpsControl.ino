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
const std::bitset<7> IRRIGATION_WEEK_DAYS (0x2A); // =0101010 Sunday least significant, Saturday most significant
const int ZONE_IRRIGATION_TIME_MINUTES = 20;
const char *START_IRRIGATION_TIME = "02:00:00";
const int DELAY_BETWEEN_IRRIGATION_ZONES_MINUTES = 5; //Cannot be more than 72 minutes
const int IRRIGATION_ZONES = 4;
const int POOL_PUMP_RUNNING_TIME_HOURS = 6;
const char *START_POOL_TIME = "07:00:00";
const char *DAYS_OF_THE_WEEK[] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

/*------------------------------------------------------------------------------------*/
/* Helper Classes                                                                   */   
/*------------------------------------------------------------------------------------*/
typedef struct PoolParametersData   {
  struct tm startPoolPumpTime = {0};
  int poolPumpRunTimeHours = POOL_PUMP_RUNNING_TIME_HOURS;
};

typedef struct IrrigationParametersData {
  std::bitset<7> irrigationWeekDays = IRRIGATION_WEEK_DAYS;
  int zoneIrrigationTimeMinutes = ZONE_IRRIGATION_TIME_MINUTES;
  struct tm startIrrigationTime = {0}; 
  int delayBetweenZonesIrrigationMinutes = DELAY_BETWEEN_IRRIGATION_ZONES_MINUTES;
  int irrigationZones = IRRIGATION_ZONES;
};

class PoolParameters {
  public:
    PoolParameters(){
      strptime(START_POOL_TIME, "%T", &poolParametersData.startPoolPumpTime);
    };
    ~PoolParameters() {};
    struct tm getStartPoolPumpTime(void) {
      return poolParametersData.startPoolPumpTime;
    };
    char *getStartPoolPumpTimeStr() {
      strftime(timebuffer, 20, "%T", &poolParametersData.startPoolPumpTime);
      return timebuffer;
    }

    int getPoolPumpTimeHours(void) {
      return poolParametersData.poolPumpRunTimeHours;
    };
  private:
    PoolParametersData poolParametersData;
    char timebuffer[100];
};


class IrrigationParameters {
  public:
    IrrigationParameters() {
      strptime(START_IRRIGATION_TIME, "%T", &irrigationParametersData.startIrrigationTime);
    };
    ~IrrigationParameters() {};

    std::bitset<7> getIrrigationWeekDays() {
      return irrigationParametersData.irrigationWeekDays;
    }

    int getZoneIrrigationTimeMinutes() {
      return irrigationParametersData.zoneIrrigationTimeMinutes;
    }

    struct tm getIrrigationStartTime() {
      return irrigationParametersData.startIrrigationTime;
    }

    char *getIrrigationStartTimeStr() {
      strftime(timebuffer, 20, "%T", &irrigationParametersData.startIrrigationTime);
      return timebuffer;
    }

    int getDelayBetweenZonesMinutes() {
      return irrigationParametersData.delayBetweenZonesIrrigationMinutes;
    }

    int getIrrigationZones() {
      return irrigationParametersData.irrigationZones;
    }

  private:
    IrrigationParametersData irrigationParametersData;
    char timebuffer[100];
};

/*------------------------------------------------------------------------------------*/
/* Global Variables                                                                   */   
/*------------------------------------------------------------------------------------*/
PoolParameters poolParams;
IrrigationParameters irrigationParams;

int irrigationZone = 1;
struct tm *startTime;
char timebuffer[100];
bool poolPumpRunnig = false;
bool irrigationPumpRunning = false;

/*------------------------------------------------------------------------------------*/
/* My Ticker for longer timers                                                        */   
/*------------------------------------------------------------------------------------*/
class LongTicker 
{
public:
  static const int MAX_MINUTES_TICKER = 60;

  LongTicker();
  LongTicker(std::string tickerName);
  ~LongTicker();
    
  void once(int minutes, Ticker::callback_t cb);
  void showStatus(const char *functionName);
  bool isRunning(void) {
    return ticker.active();
  }
  void detach() {
    ticker.detach();
  }
  void setTotalMinutesLeft(int minutes) {
    totalMinutesLeft = minutes;
  }

  int getTotalMinutesLeft(void) {
    return totalMinutesLeft;
  }

  Ticker::callback_t getCallback(void) {
    return cb;
  }

  void setCallBack(Ticker::callback_t callback) {
    cb = callback;
  }

  Ticker getTicker(void) {
    return ticker;
  }
private:
  Ticker ticker;
  std::string tickerName;
  int totalMinutesLeft;
  Ticker::callback_t cb;
};

void repeat(LongTicker *myTicker) {
  myTicker->showStatus("Begin repeat");
  int tickerMinutes = std::min(myTicker->getTotalMinutesLeft(), LongTicker::MAX_MINUTES_TICKER);
  if (tickerMinutes <= 0) {
    myTicker->getCallback()();
  } else {
    if (myTicker->getTotalMinutesLeft() > LongTicker::MAX_MINUTES_TICKER) {
      myTicker->setTotalMinutesLeft(myTicker->getTotalMinutesLeft() - tickerMinutes);
      myTicker->getTicker().once(LongTicker::MAX_MINUTES_TICKER * 60, repeat, myTicker);
      myTicker->showStatus("End repeat");
      return;
    }
    myTicker->getTicker().once(myTicker->getTotalMinutesLeft() * 60, myTicker->getCallback());
    myTicker->setTotalMinutesLeft(0);
  }
  myTicker->showStatus("End repeat");
}
LongTicker::LongTicker()
: totalMinutesLeft(0) {}

LongTicker::LongTicker(std::string tickName)
: totalMinutesLeft(0)
, tickerName(tickName) {}

LongTicker::~LongTicker() {
  ticker.detach();
}

void LongTicker::showStatus(const char *functionName) {
  struct tm *now = getCurrentTime();
  Serial.print("[TICKER] ");Serial.print(getTime(now)); Serial.print("("); Serial.print(functionName); Serial.print(") ");
  Serial.print(": Name: "); Serial.print(this->tickerName.c_str()); Serial.print(", minutesLeft: "); Serial.println(this->totalMinutesLeft);  
}

void LongTicker::once(int minutes, Ticker::callback_t cb) {
  this->showStatus("Begin once");
  if (ticker.active()) {
    debug("ERROR: This ticker is already running");
    return;        
  }
  if (minutes > MAX_MINUTES_TICKER) {
    this->totalMinutesLeft = minutes - MAX_MINUTES_TICKER;
    this->cb = cb;
    ticker.once(MAX_MINUTES_TICKER * 60, repeat, this);
    this->showStatus("End once");
    return;
  }
  ticker.once(minutes * 60, cb);
  this->totalMinutesLeft = 0;
  this->showStatus("End once");
}

/*------------------------------------------------------------------------------------*/
/* Intervals                                                                          */   
/*------------------------------------------------------------------------------------*/
Ticker ledTicker; // Builtin led ticker
LongTicker poolPumpTicker("Pool Pump");
LongTicker irrigationPumpTicker("Irrigation");
LongTicker midnightReschedulingTicker("Rescheduling");


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
/* Helpers                                                                            */   
/*------------------------------------------------------------------------------------*/
void debug(const char * text) {
  struct tm *now = getCurrentTime();
  Serial.print("[PUMPCTRL] ");Serial.print(getTime(now)); Serial.print(":"); Serial.println(text);  
}

int minutesTillMidnight (struct tm now) {
  Serial.print("DAYLIGHT SAVING TIME: "); Serial.println(now.tm_isdst);
  int offset = (now.tm_isdst == 1 ? 60 : 0);
  return ((23 - now.tm_hour) * 60) + (now.tm_min == 0 ? 0 : 60 - now.tm_min) + offset;
}

int minutesTillMidnight() {
  struct tm *now = getCurrentTime();
  return minutesTillMidnight(*now);
}

/*------------------------------------------------------------------------------------*/
/* Global Pumps Control Functions                                                        */   
/*------------------------------------------------------------------------------------*/
// Forward declarations
//void stopPoolPump(int);
//void stopIrrigationPump(int);

// Start Pool Pump Based on Schedule
void startPoolPump () {
  startPoolPump(poolParams.getPoolPumpTimeHours() * 60);
}

void startPoolPump(int minutes) {
  debug("Starting Pool Pump");
  //TODO: Start Pool Pump
  poolPumpRunnig = true;
  poolPumpTicker.once(minutes, stopPoolPump);
}

void stopPoolPump () {
  debug("Stoping Pool Pump");
  //TODO: Stop Pool Pump
  midnightReschedulingTicker.once(minutesTillMidnight(), reschedule);
  poolPumpRunnig = false;
  return;
}

void startIrrigationPump () {
  startIrrigationPump(irrigationParams.getZoneIrrigationTimeMinutes());
}

void startIrrigationPump (int minutes) {
  debug("Starting Irrigation Pump");
  //TODO: Start Irrigation Pump
  irrigationPumpRunning = true;
  irrigationPumpTicker.once(minutes, stopIrrigationPump);
}

void stopIrrigationPump () {
  debug("Stoping Irrigation Pump");
  //TODO: Stop Irrigation Pump
  if (irrigationZone >= irrigationParams.getIrrigationZones()) {
    irrigationZone = 1;
    irrigationPumpRunning = false;
    midnightReschedulingTicker.once(minutesTillMidnight(), reschedule);
    return;
  }
  debug("Starting delay between irrigation zones");
  irrigationZone++;
  irrigationPumpTicker.once(irrigationParams.getDelayBetweenZonesMinutes() * 60, startIrrigationPump);
  return;
}

/*------------------------------------------------------------------------------------*/
/* Global Scheduling Functions                                                        */   
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
  
  Serial.print("Pool Pump Start Time: "); Serial.println(poolParams.getStartPoolPumpTimeStr());
  Serial.print("Pool Pump Running Time: "); Serial.print(poolParams.getPoolPumpTimeHours()); Serial.println(" hours");
  Serial.print("Irrigation Days of the week: "); Serial.println(getDaysOfTheWeek(irrigationParams.getIrrigationWeekDays()));
  Serial.print("Start Irrigation Time: "); Serial.println(irrigationParams.getIrrigationStartTimeStr());
  Serial.print("Zone Irrigation Time: "); Serial.print(irrigationParams.getZoneIrrigationTimeMinutes()); Serial.println(" minutes");
  Serial.print("Interval between irrigation zones: "); Serial.println(irrigationParams.getDelayBetweenZonesMinutes());
  
}
void reschedule() {
  struct tm *now = getCurrentTime();
  schedulePoolPump(*now);
  scheduleIrrigationPump(*now);  
  return;
}

// Schedule Pool Pump
void schedulePoolPump(struct tm now) {
  debug("Initializing pumps tickers");
  //-------------------------------
  // Check Pool Pump
  //-------------------------------
  if (now.tm_hour >= poolParams.getStartPoolPumpTime().tm_hour) {
    if (now.tm_hour < (poolParams.getStartPoolPumpTime().tm_hour + poolParams.getPoolPumpTimeHours())) {
      // Pump Should be running. How long?
      int hoursLeft = poolParams.getStartPoolPumpTime().tm_hour + poolParams.getPoolPumpTimeHours() - now.tm_hour;
      int minutesLeft = hoursLeft * 60 - (now.tm_min > poolParams.getStartPoolPumpTime().tm_min ? 60 - now.tm_min : now.tm_min - poolParams.getStartPoolPumpTime().tm_min);
      Serial.print("Start Pool Pump for ");Serial.print(minutesLeft); Serial.println(" minutes");
      if (minutesLeft == 0) minutesLeft++; 
      startPoolPump(minutesLeft);
    } else {
      // Too late to start the pump today. Reschedule at midnigth.
      debug("Too late to start the pump today. Reschedule at midnight");
      midnightReschedulingTicker.once(minutesTillMidnight(now), reschedule);
    }
  } else {
    //Too Early to start today
    int hoursLeft = poolParams.getStartPoolPumpTime().tm_hour - now.tm_hour;  
    int minutesLeft = hoursLeft * 60;
    if (now.tm_min > poolParams.getStartPoolPumpTime().tm_min) {
      minutesLeft -= (now.tm_min - poolParams.getStartPoolPumpTime().tm_min);
    } else {
      minutesLeft += (poolParams.getStartPoolPumpTime().tm_min - now.tm_min);
    }
    if (minutesLeft == 0) minutesLeft++;
    Serial.print("Too early to start the pump today. Start in ");Serial.print(minutesLeft);Serial.println(" minutes");
    poolPumpTicker.once(minutesLeft * 60, startPoolPump);
  }
}

// Schedule Irrigation Pump
void scheduleIrrigationPump(struct tm now) {
  //-------------------------------
  // Check Irrigation Pump.
  // I do not know whether pump has been running before power on so I will start the whole cycle
  // from the beginnig if whithin the irrigation time:
  // (startIrrigationTime + zones * (zoneIrrigationTimeMinutes) + (zones-1) delayBetweenZonesIrrigationMinutes)
  if (irrigationParams.getIrrigationWeekDays().test(now.tm_wday) == false) {
    //Today is not a irrigation day. Reschedule at th begining of the next day.
    Serial.print("Today is ");Serial.print(DAYS_OF_THE_WEEK[now.tm_wday]);Serial.println(" and it is not an irrigation day");
    midnightReschedulingTicker.once(minutesTillMidnight(now), reschedule);
    return;
  }
  
  int totalIrrigationCycleMinutes = irrigationParams.getIrrigationZones() * irrigationParams.getZoneIrrigationTimeMinutes()
      + (irrigationParams.getIrrigationZones() - 1) * irrigationParams.getDelayBetweenZonesMinutes();
  
  if (now.tm_hour >= irrigationParams.getIrrigationStartTime().tm_hour) {
    if ((now.tm_hour * 60) > ((irrigationParams.getIrrigationStartTime().tm_hour * 60) + totalIrrigationCycleMinutes)) {
      // Too late to start irrigation today. Reschedule at midnight
      debug("Too late to start the irrigation today. Reschedule at midnight");
      midnightReschedulingTicker.once(minutesTillMidnight(now), reschedule);
      return;
    } else {
      // Start Complete Irrigation Cycle 
      debug("Within irrigation time. Start complete irrigation cycle");
    }
  } else {
    //Too early to start irrigation
    int hoursLeft = irrigationParams.getIrrigationStartTime().tm_hour - now.tm_hour;  
    int minutesLeft = hoursLeft * 60;
    if (now.tm_min > irrigationParams.getIrrigationStartTime().tm_min) {
      minutesLeft -= (now.tm_min - irrigationParams.getIrrigationStartTime().tm_min);
    } else {
      minutesLeft += (irrigationParams.getIrrigationStartTime().tm_min - now.tm_min);
    }
    if (minutesLeft == 0) minutesLeft++;
    Serial.print("Too early to start the irrigation today. Start in ");Serial.print(minutesLeft);Serial.println(" minutes");
    irrigationPumpTicker.once(minutesLeft * 60, startIrrigationPump);
  }
}

// Builtin LED flashing
void ledTick() {
  int state = digitalRead(BUILTIN_LED);
  digitalWrite(BUILTIN_LED, !state);
}

// WiFiManager Configuration CallBack
void configModeCallback (WiFiManager *myWiFiManager) {
  debug("Entered config mode");
  Serial.println(WiFi.softAPIP());
  debug((myWiFiManager->getConfigPortalSSID()).c_str());
  ledTicker.attach(0.2, ledTick);
}

struct tm *getCurrentTime() {
  // Get time twice to avoid ....1970
  time(nullptr);
  delay(1000);
  time_t now = time(nullptr);
  return localtime(&now);
}

char *getTime(struct tm *now) {
  strftime(timebuffer, 20, "%c", now);
  return timebuffer;
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
  // Config time
  setenv("TZ", "EST5EDT,M3.2.0/02:00:00,M11.1.0/02:00:00", 1);
  configTime(0, 0, "pool.ntp.org");
  debug("System started");

  debug("Connection Sucessful");
  ledTicker.detach();
  digitalWrite(BUILTIN_LED, LOW);


  // Initialize pumps tickers
  initScheduling();
  startTime = getCurrentTime();
  schedulePoolPump(*startTime);
  scheduleIrrigationPump(*startTime);
  
  // Start http server
  startServer();
}

/*------------------------------------------------------------------------------------*/
/* Loop                                                                               */   
/*------------------------------------------------------------------------------------*/
void loop() {
  server.handleClient();
}
