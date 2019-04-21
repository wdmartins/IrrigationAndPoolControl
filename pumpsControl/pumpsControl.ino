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
/* Forward Declarations                                                               */   
/*------------------------------------------------------------------------------------*/
void debug(const char * text);
void debug(std::string text);
struct tm *getCurrentTime();

/*------------------------------------------------------------------------------------*/
/* GPIO Definitions                                                                   */   
/*------------------------------------------------------------------------------------*/
const int POOL_PUMP_GPIO = 12;
const int IRRIGATION_PUMP_GPIO = 13;

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

    time_t getTodayStartPoolPumpTime(void) {
      struct tm *currentTime = getCurrentTime();
      currentTime->tm_hour = poolParametersData.startPoolPumpTime.tm_hour; 
      currentTime->tm_min = poolParametersData.startPoolPumpTime.tm_min; 
      currentTime->tm_sec = poolParametersData.startPoolPumpTime.tm_sec;
      return mktime(currentTime); 
    }

    char *getStartPoolPumpTimeStr() {
      strftime(timebuffer, 20, "%T", &poolParametersData.startPoolPumpTime);
      return timebuffer;
    }

    int getPoolPumpTimeHours(void) {
      return poolParametersData.poolPumpRunTimeHours;
    };

    time_t getTodayStopPoolPumpTime(void) {
      return getTodayStartPoolPumpTime() + poolParametersData.poolPumpRunTimeHours * 3600;
    }

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

    time_t getTodayIrrigationStartTime(void) {
      struct tm *currentTime = getCurrentTime();
      currentTime->tm_hour = irrigationParametersData.startIrrigationTime.tm_hour; 
      currentTime->tm_min = irrigationParametersData.startIrrigationTime.tm_min; 
      currentTime->tm_sec = irrigationParametersData.startIrrigationTime.tm_sec;
      return mktime(currentTime); 
    }

    time_t getCycleDuration(void) {
      int zones = getIrrigationZones();
      int totalIrrigationCycleMinutes = zones * getZoneIrrigationTimeMinutes()
          + (zones - 1) * getDelayBetweenZonesMinutes();
      return totalIrrigationCycleMinutes * 60;
    }

    time_t getTodayIrrigationStopTime(void) {
      return getTodayIrrigationStartTime() + getCycleDuration();
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

    bool isIrrigationDay(struct tm now) {
      return getIrrigationWeekDays().test(now.tm_wday);  
    }

  private:
    IrrigationParametersData irrigationParametersData;
    char timebuffer[100];
};

class Pump {
  public:
    Pump(const char *pumpName, int gpioIndex)
    :name(pumpName),
    gpio(gpioIndex),
    active(false){};
    ~Pump() {
      stop();
    };

    virtual void start() {
      debug(name + " pump started");
      active = true;
    };
    virtual void stop() {
      debug(name + " pump stop");
      active = false;
    };
    bool isRunning() {
      return active;
    };
  protected:
    std::string name;
    int gpio;
    bool active;
};

/*------------------------------------------------------------------------------------*/
/* Global Variables                                                                   */   
/*------------------------------------------------------------------------------------*/
PoolParameters poolParams;
IrrigationParameters irrigationParams;
Pump poolPump("Pool", POOL_PUMP_GPIO); 
Pump irrigationPump("Irrigation", IRRIGATION_PUMP_GPIO);

int irrigationZone = 1;
struct tm *startTime;
char timebuffer[100];

/*------------------------------------------------------------------------------------*/
/* My Ticker for longer timers                                                        */   
/*------------------------------------------------------------------------------------*/
class LongTicker 
{
public:
  static const int MAX_MINUTES_TICKER = 60;
  // Keep ticker and cb public
  Ticker ticker;
  Ticker::callback_t cb;

public:
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

private:
  std::string tickerName;
  int totalMinutesLeft;
};

void repeat(LongTicker *myTicker) {
  myTicker->showStatus("Begin repeat");
  int tickerMinutes = std::min(myTicker->getTotalMinutesLeft(), LongTicker::MAX_MINUTES_TICKER);
  if (tickerMinutes <= 0) {
    myTicker->cb();
  } else {
    if (myTicker->getTotalMinutesLeft() > LongTicker::MAX_MINUTES_TICKER) {
      myTicker->setTotalMinutesLeft(myTicker->getTotalMinutesLeft() - tickerMinutes);
      myTicker->ticker.once(LongTicker::MAX_MINUTES_TICKER * 60, repeat, myTicker);
      myTicker->showStatus("End repeat");
      return;
    }
    myTicker->ticker.once(myTicker->getTotalMinutesLeft() * 60, myTicker->cb);
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
  if (ticker.active()) {
    debug("This ticker is already running. It will be detached");
    ticker.detach();
  }
  this->totalMinutesLeft = minutes;
  this->showStatus("Begin once");
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

void debug(std::string text) {
  debug(text.c_str());
}

int minutesTillMidnight (struct tm now) {
  return ((23 - now.tm_hour) * 60) + (now.tm_min == 0 ? 0 : 60 - now.tm_min);
}

int minutesTillMidnight() {
  struct tm *now = getCurrentTime();
  return minutesTillMidnight(*now);
}

/*------------------------------------------------------------------------------------*/
/* Global Pumps Control Functions                                                        */   
/*------------------------------------------------------------------------------------*/

// Start Pool Pump Based on Schedule
void startPoolPump () {
  startPoolPump(poolParams.getPoolPumpTimeHours() * 60);
}

void startPoolPump(int minutes) {
  poolPump.start();
  poolPumpTicker.once(minutes, stopPoolPump);
}

void stopPoolPump () {
  poolPump.stop();
  midnightReschedulingTicker.once(minutesTillMidnight(), reschedule);
  return;
}

void startIrrigationPump () {
  startIrrigationPump(irrigationParams.getZoneIrrigationTimeMinutes());
}

void startIrrigationPump (int minutes) {
  irrigationPump.start();
  irrigationPumpTicker.once(minutes, stopIrrigationPump);
}

void stopIrrigationPump () {
  irrigationPump.stop();
  if (irrigationZone >= irrigationParams.getIrrigationZones()) {
    irrigationZone = 1;
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
  time_t nowRaw = mktime(&now);
  time_t poolStartRaw = poolParams.getTodayStartPoolPumpTime();
  time_t poolStopRaw = poolParams.getTodayStopPoolPumpTime();
  if (nowRaw >= poolStartRaw && nowRaw < poolStopRaw) {
    // Pump should be running
    startPoolPump((poolStopRaw - nowRaw) / 60);
  } else if (nowRaw > poolStopRaw) {
      // Too late to start the pump today. Reschedule at midnigth.
      debug("Too late to start the pump today. Reschedule at midnight");
      midnightReschedulingTicker.once(minutesTillMidnight(now), reschedule);
  } else {
    // To early to start today
    int minutesToStart = (poolStartRaw - nowRaw) / 60;
    Serial.print("Too early to start the pump today. Start in ");Serial.print(minutesToStart);Serial.println(" minutes");
    poolPumpTicker.once(minutesToStart, startPoolPump);
  }
}

// Schedule Irrigation Pump
void scheduleIrrigationPump(struct tm now) {
  if (irrigationParams.isIrrigationDay(now) == false) {
    //Today is not a irrigation day. Reschedule at th begining of the next day.
    Serial.print("Today is ");Serial.print(DAYS_OF_THE_WEEK[now.tm_wday]);Serial.println(" and it is not an irrigation day");
    midnightReschedulingTicker.once(minutesTillMidnight(now), reschedule);
    return;
  }
  time_t nowRaw = mktime(&now);
  time_t irrigationStartRaw = irrigationParams.getTodayIrrigationStartTime();
  time_t irrigationStopRaw = irrigationParams.getTodayIrrigationStopTime();

   if (nowRaw >= irrigationStartRaw && nowRaw < irrigationStopRaw) {
    // Start irrigation cycle
    startIrrigationPump();
  } else if (nowRaw > irrigationStopRaw) {
      // Too late to start the irrigation today. Reschedule at midnigth.
      debug("Too late to start the pump today. Reschedule at midnight");
      midnightReschedulingTicker.once(minutesTillMidnight(now), reschedule);
  } else {
    // To early to start today
    int minutesToStart = (irrigationStartRaw - nowRaw) / 60;
    Serial.print("Too early to start the irrigation today. Start in ");Serial.print(minutesToStart);Serial.println(" minutes");
    irrigationPumpTicker.once(minutesToStart, startIrrigationPump);
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
