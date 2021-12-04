# DeepakReddy-RTOSFirmware
Develop an application over FreeRTOS on ESP32 Development Board that is capable of handling Multiple Tasks over different threads

## Task Description

### Hardware Modules

1. SIM800L - GSM/GPRS Module
2. SD Card Module
3. Relays

### Software Components

1. Amazon AWS MQTT IoT Core Service
2. ESP-IDF Framework

### Software Algorithm

The software algorithm is divided into 4 major tasks that will be handled parallely through Multi-Threading.
Each task has its own priortity defined that is basically a measure of how frequently that task needs to be performed.

#### Task Names and Priorty

	Task Names									Priority
1. OTA over HTTPS						To be executed once every hour
2. Health Parameter Update				To be executed once every minute
3. Schedule download and Store Task 	To be executed once every day
4. Time Sync Task 						To be executed once every day

##### OTA over HTTPS Task

The OTA task will check for any available update notification over MQTT in a predefined topic to which it is subscribed to on Amazon AWS based on priority level defined. Once it received an update for OTA, it will trigger the "Over the Air Firmware Update over HTTPS"

##### Health Parameter Update Task

The ESP32 will read different parameters of the device based on the set priority. These parameters include:
1. POWER: The ESP32 is being powered from a UPS Battery. On top of that, the ESP32 must also monitor the AC input to the UPS.
2. TRIP: The ESP32 is controlling Motors through Relays. The TRIP signal will monitor and indicate overcurrent being drained by the motor.
3. LOW CURRENT: The ESP32 will monitor and report if the Motor is operating in a LOW CURRENT State.
4. DRY RUN: The ESP32 will monitor and report if the motor is in Dry Run state i.e motor is operating without a Load.

All the data of these tasks will be fetched and pushed to a topic on AWS over MQTT with UNIX Timestamp. In case of no internet connection, data will be stored locally on the SD Card module with a UNIX timestamp and will be synced with the cloud once internet connection is restored.

##### Schedule Download and Store Task

This task will involve donwloading a JSON Schedule from AWS over MQTT. The schedule will be stored in the ESP32 EEPROM / SPIFFS. A basic look of schedule will be:
{"Schedule" : Schdule01 ["ChannelID" : 100, "Day" : "Monday", "Start Time(epoch)" : 1625203013, "Duration(minutes)" : 60, "ConfigON" : 1121, "ConfigOFF" : 1020], Schdule02 ["ChannelID" : 100, "Day" : "Tuesday", "Start Time(epoch)" : 1625203013, "Duration(minutes)" : 60, "ConfigON" : 1121, "ConfigOFF" : 1020]}

1. There can be two channel IDs
2. Start Time is basically the minimum time at which the schedule should start executing. This is because the schedule will not only depend on start time information but will also depend on health parameter information. Two conditions must be TRUE in order for the schedule to execute:

	a.	The minimum start time for the day has been reached.
	b.	All the health parameters are stable.

In case any of the health parameter is unstable but the schedule start time has been reached, schedule will NOT execute. However, once the parameters are stable, the schedule will then execute for the duration that is present. In case the health goes unstable during schedule execution, the device should remember for how many minutes the schedule was executed and how much execution remains. It will then start executing again, the remaining duration. A JSON report for each schdule will be prepared that will hold the schedule exection information as well as the Relay and Power status for that day.

##### Time Sync Task

A simple task to sync the time with either Internet or GPS. The local device time will run on ESP32 Internal RTC but will sync itself with UTC based on defined priority.


#### Suggestions

1. The health parameter sync task should be on a separate thread as it will take long time if the data packets are large.
2. The data should be sorted on the cloud using timestamp based sorting. This can be handled later by the database developer.

