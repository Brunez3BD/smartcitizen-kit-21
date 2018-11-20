#pragma once

/* #define testing // Uncomment this line for running test protocol */

#include <Arduino.h>
#include <RTCZero.h>
#include <time.h>
#include <SPI.h>
#include "ArduinoLowPower.h"
#include "SdFat.h"
#include <SPIFlash.h>
#include "SAMD_pinmux_report.h"
#include "wiring_private.h"
#include <RHReliableDatagram.h>
#include <RH_Serial.h>
#include <FlashStorage.h>
#include <ArduinoJson.h>

#include "Pins.h"
#include "SckLed.h"
#include "SckBatt.h"
#include "Shared.h"
#include "Config.h"
#include "Commands.h"
#include "Sensors.h"
#include "SckUrban.h"
#include "SckAux.h"


// Output
enum OutLevels { OUT_SILENT, OUT_NORMAL, OUT_VERBOSE, OUT_COUNT	};
enum PrioLevels { PRIO_LOW, PRIO_MED, PRIO_HIGH };
class Status
{

	private:
		uint32_t _lastTryMillis = 0; // ms
		uint32_t _timeout; 	// ms

	public:
		bool ok = false;
		bool error = false;
		uint8_t retrys = 0;
		uint8_t _maxRetrys;

		bool retry();
		void setOk();
		void reset();

	Status(uint8_t maxRetrys=5, uint32_t timeout=1000)
    	{
		_maxRetrys = maxRetrys;
		_timeout = timeout;
    	}
};

struct SckState
{
	bool onShell = false;
	bool onSetup = false;
	bool espON = false;
	bool wifiSet = false;
	bool tokenSet = false;
	bool helloPending = false;
	SCKmodes mode = MODE_NET;
	bool cardPresent = false;
	bool sleeping = false;
	bool publishPending = false;
	Status wifiStat = Status(1, 60000);
	Status timeStat = Status(2, 3000);
	Status helloStat = Status(3, 5000);
	Status infoStat = Status(3, 5000);
	Status publishStat = Status(3, 5000);

	inline bool operator==(SckState a) {
		if (	a.onSetup == onSetup
				&& a.espON == espON
				&& a.wifiSet == wifiSet
				&& a.tokenSet == tokenSet
				&& a.helloPending == helloPending
				&& a.mode == mode
				&& a.cardPresent == cardPresent
				&& a.sleeping == sleeping
		   ) return true;
		else return false;
	}
};


class SckBase
{
	private:
		// Input/Output
		String serialBuff;
		String previousCommand;
		uint8_t outRepetitions = 0;

		// **** ESP control
		uint32_t espStarted;

		// **** Time
		const uint8_t TIME_TIMEOUT = 20;		// seconds
		void epoch2iso(uint32_t toConvert, char* isoTime);

		// **** Mode Control
		void reviewState();
		void enterSetup();

		// ESP communication
		uint8_t netPack[NETPACK_TOTAL_SIZE];
		char netBuff[NETBUFF_SIZE];
		void ESPbusUpdate();
		void receiveMessage(SAMMessage wichMessage);
		bool sendConfig();
		bool pendingSyncConfig = false;

		// Button
		const uint16_t buttonLong = 5000;
		const uint16_t buttonVeryLong = 15000;
		uint32_t buttonLastEvent = 0;
		bool butOldState = true;
		bool wakingUp = false;
		void buttonEvent();
		void buttonStillDown();

		// Configuration
		void loadConfig();
		bool publishInfo();
		bool espInfoUpdated = false;
		bool infoPublished = false;

		// Urban board
		bool urbanPresent = false;
		friend class urban;
		SckUrban urban;

		// STORAGE
		// files
		struct SckFile {char name[13]; File file;};
		SckFile configFile {"CONFIG.TXT"};
		SckFile postFile {};
		SckFile debugFile {"DEBUG.TXT"};
		SckFile infoFile {"INFO.TXT"};
		// Sd card
		bool sdSelect();
		volatile bool sdInitPending = false;
		bool sdInit();
		bool saveInfo();
		bool infoSaved = false;
		// Flash memory
		SPIFlash flash = SPIFlash(pinCS_FLASH);
		void flashSelect();

		// Power
		uint8_t wakeUP_H = 3; 	// 3AM UTC
		uint8_t wakeUP_M = 0;
		uint8_t wakeUP_S = 0;
		uint32_t sleepTime;
		void updatePower();
		void goToSleep();


		// **** Sensors
		uint32_t lastPublishTime = 0; 	// seconds
		uint32_t lastSensorUpdate = 0;
		bool timeToPublish = false;
		void updateSensors();
		bool netPublish();
		bool sdPublish();

		// Timers
		bool alarmRunning_TC3 = false;

	public:
		const String hardwareVer = "2.0";
		const String SAMversion	= "0.3.0-" + String(__GIT_HASH__);
		const String SAMbuildDate = String(__ISO_DATE__);
		String ESPversion = "not synced";
		String ESPbuildDate = "not synced";

		void setup();
		void update();

		// **** Mode Control
		SckState st;
		void printState();

		// **** Time
		RTCZero rtc;
		char ISOtimeBuff[20];
		bool setTime(String epoch);
		bool ISOtime();

		// Peripherals
		SckLed led;
		friend class SckButton;

		// **** Sensors
		AllSensors sensors;
		bool getReading(SensorType wichSensor, bool wait=true);
		bool controlSensor(SensorType wichSensorType, String wichCommand);
		void publish();
		bool enableSensor(SensorType wichSensor);
		bool disableSensor(SensorType wichSensor);
		bool writeHeader = false;

		// Configuration
		Configuration config;
		Configuration getConfig();
		void saveConfig(bool defaults=false);

		// Input/Output
		void inputUpdate();

		// ESP control
		enum ESPcontrols { ESP_OFF, ESP_FLASH, ESP_ON, ESP_REBOOT, ESP_SLEEP, ESP_WAKEUP };
		void ESPcontrol(ESPcontrols myESPControl);

		// ESP communication
		bool sendMessage(ESPMessage wichMessage, const char *content);
		bool sendMessage(ESPMessage wichMessage);
		bool sendMessage();
		String ipAddress;
		String macAddress;
		String hostname;
		void mqttCustom(const char *topic, const char *payload);

		// Output
		const char *outLevelTitles[OUT_COUNT] PROGMEM = { "Silent",	"Normal", "Verbose"	};
		OutLevels outputLevel = OUT_VERBOSE;
		char outBuff[240];
		void sckOut(String strOut, PrioLevels priority=PRIO_MED, bool newLine=true);	// Accepts String object
		void sckOut(const char *strOut, PrioLevels priority=PRIO_MED, bool newLine=true);	// Accepts constant string
		void sckOut(PrioLevels priority=PRIO_MED, bool newLine=true);
		void prompt();

		// Button
		volatile bool butState = true;
		void butFeedback();

		// Commands
		AllCommands commands;

		// SDcard
		SdFat sd;
		bool sdDetect();
		SckFile monitorFile {"MONITOR.CSV"};

		// Power
		void sck_reset();
		SckBatt battery;
		volatile bool battPendingEvent = false;
		SckCharger charger;

		// Misc
		void getUniqueID();
		uint32_t uniqueID[4];
		char uniqueID_str[33];

		const char *modeTitles[MODE_COUNT] PROGMEM = {
			"not configured",		// modeTitles[MODE_NOT_CONFIGURED]
			"network",			// modeTitles[MODE_NET]
			"sdcard",			// modeTitles[MODE_SD]
			"sleep"				// modeTitles[MODE_SLEEP]
		};

#ifdef testing
		friend class SckTest;
#endif
};

bool I2Cdetect(TwoWire *_Wire, byte address);
void ISR_button();
void ISR_sdDetect();

