/***********************************************************************	
* Author:	Zak Rowland
* Filename:		ESP32_Async_Web_Server.ino
* Date Created:	2/8/2020
* Modifications:	
*
************************************************************************
*
* Project: Electronic Directory Board
* 
* Overview: This Arduino project hosts the HTML and CSS files and
*			controls (8) RGB LEDs based on data received from the client.
* 			The data is stored in 2D arrays on the Arduino. 
*	
*
* Input: The inputs are the dropdown lists from the HTML website.
*
*
* Output: The output is (8) different digital RGB (3) signals for 
* 		  (24) total bits of data which depends on the data stored 
*		  from the HTML website. These data bits will be divided into
*		  three shift registers.
*	
*
************************************************************************/
/***********************************************************************
* To-do / ideas:
* ----------------------------------------------------------------------
* - Timed auto logout
* - Buttons to save/restore all stored data to SPIFFS,
*	  that way if system crashes the previous data can be loaded all at once
* - Hour blocks update at X:50 instead of X:00
***********************************************************************/

//Import required libraries
#include "WiFi.h"
#include "WiFiUdp.h"
//https://github.com/me-no-dev/ESPAsyncWebServer
//https://github.com/me-no-dev/AsyncTCP
#include "ESPAsyncWebServer.h"
//https://github.com/me-no-dev/arduino-esp32fs-plugin/releases/
#include "SPIFFS.h"
//Tools > Manage Libaries > All > Search NTP
#include "NTPClient.h"
//This library is for SHA hashing
#include "mbedtls/md.h" 

#include "HTML_constants.h"

//Useful defines for accessing days_of_the_week array
#define DAY_SUNDAY 0
#define DAY_MONDAY 1
#define DAY_TUESDAY 2
#define DAY_WEDNESDAY 3
#define DAY_THURSDAY 4
#define DAY_FRIDAY 5
#define DAY_SATURDAY 6

//Useful defines for accessing hour_blocks array
#define HOUR_8TO9 0
#define HOUR_9TO10 1
#define HOUR_10TO11 2
#define HOUR_11TO12 3
#define HOUR_12TO1 4
#define HOUR_1TO2 5
#define HOUR_2TO3 6
#define HOUR_3TO4 7
#define HOUR_4TO5 8
#define HOUR_5TO6 9

//Useful defines for accessing room_states array
#define ROOM_OPEN 0
#define ROOM_BOOKED 1
#define ROOM_CANCELLED 2
#define ROOM_LATE 3

//Useful defines for accessing weekday states
#define PV_104 0
#define PV_107 1
#define PV_108 2
#define PV_110 3
#define PV_119 4
#define PV_120 5
#define PV_147 6
#define PV_153 7

#define MAX_IP_LENGTH 15 //Longest IPv4 address with periods is 15 characters

#define MAX_TIME 4294967295
#define MOTION_DELAY 1000 //60,000ms in 1 minutes, 300,000ms in 5 minutes

#define RGB_RED B100
#define RGB_GREEN B010
#define RGB_YELLOW B110
#define RGB_PINK B101

#define MAX_LATE_OR_CANCEL 80

const uint8_t cd4021_clock_pin = 27; //Pin 27 of ESP32 Arduino connected to Pin 10 of CD4021BE
const uint8_t cd4021_data_pin = 14; //Pin 14 of ESP32 Arduino connected to Pin 3 of CD4021BE
const uint8_t cd4021_latch_pin = 32; //Pin 32 of ESP32 Arduino connected to Pin 9 of CD4021BE

const uint8_t tpic6c595_data_pin = 33; //Pin 33 of ESP32 Arduino connected to Pin 2 of TPIC6C595
const uint8_t tpic6c595_clock_pin = 25; //Pin 25 of ESP32 Arduino connected to Pin 15 of TPIC6C595
const uint8_t tpic6c595_latch_pin = 26; //Pin 26 of ESP32 Arduino connected to Pin 10 of TPIC6C595

const uint8_t rgb_tpic6c595_data_pin = 4;
const uint8_t rgb_tpic6c595_clock_pin = 2;
const uint8_t rgb_tpic6c595_latch_pin = 15;

byte data_from_sensors = 0; //Used to hold data from CD4021BE
bool active[8] = {false, false, false, false, false, false, false, false};
unsigned long start_time[8] = {0, 0, 0, 0, 0, 0, 0, 0};
unsigned long current = 0;
byte send_to_led = 0;
//int rgb_data = 0; //Data for 24bit daisy-chained shift registers

//Network credentials
const char* ssid = "Favell Gift Shop";
const char* password = "71255972";

//Pacific Time is UTC-8 = -8*60*60 = -28,800 ... this is the offset in seconds
const long utc_offset_in_seconds = -28800;

const char room_names[8][6] = {"PV104", "PV107", "PV108", "PV110", "PV119", "PV120", "PV147", "PV153"};
const char days_of_the_week[7][10] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
const char hour_blocks[10][13] = {"8AM to 9AM", "9AM to 10AM", "10AM to 11AM", "11AM to 12PM", "12PM to 1PM", "1PM to 2PM",
                                  "2PM to 3PM", "3PM to 4PM", "4PM to 5PM", "5PM to 6PM"};
const char room_states[4][10] = {"Open", "Booked", "Cancelled", "Late"}; 

char hashed_username[64] = {0}; //Space for the encrypted result of a received username
char hashed_password[64] = {0}; //Space for the encrypted result of a received password
char connected_ip[MAX_IP_LENGTH + 1] = "NONE"; //Space for the current IP address controlling the server

mbedtls_md_context_t ctx; //Context used for SHA encryption functions
mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256; //SHA type is 256
bool user_match = false; //Tracks if received username matches
bool pass_match = false; //Tracks if received password matches
bool tried_but_failed = false; //Tracks if someone attempted a login

uint8_t current_displayed_day = 0; //Current day being displayed stored as an int, refer to defines
uint8_t current_displayed_hour = 0; //Current hour being displayed stored as an int, refer to defines

String day_state; //Current day being displayed stored as a string, refer to defines and constants
String hour_state; //Current hour being displayed stored as a string, refer to defines and constants

uint8_t monday_state_data[10][8] = {{0}}; //Monday - (10) different hour blocks and (8) labs
uint8_t tuesday_state_data[10][8] = {{0}}; //Tuesday - (10) different hour blocks and (8) labs
uint8_t wednesday_state_data[10][8] = {{0}}; //Wednesday - (10) different hour blocks and (8) labs
uint8_t thursday_state_data[10][8] = {{0}}; //Thursday - (10) different hour blocks and (8) labs
uint8_t friday_state_data[10][8] = {{0}}; //Friday - (10) different hour blocks and (8) labs
uint8_t current_states[8] = {0}; //Current room states being displayed for (8) labs

uint8_t late_or_cancelled_day[MAX_LATE_OR_CANCEL] = {0}; //Up to 80 different rooms can be late/cancelled at a time (increase define size if more is needed)
uint8_t late_or_cancelled_hour[MAX_LATE_OR_CANCEL] = {0};
uint8_t late_or_cancelled_room[MAX_LATE_OR_CANCEL] = {0};
uint8_t room_previous_state[MAX_LATE_OR_CANCEL] = {0}; //Store previous room state to be restored later
uint8_t late_or_cancelled_count = 0; //Track the number of rooms that are late or cancelled, can be used for indexing
uint8_t current_ntp_hour = 0; //Track the current hour via NTP
uint8_t previous_ntp_hour = 0; //Track the previous hour

//Define NTP Client to get current time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utc_offset_in_seconds);

AsyncWebServer server(80); //Create AsyncWebServer object on port 80

/**********************************************************************  
* Name: processor
* 
* Purpose: This function replaces placeholders (%var%) on the web page 
*          with current data.
*
* Precondition: The placeholders on the website (e.g. %DAY%) are
*               displayed as-is.
*
* Postcondition: The placeholders on the website (e.g. %DAY) are
*                replaced by the stored value (e.g. day_state).
*      
************************************************************************/
String processor(const String& var){
  if(var == "NTP_HOUR") return hour_blocks[current_ntp_hour]; //Display current hour
  //---------------------------- Modify index.html before serving ----------------------------
  if(var == "DAY") return day_state;
  if(var == "HOUR") return hour_state;
  if(var == "PV104STAT") return room_states[current_states[PV_104]];
  if(var == "PV107STAT") return room_states[current_states[PV_107]];
  if(var == "PV108STAT") return room_states[current_states[PV_108]];
  if(var == "PV110STAT") return room_states[current_states[PV_110]];
  if(var == "PV119STAT") return room_states[current_states[PV_119]];
  if(var == "PV120STAT") return room_states[current_states[PV_120]];
  if(var == "PV147STAT") return room_states[current_states[PV_147]];
  if(var == "PV153STAT") return room_states[current_states[PV_153]];
  //-------------------------------------------------------------------------------------------
  //---------------------------- Modify monday.html before serving ----------------------------
  if(var == "PV104STAT_MON_8TO9") return room_states[monday_state_data[HOUR_8TO9][PV_104]];
  if(var == "PV104STAT_MON_9TO10") return room_states[monday_state_data[HOUR_9TO10][PV_104]];
  if(var == "PV104STAT_MON_10TO11") return room_states[monday_state_data[HOUR_10TO11][PV_104]];
  if(var == "PV104STAT_MON_11TO12") return room_states[monday_state_data[HOUR_11TO12][PV_104]];
  if(var == "PV104STAT_MON_12TO1") return room_states[monday_state_data[HOUR_12TO1][PV_104]];
  if(var == "PV104STAT_MON_1TO2") return room_states[monday_state_data[HOUR_1TO2][PV_104]];
  if(var == "PV104STAT_MON_2TO3") return room_states[monday_state_data[HOUR_2TO3][PV_104]];
  if(var == "PV104STAT_MON_3TO4") return room_states[monday_state_data[HOUR_3TO4][PV_104]];
  if(var == "PV104STAT_MON_4TO5") return room_states[monday_state_data[HOUR_4TO5][PV_104]];
  if(var == "PV104STAT_MON_5TO6") return room_states[monday_state_data[HOUR_5TO6][PV_104]];
  
  if(var == "PV107STAT_MON_8TO9") return room_states[monday_state_data[HOUR_8TO9][PV_107]];
  if(var == "PV107STAT_MON_9TO10") return room_states[monday_state_data[HOUR_9TO10][PV_107]];
  if(var == "PV107STAT_MON_10TO11") return room_states[monday_state_data[HOUR_10TO11][PV_107]];
  if(var == "PV107STAT_MON_11TO12") return room_states[monday_state_data[HOUR_11TO12][PV_107]];
  if(var == "PV107STAT_MON_12TO1") return room_states[monday_state_data[HOUR_12TO1][PV_107]];
  if(var == "PV107STAT_MON_1TO2") return room_states[monday_state_data[HOUR_1TO2][PV_107]];
  if(var == "PV107STAT_MON_2TO3") return room_states[monday_state_data[HOUR_2TO3][PV_107]];
  if(var == "PV107STAT_MON_3TO4") return room_states[monday_state_data[HOUR_3TO4][PV_107]];
  if(var == "PV107STAT_MON_4TO5") return room_states[monday_state_data[HOUR_4TO5][PV_107]];
  if(var == "PV107STAT_MON_5TO6") return room_states[monday_state_data[HOUR_5TO6][PV_107]];
  
  if(var == "PV108STAT_MON_8TO9") return room_states[monday_state_data[HOUR_8TO9][PV_108]];
  if(var == "PV108STAT_MON_9TO10") return room_states[monday_state_data[HOUR_9TO10][PV_108]];
  if(var == "PV108STAT_MON_10TO11") return room_states[monday_state_data[HOUR_10TO11][PV_108]];
  if(var == "PV108STAT_MON_11TO12") return room_states[monday_state_data[HOUR_11TO12][PV_108]];
  if(var == "PV108STAT_MON_12TO1") return room_states[monday_state_data[HOUR_12TO1][PV_108]];
  if(var == "PV108STAT_MON_1TO2") return room_states[monday_state_data[HOUR_1TO2][PV_108]];
  if(var == "PV108STAT_MON_2TO3") return room_states[monday_state_data[HOUR_2TO3][PV_108]];
  if(var == "PV108STAT_MON_3TO4") return room_states[monday_state_data[HOUR_3TO4][PV_108]];
  if(var == "PV108STAT_MON_4TO5") return room_states[monday_state_data[HOUR_4TO5][PV_108]];
  if(var == "PV108STAT_MON_5TO6") return room_states[monday_state_data[HOUR_5TO6][PV_108]];
  
  if(var == "PV110STAT_MON_8TO9") return room_states[monday_state_data[HOUR_8TO9][PV_110]];
  if(var == "PV110STAT_MON_9TO10") return room_states[monday_state_data[HOUR_9TO10][PV_110]];
  if(var == "PV110STAT_MON_10TO11") return room_states[monday_state_data[HOUR_10TO11][PV_110]];
  if(var == "PV110STAT_MON_11TO12") return room_states[monday_state_data[HOUR_11TO12][PV_110]];
  if(var == "PV110STAT_MON_12TO1") return room_states[monday_state_data[HOUR_12TO1][PV_110]];
  if(var == "PV110STAT_MON_1TO2") return room_states[monday_state_data[HOUR_1TO2][PV_110]];
  if(var == "PV110STAT_MON_2TO3") return room_states[monday_state_data[HOUR_2TO3][PV_110]];
  if(var == "PV110STAT_MON_3TO4") return room_states[monday_state_data[HOUR_3TO4][PV_110]];
  if(var == "PV110STAT_MON_4TO5") return room_states[monday_state_data[HOUR_4TO5][PV_110]];
  if(var == "PV110STAT_MON_5TO6") return room_states[monday_state_data[HOUR_5TO6][PV_110]];
  
  if(var == "PV119STAT_MON_8TO9") return room_states[monday_state_data[HOUR_8TO9][PV_119]];
  if(var == "PV119STAT_MON_9TO10") return room_states[monday_state_data[HOUR_9TO10][PV_119]];
  if(var == "PV119STAT_MON_10TO11") return room_states[monday_state_data[HOUR_10TO11][PV_119]];
  if(var == "PV119STAT_MON_11TO12") return room_states[monday_state_data[HOUR_11TO12][PV_119]];
  if(var == "PV119STAT_MON_12TO1") return room_states[monday_state_data[HOUR_12TO1][PV_119]];
  if(var == "PV119STAT_MON_1TO2") return room_states[monday_state_data[HOUR_1TO2][PV_119]];
  if(var == "PV119STAT_MON_2TO3") return room_states[monday_state_data[HOUR_2TO3][PV_119]];
  if(var == "PV119STAT_MON_3TO4") return room_states[monday_state_data[HOUR_3TO4][PV_119]];
  if(var == "PV119STAT_MON_4TO5") return room_states[monday_state_data[HOUR_4TO5][PV_119]];
  if(var == "PV119STAT_MON_5TO6") return room_states[monday_state_data[HOUR_5TO6][PV_119]];
  
  if(var == "PV120STAT_MON_8TO9") return room_states[monday_state_data[HOUR_8TO9][PV_120]];
  if(var == "PV120STAT_MON_9TO10") return room_states[monday_state_data[HOUR_9TO10][PV_120]];
  if(var == "PV120STAT_MON_10TO11") return room_states[monday_state_data[HOUR_10TO11][PV_120]];
  if(var == "PV120STAT_MON_11TO12") return room_states[monday_state_data[HOUR_11TO12][PV_120]];
  if(var == "PV120STAT_MON_12TO1") return room_states[monday_state_data[HOUR_12TO1][PV_120]];
  if(var == "PV120STAT_MON_1TO2") return room_states[monday_state_data[HOUR_1TO2][PV_120]];
  if(var == "PV120STAT_MON_2TO3") return room_states[monday_state_data[HOUR_2TO3][PV_120]];
  if(var == "PV120STAT_MON_3TO4") return room_states[monday_state_data[HOUR_3TO4][PV_120]];
  if(var == "PV120STAT_MON_4TO5") return room_states[monday_state_data[HOUR_4TO5][PV_120]];
  if(var == "PV120STAT_MON_5TO6") return room_states[monday_state_data[HOUR_5TO6][PV_120]];
  
  if(var == "PV147STAT_MON_8TO9") return room_states[monday_state_data[HOUR_8TO9][PV_147]];
  if(var == "PV147STAT_MON_9TO10") return room_states[monday_state_data[HOUR_9TO10][PV_147]];
  if(var == "PV147STAT_MON_10TO11") return room_states[monday_state_data[HOUR_10TO11][PV_147]];
  if(var == "PV147STAT_MON_11TO12") return room_states[monday_state_data[HOUR_11TO12][PV_147]];
  if(var == "PV147STAT_MON_12TO1") return room_states[monday_state_data[HOUR_12TO1][PV_147]];
  if(var == "PV147STAT_MON_1TO2") return room_states[monday_state_data[HOUR_1TO2][PV_147]];
  if(var == "PV147STAT_MON_2TO3") return room_states[monday_state_data[HOUR_2TO3][PV_147]];
  if(var == "PV147STAT_MON_3TO4") return room_states[monday_state_data[HOUR_3TO4][PV_147]];
  if(var == "PV147STAT_MON_4TO5") return room_states[monday_state_data[HOUR_4TO5][PV_147]];
  if(var == "PV147STAT_MON_5TO6") return room_states[monday_state_data[HOUR_5TO6][PV_147]];
  
  if(var == "PV153STAT_MON_8TO9") return room_states[monday_state_data[HOUR_8TO9][PV_153]];
  if(var == "PV153STAT_MON_9TO10") return room_states[monday_state_data[HOUR_9TO10][PV_153]];
  if(var == "PV153STAT_MON_10TO11") return room_states[monday_state_data[HOUR_10TO11][PV_153]];
  if(var == "PV153STAT_MON_11TO12") return room_states[monday_state_data[HOUR_11TO12][PV_153]];
  if(var == "PV153STAT_MON_12TO1") return room_states[monday_state_data[HOUR_12TO1][PV_153]];
  if(var == "PV153STAT_MON_1TO2") return room_states[monday_state_data[HOUR_1TO2][PV_153]];
  if(var == "PV153STAT_MON_2TO3") return room_states[monday_state_data[HOUR_2TO3][PV_153]];
  if(var == "PV153STAT_MON_3TO4") return room_states[monday_state_data[HOUR_3TO4][PV_153]];
  if(var == "PV153STAT_MON_4TO5") return room_states[monday_state_data[HOUR_4TO5][PV_153]];
  if(var == "PV153STAT_MON_5TO6") return room_states[monday_state_data[HOUR_5TO6][PV_153]];
  //-------------------------------------------------------------------------------------------
  //---------------------------- Modify tuesday.html before serving ----------------------------
  if(var == "PV104STAT_TUES_8TO9") return room_states[tuesday_state_data[HOUR_8TO9][PV_104]];
  if(var == "PV104STAT_TUES_9TO10") return room_states[tuesday_state_data[HOUR_9TO10][PV_104]];
  if(var == "PV104STAT_TUES_10TO11") return room_states[tuesday_state_data[HOUR_10TO11][PV_104]];
  if(var == "PV104STAT_TUES_11TO12") return room_states[tuesday_state_data[HOUR_11TO12][PV_104]];
  if(var == "PV104STAT_TUES_12TO1") return room_states[tuesday_state_data[HOUR_12TO1][PV_104]];
  if(var == "PV104STAT_TUES_1TO2") return room_states[tuesday_state_data[HOUR_1TO2][PV_104]];
  if(var == "PV104STAT_TUES_2TO3") return room_states[tuesday_state_data[HOUR_2TO3][PV_104]];
  if(var == "PV104STAT_TUES_3TO4") return room_states[tuesday_state_data[HOUR_3TO4][PV_104]];
  if(var == "PV104STAT_TUES_4TO5") return room_states[tuesday_state_data[HOUR_4TO5][PV_104]];
  if(var == "PV104STAT_TUES_5TO6") return room_states[tuesday_state_data[HOUR_5TO6][PV_104]];
  
  if(var == "PV107STAT_TUES_8TO9") return room_states[tuesday_state_data[HOUR_8TO9][PV_107]];
  if(var == "PV107STAT_TUES_9TO10") return room_states[tuesday_state_data[HOUR_9TO10][PV_107]];
  if(var == "PV107STAT_TUES_10TO11") return room_states[tuesday_state_data[HOUR_10TO11][PV_107]];
  if(var == "PV107STAT_TUES_11TO12") return room_states[tuesday_state_data[HOUR_11TO12][PV_107]];
  if(var == "PV107STAT_TUES_12TO1") return room_states[tuesday_state_data[HOUR_12TO1][PV_107]];
  if(var == "PV107STAT_TUES_1TO2") return room_states[tuesday_state_data[HOUR_1TO2][PV_107]];
  if(var == "PV107STAT_TUES_2TO3") return room_states[tuesday_state_data[HOUR_2TO3][PV_107]];
  if(var == "PV107STAT_TUES_3TO4") return room_states[tuesday_state_data[HOUR_3TO4][PV_107]];
  if(var == "PV107STAT_TUES_4TO5") return room_states[tuesday_state_data[HOUR_4TO5][PV_107]];
  if(var == "PV107STAT_TUES_5TO6") return room_states[tuesday_state_data[HOUR_5TO6][PV_107]];
  
  if(var == "PV108STAT_TUES_8TO9") return room_states[tuesday_state_data[HOUR_8TO9][PV_108]];
  if(var == "PV108STAT_TUES_9TO10") return room_states[tuesday_state_data[HOUR_9TO10][PV_108]];
  if(var == "PV108STAT_TUES_10TO11") return room_states[tuesday_state_data[HOUR_10TO11][PV_108]];
  if(var == "PV108STAT_TUES_11TO12") return room_states[tuesday_state_data[HOUR_11TO12][PV_108]];
  if(var == "PV108STAT_TUES_12TO1") return room_states[tuesday_state_data[HOUR_12TO1][PV_108]];
  if(var == "PV108STAT_TUES_1TO2") return room_states[tuesday_state_data[HOUR_1TO2][PV_108]];
  if(var == "PV108STAT_TUES_2TO3") return room_states[tuesday_state_data[HOUR_2TO3][PV_108]];
  if(var == "PV108STAT_TUES_3TO4") return room_states[tuesday_state_data[HOUR_3TO4][PV_108]];
  if(var == "PV108STAT_TUES_4TO5") return room_states[tuesday_state_data[HOUR_4TO5][PV_108]];
  if(var == "PV108STAT_TUES_5TO6") return room_states[tuesday_state_data[HOUR_5TO6][PV_108]];
  
  if(var == "PV110STAT_TUES_8TO9") return room_states[tuesday_state_data[HOUR_8TO9][PV_110]];
  if(var == "PV110STAT_TUES_9TO10") return room_states[tuesday_state_data[HOUR_9TO10][PV_110]];
  if(var == "PV110STAT_TUES_10TO11") return room_states[tuesday_state_data[HOUR_10TO11][PV_110]];
  if(var == "PV110STAT_TUES_11TO12") return room_states[tuesday_state_data[HOUR_11TO12][PV_110]];
  if(var == "PV110STAT_TUES_12TO1") return room_states[tuesday_state_data[HOUR_12TO1][PV_110]];
  if(var == "PV110STAT_TUES_1TO2") return room_states[tuesday_state_data[HOUR_1TO2][PV_110]];
  if(var == "PV110STAT_TUES_2TO3") return room_states[tuesday_state_data[HOUR_2TO3][PV_110]];
  if(var == "PV110STAT_TUES_3TO4") return room_states[tuesday_state_data[HOUR_3TO4][PV_110]];
  if(var == "PV110STAT_TUES_4TO5") return room_states[tuesday_state_data[HOUR_4TO5][PV_110]];
  if(var == "PV110STAT_TUES_5TO6") return room_states[tuesday_state_data[HOUR_5TO6][PV_110]];
  
  if(var == "PV119STAT_TUES_8TO9") return room_states[tuesday_state_data[HOUR_8TO9][PV_119]];
  if(var == "PV119STAT_TUES_9TO10") return room_states[tuesday_state_data[HOUR_9TO10][PV_119]];
  if(var == "PV119STAT_TUES_10TO11") return room_states[tuesday_state_data[HOUR_10TO11][PV_119]];
  if(var == "PV119STAT_TUES_11TO12") return room_states[tuesday_state_data[HOUR_11TO12][PV_119]];
  if(var == "PV119STAT_TUES_12TO1") return room_states[tuesday_state_data[HOUR_12TO1][PV_119]];
  if(var == "PV119STAT_TUES_1TO2") return room_states[tuesday_state_data[HOUR_1TO2][PV_119]];
  if(var == "PV119STAT_TUES_2TO3") return room_states[tuesday_state_data[HOUR_2TO3][PV_119]];
  if(var == "PV119STAT_TUES_3TO4") return room_states[tuesday_state_data[HOUR_3TO4][PV_119]];
  if(var == "PV119STAT_TUES_4TO5") return room_states[tuesday_state_data[HOUR_4TO5][PV_119]];
  if(var == "PV119STAT_TUES_5TO6") return room_states[tuesday_state_data[HOUR_5TO6][PV_119]];
  
  if(var == "PV120STAT_TUES_8TO9") return room_states[tuesday_state_data[HOUR_8TO9][PV_120]];
  if(var == "PV120STAT_TUES_9TO10") return room_states[tuesday_state_data[HOUR_9TO10][PV_120]];
  if(var == "PV120STAT_TUES_10TO11") return room_states[tuesday_state_data[HOUR_10TO11][PV_120]];
  if(var == "PV120STAT_TUES_11TO12") return room_states[tuesday_state_data[HOUR_11TO12][PV_120]];
  if(var == "PV120STAT_TUES_12TO1") return room_states[tuesday_state_data[HOUR_12TO1][PV_120]];
  if(var == "PV120STAT_TUES_1TO2") return room_states[tuesday_state_data[HOUR_1TO2][PV_120]];
  if(var == "PV120STAT_TUES_2TO3") return room_states[tuesday_state_data[HOUR_2TO3][PV_120]];
  if(var == "PV120STAT_TUES_3TO4") return room_states[tuesday_state_data[HOUR_3TO4][PV_120]];
  if(var == "PV120STAT_TUES_4TO5") return room_states[tuesday_state_data[HOUR_4TO5][PV_120]];
  if(var == "PV120STAT_TUES_5TO6") return room_states[tuesday_state_data[HOUR_5TO6][PV_120]];
  
  if(var == "PV147STAT_TUES_8TO9") return room_states[tuesday_state_data[HOUR_8TO9][PV_147]];
  if(var == "PV147STAT_TUES_9TO10") return room_states[tuesday_state_data[HOUR_9TO10][PV_147]];
  if(var == "PV147STAT_TUES_10TO11") return room_states[tuesday_state_data[HOUR_10TO11][PV_147]];
  if(var == "PV147STAT_TUES_11TO12") return room_states[tuesday_state_data[HOUR_11TO12][PV_147]];
  if(var == "PV147STAT_TUES_12TO1") return room_states[tuesday_state_data[HOUR_12TO1][PV_147]];
  if(var == "PV147STAT_TUES_1TO2") return room_states[tuesday_state_data[HOUR_1TO2][PV_147]];
  if(var == "PV147STAT_TUES_2TO3") return room_states[tuesday_state_data[HOUR_2TO3][PV_147]];
  if(var == "PV147STAT_TUES_3TO4") return room_states[tuesday_state_data[HOUR_3TO4][PV_147]];
  if(var == "PV147STAT_TUES_4TO5") return room_states[tuesday_state_data[HOUR_4TO5][PV_147]];
  if(var == "PV147STAT_TUES_5TO6") return room_states[tuesday_state_data[HOUR_5TO6][PV_147]];
  
  if(var == "PV153STAT_TUES_8TO9") return room_states[tuesday_state_data[HOUR_8TO9][PV_153]];
  if(var == "PV153STAT_TUES_9TO10") return room_states[tuesday_state_data[HOUR_9TO10][PV_153]];
  if(var == "PV153STAT_TUES_10TO11") return room_states[tuesday_state_data[HOUR_10TO11][PV_153]];
  if(var == "PV153STAT_TUES_11TO12") return room_states[tuesday_state_data[HOUR_11TO12][PV_153]];
  if(var == "PV153STAT_TUES_12TO1") return room_states[tuesday_state_data[HOUR_12TO1][PV_153]];
  if(var == "PV153STAT_TUES_1TO2") return room_states[tuesday_state_data[HOUR_1TO2][PV_153]];
  if(var == "PV153STAT_TUES_2TO3") return room_states[tuesday_state_data[HOUR_2TO3][PV_153]];
  if(var == "PV153STAT_TUES_3TO4") return room_states[tuesday_state_data[HOUR_3TO4][PV_153]];
  if(var == "PV153STAT_TUES_4TO5") return room_states[tuesday_state_data[HOUR_4TO5][PV_153]];
  if(var == "PV153STAT_TUES_5TO6") return room_states[tuesday_state_data[HOUR_5TO6][PV_153]];
  //-------------------------------------------------------------------------------------------
  //---------------------------- Modify wednesday.html before serving ----------------------------
  if(var == "PV104STAT_WED_8TO9") return room_states[wednesday_state_data[HOUR_8TO9][PV_104]];
  if(var == "PV104STAT_WED_9TO10") return room_states[wednesday_state_data[HOUR_9TO10][PV_104]];
  if(var == "PV104STAT_WED_10TO11") return room_states[wednesday_state_data[HOUR_10TO11][PV_104]];
  if(var == "PV104STAT_WED_11TO12") return room_states[wednesday_state_data[HOUR_11TO12][PV_104]];
  if(var == "PV104STAT_WED_12TO1") return room_states[wednesday_state_data[HOUR_12TO1][PV_104]];
  if(var == "PV104STAT_WED_1TO2") return room_states[wednesday_state_data[HOUR_1TO2][PV_104]];
  if(var == "PV104STAT_WED_2TO3") return room_states[wednesday_state_data[HOUR_2TO3][PV_104]];
  if(var == "PV104STAT_WED_3TO4") return room_states[wednesday_state_data[HOUR_3TO4][PV_104]];
  if(var == "PV104STAT_WED_4TO5") return room_states[wednesday_state_data[HOUR_4TO5][PV_104]];
  if(var == "PV104STAT_WED_5TO6") return room_states[wednesday_state_data[HOUR_5TO6][PV_104]];
  
  if(var == "PV107STAT_WED_8TO9") return room_states[wednesday_state_data[HOUR_8TO9][PV_107]];
  if(var == "PV107STAT_WED_9TO10") return room_states[wednesday_state_data[HOUR_9TO10][PV_107]];
  if(var == "PV107STAT_WED_10TO11") return room_states[wednesday_state_data[HOUR_10TO11][PV_107]];
  if(var == "PV107STAT_WED_11TO12") return room_states[wednesday_state_data[HOUR_11TO12][PV_107]];
  if(var == "PV107STAT_WED_12TO1") return room_states[wednesday_state_data[HOUR_12TO1][PV_107]];
  if(var == "PV107STAT_WED_1TO2") return room_states[wednesday_state_data[HOUR_1TO2][PV_107]];
  if(var == "PV107STAT_WED_2TO3") return room_states[wednesday_state_data[HOUR_2TO3][PV_107]];
  if(var == "PV107STAT_WED_3TO4") return room_states[wednesday_state_data[HOUR_3TO4][PV_107]];
  if(var == "PV107STAT_WED_4TO5") return room_states[wednesday_state_data[HOUR_4TO5][PV_107]];
  if(var == "PV107STAT_WED_5TO6") return room_states[wednesday_state_data[HOUR_5TO6][PV_107]];
  
  if(var == "PV108STAT_WED_8TO9") return room_states[wednesday_state_data[HOUR_8TO9][PV_108]];
  if(var == "PV108STAT_WED_9TO10") return room_states[wednesday_state_data[HOUR_9TO10][PV_108]];
  if(var == "PV108STAT_WED_10TO11") return room_states[wednesday_state_data[HOUR_10TO11][PV_108]];
  if(var == "PV108STAT_WED_11TO12") return room_states[wednesday_state_data[HOUR_11TO12][PV_108]];
  if(var == "PV108STAT_WED_12TO1") return room_states[wednesday_state_data[HOUR_12TO1][PV_108]];
  if(var == "PV108STAT_WED_1TO2") return room_states[wednesday_state_data[HOUR_1TO2][PV_108]];
  if(var == "PV108STAT_WED_2TO3") return room_states[wednesday_state_data[HOUR_2TO3][PV_108]];
  if(var == "PV108STAT_WED_3TO4") return room_states[wednesday_state_data[HOUR_3TO4][PV_108]];
  if(var == "PV108STAT_WED_4TO5") return room_states[wednesday_state_data[HOUR_4TO5][PV_108]];
  if(var == "PV108STAT_WED_5TO6") return room_states[wednesday_state_data[HOUR_5TO6][PV_108]];
  
  if(var == "PV110STAT_WED_8TO9") return room_states[wednesday_state_data[HOUR_8TO9][PV_110]];
  if(var == "PV110STAT_WED_9TO10") return room_states[wednesday_state_data[HOUR_9TO10][PV_110]];
  if(var == "PV110STAT_WED_10TO11") return room_states[wednesday_state_data[HOUR_10TO11][PV_110]];
  if(var == "PV110STAT_WED_11TO12") return room_states[wednesday_state_data[HOUR_11TO12][PV_110]];
  if(var == "PV110STAT_WED_12TO1") return room_states[wednesday_state_data[HOUR_12TO1][PV_110]];
  if(var == "PV110STAT_WED_1TO2") return room_states[wednesday_state_data[HOUR_1TO2][PV_110]];
  if(var == "PV110STAT_WED_2TO3") return room_states[wednesday_state_data[HOUR_2TO3][PV_110]];
  if(var == "PV110STAT_WED_3TO4") return room_states[wednesday_state_data[HOUR_3TO4][PV_110]];
  if(var == "PV110STAT_WED_4TO5") return room_states[wednesday_state_data[HOUR_4TO5][PV_110]];
  if(var == "PV110STAT_WED_5TO6") return room_states[wednesday_state_data[HOUR_5TO6][PV_110]];
  
  if(var == "PV119STAT_WED_8TO9") return room_states[wednesday_state_data[HOUR_8TO9][PV_119]];
  if(var == "PV119STAT_WED_9TO10") return room_states[wednesday_state_data[HOUR_9TO10][PV_119]];
  if(var == "PV119STAT_WED_10TO11") return room_states[wednesday_state_data[HOUR_10TO11][PV_119]];
  if(var == "PV119STAT_WED_11TO12") return room_states[wednesday_state_data[HOUR_11TO12][PV_119]];
  if(var == "PV119STAT_WED_12TO1") return room_states[wednesday_state_data[HOUR_12TO1][PV_119]];
  if(var == "PV119STAT_WED_1TO2") return room_states[wednesday_state_data[HOUR_1TO2][PV_119]];
  if(var == "PV119STAT_WED_2TO3") return room_states[wednesday_state_data[HOUR_2TO3][PV_119]];
  if(var == "PV119STAT_WED_3TO4") return room_states[wednesday_state_data[HOUR_3TO4][PV_119]];
  if(var == "PV119STAT_WED_4TO5") return room_states[wednesday_state_data[HOUR_4TO5][PV_119]];
  if(var == "PV119STAT_WED_5TO6") return room_states[wednesday_state_data[HOUR_5TO6][PV_119]];
  
  if(var == "PV120STAT_WED_8TO9") return room_states[wednesday_state_data[HOUR_8TO9][PV_120]];
  if(var == "PV120STAT_WED_9TO10") return room_states[wednesday_state_data[HOUR_9TO10][PV_120]];
  if(var == "PV120STAT_WED_10TO11") return room_states[wednesday_state_data[HOUR_10TO11][PV_120]];
  if(var == "PV120STAT_WED_11TO12") return room_states[wednesday_state_data[HOUR_11TO12][PV_120]];
  if(var == "PV120STAT_WED_12TO1") return room_states[wednesday_state_data[HOUR_12TO1][PV_120]];
  if(var == "PV120STAT_WED_1TO2") return room_states[wednesday_state_data[HOUR_1TO2][PV_120]];
  if(var == "PV120STAT_WED_2TO3") return room_states[wednesday_state_data[HOUR_2TO3][PV_120]];
  if(var == "PV120STAT_WED_3TO4") return room_states[wednesday_state_data[HOUR_3TO4][PV_120]];
  if(var == "PV120STAT_WED_4TO5") return room_states[wednesday_state_data[HOUR_4TO5][PV_120]];
  if(var == "PV120STAT_WED_5TO6") return room_states[wednesday_state_data[HOUR_5TO6][PV_120]];
  
  if(var == "PV147STAT_WED_8TO9") return room_states[wednesday_state_data[HOUR_8TO9][PV_147]];
  if(var == "PV147STAT_WED_9TO10") return room_states[wednesday_state_data[HOUR_9TO10][PV_147]];
  if(var == "PV147STAT_WED_10TO11") return room_states[wednesday_state_data[HOUR_10TO11][PV_147]];
  if(var == "PV147STAT_WED_11TO12") return room_states[wednesday_state_data[HOUR_11TO12][PV_147]];
  if(var == "PV147STAT_WED_12TO1") return room_states[wednesday_state_data[HOUR_12TO1][PV_147]];
  if(var == "PV147STAT_WED_1TO2") return room_states[wednesday_state_data[HOUR_1TO2][PV_147]];
  if(var == "PV147STAT_WED_2TO3") return room_states[wednesday_state_data[HOUR_2TO3][PV_147]];
  if(var == "PV147STAT_WED_3TO4") return room_states[wednesday_state_data[HOUR_3TO4][PV_147]];
  if(var == "PV147STAT_WED_4TO5") return room_states[wednesday_state_data[HOUR_4TO5][PV_147]];
  if(var == "PV147STAT_WED_5TO6") return room_states[wednesday_state_data[HOUR_5TO6][PV_147]];
  
  if(var == "PV153STAT_WED_8TO9") return room_states[wednesday_state_data[HOUR_8TO9][PV_153]];
  if(var == "PV153STAT_WED_9TO10") return room_states[wednesday_state_data[HOUR_9TO10][PV_153]];
  if(var == "PV153STAT_WED_10TO11") return room_states[wednesday_state_data[HOUR_10TO11][PV_153]];
  if(var == "PV153STAT_WED_11TO12") return room_states[wednesday_state_data[HOUR_11TO12][PV_153]];
  if(var == "PV153STAT_WED_12TO1") return room_states[wednesday_state_data[HOUR_12TO1][PV_153]];
  if(var == "PV153STAT_WED_1TO2") return room_states[wednesday_state_data[HOUR_1TO2][PV_153]];
  if(var == "PV153STAT_WED_2TO3") return room_states[wednesday_state_data[HOUR_2TO3][PV_153]];
  if(var == "PV153STAT_WED_3TO4") return room_states[wednesday_state_data[HOUR_3TO4][PV_153]];
  if(var == "PV153STAT_WED_4TO5") return room_states[wednesday_state_data[HOUR_4TO5][PV_153]];
  if(var == "PV153STAT_WED_5TO6") return room_states[wednesday_state_data[HOUR_5TO6][PV_153]];
  //-------------------------------------------------------------------------------------------
  //---------------------------- Modify thursday.html before serving ----------------------------
  if(var == "PV104STAT_THURS_8TO9") return room_states[thursday_state_data[HOUR_8TO9][PV_104]];
  if(var == "PV104STAT_THURS_9TO10") return room_states[thursday_state_data[HOUR_9TO10][PV_104]];
  if(var == "PV104STAT_THURS_10TO11") return room_states[thursday_state_data[HOUR_10TO11][PV_104]];
  if(var == "PV104STAT_THURS_11TO12") return room_states[thursday_state_data[HOUR_11TO12][PV_104]];
  if(var == "PV104STAT_THURS_12TO1") return room_states[thursday_state_data[HOUR_12TO1][PV_104]];
  if(var == "PV104STAT_THURS_1TO2") return room_states[thursday_state_data[HOUR_1TO2][PV_104]];
  if(var == "PV104STAT_THURS_2TO3") return room_states[thursday_state_data[HOUR_2TO3][PV_104]];
  if(var == "PV104STAT_THURS_3TO4") return room_states[thursday_state_data[HOUR_3TO4][PV_104]];
  if(var == "PV104STAT_THURS_4TO5") return room_states[thursday_state_data[HOUR_4TO5][PV_104]];
  if(var == "PV104STAT_THURS_5TO6") return room_states[thursday_state_data[HOUR_5TO6][PV_104]];
  
  if(var == "PV107STAT_THURS_8TO9") return room_states[thursday_state_data[HOUR_8TO9][PV_107]];
  if(var == "PV107STAT_THURS_9TO10") return room_states[thursday_state_data[HOUR_9TO10][PV_107]];
  if(var == "PV107STAT_THURS_10TO11") return room_states[thursday_state_data[HOUR_10TO11][PV_107]];
  if(var == "PV107STAT_THURS_11TO12") return room_states[thursday_state_data[HOUR_11TO12][PV_107]];
  if(var == "PV107STAT_THURS_12TO1") return room_states[thursday_state_data[HOUR_12TO1][PV_107]];
  if(var == "PV107STAT_THURS_1TO2") return room_states[thursday_state_data[HOUR_1TO2][PV_107]];
  if(var == "PV107STAT_THURS_2TO3") return room_states[thursday_state_data[HOUR_2TO3][PV_107]];
  if(var == "PV107STAT_THURS_3TO4") return room_states[thursday_state_data[HOUR_3TO4][PV_107]];
  if(var == "PV107STAT_THURS_4TO5") return room_states[thursday_state_data[HOUR_4TO5][PV_107]];
  if(var == "PV107STAT_THURS_5TO6") return room_states[thursday_state_data[HOUR_5TO6][PV_107]];
  
  if(var == "PV108STAT_THURS_8TO9") return room_states[thursday_state_data[HOUR_8TO9][PV_108]];
  if(var == "PV108STAT_THURS_9TO10") return room_states[thursday_state_data[HOUR_9TO10][PV_108]];
  if(var == "PV108STAT_THURS_10TO11") return room_states[thursday_state_data[HOUR_10TO11][PV_108]];
  if(var == "PV108STAT_THURS_11TO12") return room_states[thursday_state_data[HOUR_11TO12][PV_108]];
  if(var == "PV108STAT_THURS_12TO1") return room_states[thursday_state_data[HOUR_12TO1][PV_108]];
  if(var == "PV108STAT_THURS_1TO2") return room_states[thursday_state_data[HOUR_1TO2][PV_108]];
  if(var == "PV108STAT_THURS_2TO3") return room_states[thursday_state_data[HOUR_2TO3][PV_108]];
  if(var == "PV108STAT_THURS_3TO4") return room_states[thursday_state_data[HOUR_3TO4][PV_108]];
  if(var == "PV108STAT_THURS_4TO5") return room_states[thursday_state_data[HOUR_4TO5][PV_108]];
  if(var == "PV108STAT_THURS_5TO6") return room_states[thursday_state_data[HOUR_5TO6][PV_108]];
  
  if(var == "PV110STAT_THURS_8TO9") return room_states[thursday_state_data[HOUR_8TO9][PV_110]];
  if(var == "PV110STAT_THURS_9TO10") return room_states[thursday_state_data[HOUR_9TO10][PV_110]];
  if(var == "PV110STAT_THURS_10TO11") return room_states[thursday_state_data[HOUR_10TO11][PV_110]];
  if(var == "PV110STAT_THURS_11TO12") return room_states[thursday_state_data[HOUR_11TO12][PV_110]];
  if(var == "PV110STAT_THURS_12TO1") return room_states[thursday_state_data[HOUR_12TO1][PV_110]];
  if(var == "PV110STAT_THURS_1TO2") return room_states[thursday_state_data[HOUR_1TO2][PV_110]];
  if(var == "PV110STAT_THURS_2TO3") return room_states[thursday_state_data[HOUR_2TO3][PV_110]];
  if(var == "PV110STAT_THURS_3TO4") return room_states[thursday_state_data[HOUR_3TO4][PV_110]];
  if(var == "PV110STAT_THURS_4TO5") return room_states[thursday_state_data[HOUR_4TO5][PV_110]];
  if(var == "PV110STAT_THURS_5TO6") return room_states[thursday_state_data[HOUR_5TO6][PV_110]];
  
  if(var == "PV119STAT_THURS_8TO9") return room_states[thursday_state_data[HOUR_8TO9][PV_119]];
  if(var == "PV119STAT_THURS_9TO10") return room_states[thursday_state_data[HOUR_9TO10][PV_119]];
  if(var == "PV119STAT_THURS_10TO11") return room_states[thursday_state_data[HOUR_10TO11][PV_119]];
  if(var == "PV119STAT_THURS_11TO12") return room_states[thursday_state_data[HOUR_11TO12][PV_119]];
  if(var == "PV119STAT_THURS_12TO1") return room_states[thursday_state_data[HOUR_12TO1][PV_119]];
  if(var == "PV119STAT_THURS_1TO2") return room_states[thursday_state_data[HOUR_1TO2][PV_119]];
  if(var == "PV119STAT_THURS_2TO3") return room_states[thursday_state_data[HOUR_2TO3][PV_119]];
  if(var == "PV119STAT_THURS_3TO4") return room_states[thursday_state_data[HOUR_3TO4][PV_119]];
  if(var == "PV119STAT_THURS_4TO5") return room_states[thursday_state_data[HOUR_4TO5][PV_119]];
  if(var == "PV119STAT_THURS_5TO6") return room_states[thursday_state_data[HOUR_5TO6][PV_119]];
  
  if(var == "PV120STAT_THURS_8TO9") return room_states[thursday_state_data[HOUR_8TO9][PV_120]];
  if(var == "PV120STAT_THURS_9TO10") return room_states[thursday_state_data[HOUR_9TO10][PV_120]];
  if(var == "PV120STAT_THURS_10TO11") return room_states[thursday_state_data[HOUR_10TO11][PV_120]];
  if(var == "PV120STAT_THURS_11TO12") return room_states[thursday_state_data[HOUR_11TO12][PV_120]];
  if(var == "PV120STAT_THURS_12TO1") return room_states[thursday_state_data[HOUR_12TO1][PV_120]];
  if(var == "PV120STAT_THURS_1TO2") return room_states[thursday_state_data[HOUR_1TO2][PV_120]];
  if(var == "PV120STAT_THURS_2TO3") return room_states[thursday_state_data[HOUR_2TO3][PV_120]];
  if(var == "PV120STAT_THURS_3TO4") return room_states[thursday_state_data[HOUR_3TO4][PV_120]];
  if(var == "PV120STAT_THURS_4TO5") return room_states[thursday_state_data[HOUR_4TO5][PV_120]];
  if(var == "PV120STAT_THURS_5TO6") return room_states[thursday_state_data[HOUR_5TO6][PV_120]];
  
  if(var == "PV147STAT_THURS_8TO9") return room_states[thursday_state_data[HOUR_8TO9][PV_147]];
  if(var == "PV147STAT_THURS_9TO10") return room_states[thursday_state_data[HOUR_9TO10][PV_147]];
  if(var == "PV147STAT_THURS_10TO11") return room_states[thursday_state_data[HOUR_10TO11][PV_147]];
  if(var == "PV147STAT_THURS_11TO12") return room_states[thursday_state_data[HOUR_11TO12][PV_147]];
  if(var == "PV147STAT_THURS_12TO1") return room_states[thursday_state_data[HOUR_12TO1][PV_147]];
  if(var == "PV147STAT_THURS_1TO2") return room_states[thursday_state_data[HOUR_1TO2][PV_147]];
  if(var == "PV147STAT_THURS_2TO3") return room_states[thursday_state_data[HOUR_2TO3][PV_147]];
  if(var == "PV147STAT_THURS_3TO4") return room_states[thursday_state_data[HOUR_3TO4][PV_147]];
  if(var == "PV147STAT_THURS_4TO5") return room_states[thursday_state_data[HOUR_4TO5][PV_147]];
  if(var == "PV147STAT_THURS_5TO6") return room_states[thursday_state_data[HOUR_5TO6][PV_147]];
  
  if(var == "PV153STAT_THURS_8TO9") return room_states[thursday_state_data[HOUR_8TO9][PV_153]];
  if(var == "PV153STAT_THURS_9TO10") return room_states[thursday_state_data[HOUR_9TO10][PV_153]];
  if(var == "PV153STAT_THURS_10TO11") return room_states[thursday_state_data[HOUR_10TO11][PV_153]];
  if(var == "PV153STAT_THURS_11TO12") return room_states[thursday_state_data[HOUR_11TO12][PV_153]];
  if(var == "PV153STAT_THURS_12TO1") return room_states[thursday_state_data[HOUR_12TO1][PV_153]];
  if(var == "PV153STAT_THURS_1TO2") return room_states[thursday_state_data[HOUR_1TO2][PV_153]];
  if(var == "PV153STAT_THURS_2TO3") return room_states[thursday_state_data[HOUR_2TO3][PV_153]];
  if(var == "PV153STAT_THURS_3TO4") return room_states[thursday_state_data[HOUR_3TO4][PV_153]];
  if(var == "PV153STAT_THURS_4TO5") return room_states[thursday_state_data[HOUR_4TO5][PV_153]];
  if(var == "PV153STAT_THURS_5TO6") return room_states[thursday_state_data[HOUR_5TO6][PV_153]];
  //-------------------------------------------------------------------------------------------
  //---------------------------- Modify friday.html before serving ----------------------------
  if(var == "PV104STAT_FRI_8TO9") return room_states[friday_state_data[HOUR_8TO9][PV_104]];
  if(var == "PV104STAT_FRI_9TO10") return room_states[friday_state_data[HOUR_9TO10][PV_104]];
  if(var == "PV104STAT_FRI_10TO11") return room_states[friday_state_data[HOUR_10TO11][PV_104]];
  if(var == "PV104STAT_FRI_11TO12") return room_states[friday_state_data[HOUR_11TO12][PV_104]];
  if(var == "PV104STAT_FRI_12TO1") return room_states[friday_state_data[HOUR_12TO1][PV_104]];
  if(var == "PV104STAT_FRI_1TO2") return room_states[friday_state_data[HOUR_1TO2][PV_104]];
  if(var == "PV104STAT_FRI_2TO3") return room_states[friday_state_data[HOUR_2TO3][PV_104]];
  if(var == "PV104STAT_FRI_3TO4") return room_states[friday_state_data[HOUR_3TO4][PV_104]];
  if(var == "PV104STAT_FRI_4TO5") return room_states[friday_state_data[HOUR_4TO5][PV_104]];
  if(var == "PV104STAT_FRI_5TO6") return room_states[friday_state_data[HOUR_5TO6][PV_104]];
  
  if(var == "PV107STAT_FRI_8TO9") return room_states[friday_state_data[HOUR_8TO9][PV_107]];
  if(var == "PV107STAT_FRI_9TO10") return room_states[friday_state_data[HOUR_9TO10][PV_107]];
  if(var == "PV107STAT_FRI_10TO11") return room_states[friday_state_data[HOUR_10TO11][PV_107]];
  if(var == "PV107STAT_FRI_11TO12") return room_states[friday_state_data[HOUR_11TO12][PV_107]];
  if(var == "PV107STAT_FRI_12TO1") return room_states[friday_state_data[HOUR_12TO1][PV_107]];
  if(var == "PV107STAT_FRI_1TO2") return room_states[friday_state_data[HOUR_1TO2][PV_107]];
  if(var == "PV107STAT_FRI_2TO3") return room_states[friday_state_data[HOUR_2TO3][PV_107]];
  if(var == "PV107STAT_FRI_3TO4") return room_states[friday_state_data[HOUR_3TO4][PV_107]];
  if(var == "PV107STAT_FRI_4TO5") return room_states[friday_state_data[HOUR_4TO5][PV_107]];
  if(var == "PV107STAT_FRI_5TO6") return room_states[friday_state_data[HOUR_5TO6][PV_107]];
  
  if(var == "PV108STAT_FRI_8TO9") return room_states[friday_state_data[HOUR_8TO9][PV_108]];
  if(var == "PV108STAT_FRI_9TO10") return room_states[friday_state_data[HOUR_9TO10][PV_108]];
  if(var == "PV108STAT_FRI_10TO11") return room_states[friday_state_data[HOUR_10TO11][PV_108]];
  if(var == "PV108STAT_FRI_11TO12") return room_states[friday_state_data[HOUR_11TO12][PV_108]];
  if(var == "PV108STAT_FRI_12TO1") return room_states[friday_state_data[HOUR_12TO1][PV_108]];
  if(var == "PV108STAT_FRI_1TO2") return room_states[friday_state_data[HOUR_1TO2][PV_108]];
  if(var == "PV108STAT_FRI_2TO3") return room_states[friday_state_data[HOUR_2TO3][PV_108]];
  if(var == "PV108STAT_FRI_3TO4") return room_states[friday_state_data[HOUR_3TO4][PV_108]];
  if(var == "PV108STAT_FRI_4TO5") return room_states[friday_state_data[HOUR_4TO5][PV_108]];
  if(var == "PV108STAT_FRI_5TO6") return room_states[friday_state_data[HOUR_5TO6][PV_108]];
  
  if(var == "PV110STAT_FRI_8TO9") return room_states[friday_state_data[HOUR_8TO9][PV_110]];
  if(var == "PV110STAT_FRI_9TO10") return room_states[friday_state_data[HOUR_9TO10][PV_110]];
  if(var == "PV110STAT_FRI_10TO11") return room_states[friday_state_data[HOUR_10TO11][PV_110]];
  if(var == "PV110STAT_FRI_11TO12") return room_states[friday_state_data[HOUR_11TO12][PV_110]];
  if(var == "PV110STAT_FRI_12TO1") return room_states[friday_state_data[HOUR_12TO1][PV_110]];
  if(var == "PV110STAT_FRI_1TO2") return room_states[friday_state_data[HOUR_1TO2][PV_110]];
  if(var == "PV110STAT_FRI_2TO3") return room_states[friday_state_data[HOUR_2TO3][PV_110]];
  if(var == "PV110STAT_FRI_3TO4") return room_states[friday_state_data[HOUR_3TO4][PV_110]];
  if(var == "PV110STAT_FRI_4TO5") return room_states[friday_state_data[HOUR_4TO5][PV_110]];
  if(var == "PV110STAT_FRI_5TO6") return room_states[friday_state_data[HOUR_5TO6][PV_110]];
  
  if(var == "PV119STAT_FRI_8TO9") return room_states[friday_state_data[HOUR_8TO9][PV_119]];
  if(var == "PV119STAT_FRI_9TO10") return room_states[friday_state_data[HOUR_9TO10][PV_119]];
  if(var == "PV119STAT_FRI_10TO11") return room_states[friday_state_data[HOUR_10TO11][PV_119]];
  if(var == "PV119STAT_FRI_11TO12") return room_states[friday_state_data[HOUR_11TO12][PV_119]];
  if(var == "PV119STAT_FRI_12TO1") return room_states[friday_state_data[HOUR_12TO1][PV_119]];
  if(var == "PV119STAT_FRI_1TO2") return room_states[friday_state_data[HOUR_1TO2][PV_119]];
  if(var == "PV119STAT_FRI_2TO3") return room_states[friday_state_data[HOUR_2TO3][PV_119]];
  if(var == "PV119STAT_FRI_3TO4") return room_states[friday_state_data[HOUR_3TO4][PV_119]];
  if(var == "PV119STAT_FRI_4TO5") return room_states[friday_state_data[HOUR_4TO5][PV_119]];
  if(var == "PV119STAT_FRI_5TO6") return room_states[friday_state_data[HOUR_5TO6][PV_119]];
  
  if(var == "PV120STAT_FRI_8TO9") return room_states[friday_state_data[HOUR_8TO9][PV_120]];
  if(var == "PV120STAT_FRI_9TO10") return room_states[friday_state_data[HOUR_9TO10][PV_120]];
  if(var == "PV120STAT_FRI_10TO11") return room_states[friday_state_data[HOUR_10TO11][PV_120]];
  if(var == "PV120STAT_FRI_11TO12") return room_states[friday_state_data[HOUR_11TO12][PV_120]];
  if(var == "PV120STAT_FRI_12TO1") return room_states[friday_state_data[HOUR_12TO1][PV_120]];
  if(var == "PV120STAT_FRI_1TO2") return room_states[friday_state_data[HOUR_1TO2][PV_120]];
  if(var == "PV120STAT_FRI_2TO3") return room_states[friday_state_data[HOUR_2TO3][PV_120]];
  if(var == "PV120STAT_FRI_3TO4") return room_states[friday_state_data[HOUR_3TO4][PV_120]];
  if(var == "PV120STAT_FRI_4TO5") return room_states[friday_state_data[HOUR_4TO5][PV_120]];
  if(var == "PV120STAT_FRI_5TO6") return room_states[friday_state_data[HOUR_5TO6][PV_120]];
  
  if(var == "PV147STAT_FRI_8TO9") return room_states[friday_state_data[HOUR_8TO9][PV_147]];
  if(var == "PV147STAT_FRI_9TO10") return room_states[friday_state_data[HOUR_9TO10][PV_147]];
  if(var == "PV147STAT_FRI_10TO11") return room_states[friday_state_data[HOUR_10TO11][PV_147]];
  if(var == "PV147STAT_FRI_11TO12") return room_states[friday_state_data[HOUR_11TO12][PV_147]];
  if(var == "PV147STAT_FRI_12TO1") return room_states[friday_state_data[HOUR_12TO1][PV_147]];
  if(var == "PV147STAT_FRI_1TO2") return room_states[friday_state_data[HOUR_1TO2][PV_147]];
  if(var == "PV147STAT_FRI_2TO3") return room_states[friday_state_data[HOUR_2TO3][PV_147]];
  if(var == "PV147STAT_FRI_3TO4") return room_states[friday_state_data[HOUR_3TO4][PV_147]];
  if(var == "PV147STAT_FRI_4TO5") return room_states[friday_state_data[HOUR_4TO5][PV_147]];
  if(var == "PV147STAT_FRI_5TO6") return room_states[friday_state_data[HOUR_5TO6][PV_147]];
  
  if(var == "PV153STAT_FRI_8TO9") return room_states[friday_state_data[HOUR_8TO9][PV_153]];
  if(var == "PV153STAT_FRI_9TO10") return room_states[friday_state_data[HOUR_9TO10][PV_153]];
  if(var == "PV153STAT_FRI_10TO11") return room_states[friday_state_data[HOUR_10TO11][PV_153]];
  if(var == "PV153STAT_FRI_11TO12") return room_states[friday_state_data[HOUR_11TO12][PV_153]];
  if(var == "PV153STAT_FRI_12TO1") return room_states[friday_state_data[HOUR_12TO1][PV_153]];
  if(var == "PV153STAT_FRI_1TO2") return room_states[friday_state_data[HOUR_1TO2][PV_153]];
  if(var == "PV153STAT_FRI_2TO3") return room_states[friday_state_data[HOUR_2TO3][PV_153]];
  if(var == "PV153STAT_FRI_3TO4") return room_states[friday_state_data[HOUR_3TO4][PV_153]];
  if(var == "PV153STAT_FRI_4TO5") return room_states[friday_state_data[HOUR_4TO5][PV_153]];
  if(var == "PV153STAT_FRI_5TO6") return room_states[friday_state_data[HOUR_5TO6][PV_153]];
  //-------------------------------------------------------------------------------------------
  
  return String();
}

/**********************************************************************  
* Name: StoreData
* 
* Purpose: This function replaces the current and stored data based on
*          the received state, current day, current hour, and room.
*          
* Precondition: The current and stored data will remain unchanged.
*
* Postcondition: The current and stored data will be updated.
*      
************************************************************************/
void StoreData(String state, uint8_t day, uint8_t hour, uint8_t room){
  bool late_or_cancel = false;
  if(state == "open") current_states[room] = ROOM_OPEN; //Store the receieved room state in int form
  else if(state == "booked") current_states[room] = ROOM_BOOKED;
  else if(state == "cancelled"){
    //If the room state is already late or cancelled, don't update previous state
    if(current_states[room] != ROOM_CANCELLED && current_states[room] != ROOM_LATE) late_or_cancel = true;
    current_states[room] = ROOM_CANCELLED;
  }
  else if(state == "late"){
    //If the room state is already late or cancelled, don't update previous state
    if(current_states[room] != ROOM_CANCELLED && current_states[room] != ROOM_LATE) late_or_cancel = true;
    current_states[room] = ROOM_LATE;
  }
  else current_states[room] = ROOM_OPEN;
  switch(day){ //Update the stored state data in current weekday and hour block
    case DAY_MONDAY:
      if(late_or_cancel == true) room_previous_state[late_or_cancelled_count] = monday_state_data[hour][room];
      monday_state_data[hour][room] = current_states[room];
      break;
    case DAY_TUESDAY:
      if(late_or_cancel == true) room_previous_state[late_or_cancelled_count] = tuesday_state_data[hour][room];
      tuesday_state_data[hour][room] = current_states[room];
      break;
    case DAY_WEDNESDAY:
      if(late_or_cancel == true) room_previous_state[late_or_cancelled_count] = wednesday_state_data[hour][room];
      wednesday_state_data[hour][room] = current_states[room];
      break;
    case DAY_THURSDAY:
      if(late_or_cancel == true) room_previous_state[late_or_cancelled_count] = thursday_state_data[hour][room];
      thursday_state_data[hour][room] = current_states[room];
      break;
    case DAY_FRIDAY:
      if(late_or_cancel == true) room_previous_state[late_or_cancelled_count] = friday_state_data[hour][room];
      friday_state_data[hour][room] = current_states[room];
      break;
  }
  if(late_or_cancel == true){
    late_or_cancelled_day[late_or_cancelled_count] = day;
    late_or_cancelled_hour[late_or_cancelled_count] = hour;
    late_or_cancelled_room[late_or_cancelled_count] = room;
    late_or_cancelled_count++; 
  }
}

void StoreControlData(uint8_t state, uint8_t day, uint8_t hour, uint8_t room){
  bool late_or_cancel = false;
  switch(day){ //Update the stored state data in current weekday and hour block
    case DAY_MONDAY:
      if(state == ROOM_OPEN) monday_state_data[hour][room] = ROOM_OPEN;
      else if(state == ROOM_BOOKED) monday_state_data[hour][room] = ROOM_BOOKED;
      else if(state == ROOM_CANCELLED){
        if(monday_state_data[hour][room] != ROOM_CANCELLED && monday_state_data[hour][room] != ROOM_LATE){
          late_or_cancel = true;
          room_previous_state[late_or_cancelled_count] = monday_state_data[hour][room];
        }
        monday_state_data[hour][room] = ROOM_CANCELLED;
      }
      else if(state == ROOM_LATE){
        if(monday_state_data[hour][room] != ROOM_CANCELLED && monday_state_data[hour][room] != ROOM_LATE){
          late_or_cancel = true;
          room_previous_state[late_or_cancelled_count] = monday_state_data[hour][room];
        }
        monday_state_data[hour][room] = ROOM_LATE;
      }
      else monday_state_data[hour][room] = ROOM_OPEN;
      break;
    case DAY_TUESDAY:
      if(state == ROOM_OPEN) tuesday_state_data[hour][room] = ROOM_OPEN;
      else if(state == ROOM_BOOKED) tuesday_state_data[hour][room] = ROOM_BOOKED;
      else if(state == ROOM_CANCELLED){
        if(tuesday_state_data[hour][room] != ROOM_CANCELLED && tuesday_state_data[hour][room] != ROOM_LATE){
          late_or_cancel = true;
          room_previous_state[late_or_cancelled_count] = tuesday_state_data[hour][room];
        }
        tuesday_state_data[hour][room] = ROOM_CANCELLED;
      }
      else if(state == ROOM_LATE){
        if(tuesday_state_data[hour][room] != ROOM_CANCELLED && tuesday_state_data[hour][room] != ROOM_LATE){
          late_or_cancel = true;
          room_previous_state[late_or_cancelled_count] = tuesday_state_data[hour][room];
        }
        tuesday_state_data[hour][room] = ROOM_LATE;
      }
      else tuesday_state_data[hour][room] = ROOM_OPEN;
      break;
    case DAY_WEDNESDAY:
      if(state == ROOM_OPEN) wednesday_state_data[hour][room] = ROOM_OPEN;
      else if(state == ROOM_BOOKED) wednesday_state_data[hour][room] = ROOM_BOOKED;
      else if(state == ROOM_CANCELLED){
        if(wednesday_state_data[hour][room] != ROOM_CANCELLED && wednesday_state_data[hour][room] != ROOM_LATE){
          late_or_cancel = true;
          room_previous_state[late_or_cancelled_count] = wednesday_state_data[hour][room];
        }
        wednesday_state_data[hour][room] = ROOM_CANCELLED;
      }
      else if(state == ROOM_LATE){
        if(wednesday_state_data[hour][room] != ROOM_CANCELLED && wednesday_state_data[hour][room] != ROOM_LATE){
          late_or_cancel = true;
          room_previous_state[late_or_cancelled_count] = wednesday_state_data[hour][room];
        }
        wednesday_state_data[hour][room] = ROOM_LATE;
      }
      else wednesday_state_data[hour][room] = ROOM_OPEN;
      break;
    case DAY_THURSDAY:
      if(state == ROOM_OPEN) thursday_state_data[hour][room] = ROOM_OPEN;
      else if(state == ROOM_BOOKED) thursday_state_data[hour][room] = ROOM_BOOKED;
      else if(state == ROOM_CANCELLED){
        if(thursday_state_data[hour][room] != ROOM_CANCELLED && thursday_state_data[hour][room] != ROOM_LATE){
          late_or_cancel = true;
          room_previous_state[late_or_cancelled_count] = thursday_state_data[hour][room];
        }
        thursday_state_data[hour][room] = ROOM_CANCELLED;
      }
      else if(state == ROOM_LATE){
        if(thursday_state_data[hour][room] != ROOM_CANCELLED && thursday_state_data[hour][room] != ROOM_LATE){
          late_or_cancel = true;
          room_previous_state[late_or_cancelled_count] = thursday_state_data[hour][room];
        }
        thursday_state_data[hour][room] = ROOM_LATE;
      }
      else thursday_state_data[hour][room] = ROOM_OPEN;
      break;
    case DAY_FRIDAY:
      if(state == ROOM_OPEN) friday_state_data[hour][room] = ROOM_OPEN;
      else if(state == ROOM_BOOKED) friday_state_data[hour][room] = ROOM_BOOKED;
      else if(state == ROOM_CANCELLED){
        if(friday_state_data[hour][room] != ROOM_CANCELLED && friday_state_data[hour][room] != ROOM_LATE){
          late_or_cancel = true;
          room_previous_state[late_or_cancelled_count] = friday_state_data[hour][room];
        }
        friday_state_data[hour][room] = ROOM_CANCELLED;
      }
      else if(state == ROOM_LATE){
        if(friday_state_data[hour][room] != ROOM_CANCELLED && friday_state_data[hour][room] != ROOM_LATE){
          late_or_cancel = true;
          room_previous_state[late_or_cancelled_count] = friday_state_data[hour][room];
        }
        friday_state_data[hour][room] = ROOM_LATE;
      }
      else friday_state_data[hour][room] = ROOM_OPEN;
      break;
  }
  if(late_or_cancel == true){
    late_or_cancelled_day[late_or_cancelled_count] = day;
    late_or_cancelled_hour[late_or_cancelled_count] = hour;
    late_or_cancelled_room[late_or_cancelled_count] = room;
    late_or_cancelled_count++; 
  }
}

/**********************************************************************  
* Name: EncryptStringSHA256
* 
* Purpose: This function receives a string and encrypts it using SHA256
*           functions included with the ESP32 libraries.
*          
* Precondition: The string (a username or password) would be stored 
*                 as plain text.
*
* Postcondition: The string is encrypted using the SHA256 algorithm,
*                 converted to ascii characters, then stored in a 64 byte
*                 array. This array is passed as an argument and a pointer 
*                 to the hashed string is returned.
*      
************************************************************************/
char * EncryptStringSHA256(String string, mbedtls_md_context_t context, mbedtls_md_type_t type, char hashed_string[64]){
  byte string_sha_result[32] = {0}; //Space for the raw result of the encryption
  size_t string_length = strlen(string.c_str()); //Store the length of the string to be encrypted.
  char str[3]; //Space for the converted data
  //Prints for debugging
  //Serial.println("##################################### Entering encryption function #####################################");
  //Serial.print("SHA256 received string: ");
  //Serial.println(string);
  //Serial.println("Encrypting...");
  mbedtls_md_init(&context);
  mbedtls_md_setup(&context, mbedtls_md_info_from_type(type), 0);
  mbedtls_md_starts(&context);
  mbedtls_md_update(&context, (const unsigned char *) string.c_str(), string_length);
  mbedtls_md_finish(&context, string_sha_result);
  mbedtls_md_free(&context);
  for(int ii = 0; ii < sizeof(hashed_string); ii++){ //Fill array with null characters
    hashed_string[ii] = '\0';
  }
  //Serial.print("Hashed string: "); //Print the encrypted string for debugging
  for(int ee = 0; ee < sizeof(string_sha_result); ee++){
    sprintf(str, "%02x", (int)string_sha_result[ee]); //Convert values to ascii characters
    strcat(hashed_string, str); //Add converted values to string
    //Serial.print(str); //Print the sections of the encrypted string
  }
  //Serial.println("\n########################################################################################################");
  return (char *)hashed_string;
}

/**********************************************************************  
* Name: ReadClientsIP
* 
* Purpose: This function is passed a clients IP address of type
*           IPAddress and converts it to a C string. The function is
*           also passed an array of space to store the converted IP.
*           A pointer to the converted IP is returned.
*          
* Precondition: The clients IP address is unknown.
*
* Postcondition: The clients IP address is read and a pointer to the
*                 converted IP's C string is returned.                 
*      
************************************************************************/
char * ReadClientsIP(IPAddress received_ip_ip_var, char memory_for_string[MAX_IP_LENGTH + 1]){
  String received_ip_string_var; //Space to build the string
  //Serial.print("ReadClientsIP received client IP: "); //Print for alert for debugging
  received_ip_string_var = String(received_ip_ip_var[0]); //Store first byte of IP
  received_ip_string_var += "."; //Add a period
  received_ip_string_var += String(received_ip_ip_var[1]); //Store second byte of IP
  received_ip_string_var += "."; //Add a period
  received_ip_string_var += String(received_ip_ip_var[2]); //Store third byte of IP
  received_ip_string_var += "."; //Add a period
  received_ip_string_var += String(received_ip_ip_var[3]); //Store last byte of IP
  strcpy(memory_for_string, received_ip_string_var.c_str()); //Convert to C string
  //Serial.println(memory_for_string); //Print converted IP for debugging
  return (char *)memory_for_string;
}

/**********************************************************************  
* Name: UpdateDisplayedData
* 
* Purpose: This function updates the current states being displayed
*           based on changes to status, weekday, or hour block.
*
* Precondition: The website will display old data if a variable is updated.
*
* Postcondition: The website will display updated data.
*      
************************************************************************/
void UpdateDisplayedData(){
  int uu = 0;
  switch(current_displayed_day) //Update the current states being displayed based on changes to weekday or hour block
      {
        case DAY_MONDAY:
          for(uu = 0; uu < 8; uu++){
            current_states[uu] = monday_state_data[current_displayed_hour][uu];
          }
          break;
        case DAY_TUESDAY:
          for(uu = 0; uu < 8; uu++){
            current_states[uu] = tuesday_state_data[current_displayed_hour][uu];
          }
          break;
        case DAY_WEDNESDAY:
          for(uu = 0; uu < 8; uu++){
            current_states[uu] = wednesday_state_data[current_displayed_hour][uu];
          }
          break;
        case DAY_THURSDAY:
          for(uu = 0; uu < 8; uu++){
            current_states[uu] = thursday_state_data[current_displayed_hour][uu];
          }
          break;
        case DAY_FRIDAY:
          for(uu = 0; uu < 8; uu++){
            current_states[uu] = friday_state_data[current_displayed_hour][uu];
          }
          break;
      }  
}

/**********************************************************************  
* Name: HourUpdate
* 
* Purpose: This function checks the current NTP hour and updates
*          accordingly. If the hour changes and a room is late/cancelled
*          before that hour, that room state is reverted.
*
* Precondition: The current hour based on NTP is unknown.
*
* Postcondition: The current hour is stored and room states are reverted
*                if they are in the past.
*      
************************************************************************/
void HourUpdate(){
  timeClient.update();
  uint8_t day = timeClient.getDay();
  timeClient.update();
  if(day == DAY_SATURDAY || day == DAY_SUNDAY) day = DAY_MONDAY;
  switch(timeClient.getHours()) //Store current ntp hour if within range of 8am to 6pm, otherwise default to 8am
  {
    case 8: current_ntp_hour = HOUR_8TO9;
      break;
    case 9: current_ntp_hour = HOUR_9TO10;
      break;
    case 10: current_ntp_hour = HOUR_10TO11;
      break;
    case 11: current_ntp_hour = HOUR_11TO12;
      break;
    case 12: current_ntp_hour = HOUR_12TO1;
      break;
    case 13: current_ntp_hour = HOUR_1TO2;
      break;
    case 14: current_ntp_hour = HOUR_2TO3;
      break;
    case 15: current_ntp_hour = HOUR_3TO4;
      break;
    case 16: current_ntp_hour = HOUR_4TO5;
      break;
    case 17: current_ntp_hour = HOUR_5TO6;
      break;
    default: current_ntp_hour = HOUR_8TO9;
      break;
  }
  
  //This code block restores any late/cancelled rooms if their hour is behind current
  if(current_ntp_hour != previous_ntp_hour){
    Serial.println("Hour change detected."); //Print an alert for debugging
    if(late_or_cancelled_count > 0){
      Serial.println("Some rooms are late or cancelled, checking times ..."); //Print an alert for debugging
      for(int ii = 0; ii < late_or_cancelled_count; ii++){
        if(((late_or_cancelled_hour[ii] < current_ntp_hour) || (late_or_cancelled_hour[ii] == HOUR_5TO6 && current_ntp_hour == HOUR_8TO9)) && day >= late_or_cancelled_day[ii]){
          Serial.println("Reverting a room state.");
          switch(late_or_cancelled_day[ii]){
            case DAY_MONDAY:
              monday_state_data[late_or_cancelled_hour[ii]][late_or_cancelled_room[ii]] = room_previous_state[ii];
              break;
            case DAY_TUESDAY:
              tuesday_state_data[late_or_cancelled_hour[ii]][late_or_cancelled_room[ii]] = room_previous_state[ii];
              break;
            case DAY_WEDNESDAY:
              wednesday_state_data[late_or_cancelled_hour[ii]][late_or_cancelled_room[ii]] = room_previous_state[ii];
              break;
            case DAY_THURSDAY:
              thursday_state_data[late_or_cancelled_hour[ii]][late_or_cancelled_room[ii]] = room_previous_state[ii];
              break;
            case DAY_FRIDAY:
              friday_state_data[late_or_cancelled_hour[ii]][late_or_cancelled_room[ii]] = room_previous_state[ii];
              break;
          }
          if(late_or_cancelled_count == 1){
            late_or_cancelled_count = 0;
            late_or_cancelled_day[0] = 0;
            late_or_cancelled_hour[0] = 0;
            late_or_cancelled_room[0] = 0;
            room_previous_state[0] = 0;
          }
          else{
            for(int jj = ii; jj < late_or_cancelled_count; ++jj) //24
            {
              late_or_cancelled_day[jj] = late_or_cancelled_day[jj + 1];
              late_or_cancelled_hour[jj] = late_or_cancelled_hour[jj + 1];
              late_or_cancelled_room[jj] = late_or_cancelled_room[jj + 1];
              room_previous_state[jj] = room_previous_state[jj + 1];
            }
            ii--;
            late_or_cancelled_count--;
             
          }
        }
      }
    }
    previous_ntp_hour = current_ntp_hour;
  }
}

/**********************************************************************  
* Name: MotionUpdate
* 
* Purpose: This function checks the status of the motion sensors and
*          illuminates status LEDs accordingly.
*
* Precondition: The motion data is unknown.
*
* Postcondition: The motion data is stored and the status LEDs are updated.
*      
************************************************************************/
void MotionUpdate(){
  digitalWrite(cd4021_latch_pin, 1); //Set latch pin to 1 to retrieve recent data into the CD4021
  delayMicroseconds(20); //Delay for timing requirements
  digitalWrite(cd4021_latch_pin, 0); //Set latch pin to 0 to latch data from the CD4021

  //Serial.println("\nReading motion sensor data...");
  data_from_sensors = ShiftIn(cd4021_data_pin, cd4021_clock_pin); //Shift in data from CD4021
  for (int ii = 7; ii >= 0; ii--)
  {
      //Serial.print(bitRead(data_from_sensors, ii)); //Print value of ii bit

      if(bitRead(data_from_sensors, ii) == 1)
      {
          start_time[ii] = millis();
          active[ii] = true;
          bitWrite(send_to_led, ii, 1);
      }

      current = millis();
      
      if(active[ii] == true && (current - start_time[ii]) >= MOTION_DELAY)
      {
          bitWrite(send_to_led, ii, 0);
          active[ii] = false;  
      }
      else if(active[ii] == true && current < start_time[ii])
      {
          if((current + start_time[ii]) - MAX_TIME >= MOTION_DELAY)
          {
              bitWrite(send_to_led, ii, 0);
              active[ii] = false;
          }  
      }
  }
  digitalWrite(tpic6c595_latch_pin, LOW);
  shiftOut(tpic6c595_data_pin, tpic6c595_clock_pin, MSBFIRST, send_to_led);
  digitalWrite(tpic6c595_latch_pin, HIGH);
}

/**********************************************************************  
* Name: ShiftIn
* 
* Purpose: This function ...
*
* Precondition: 
*     
*
* Postcondition: 
*      
************************************************************************/
byte ShiftIn(int data_pin, int clock_pin) {
  uint8_t pin_state = 0;
  byte data_in = 0;

  pinMode(clock_pin, OUTPUT);
  pinMode(data_pin, INPUT);

  for (int ii = 7; ii >= 0; ii--)
  {
    digitalWrite(clock_pin, 0); //Start shift by setting clock low
    delayMicroseconds(1); //Delay for timing
    pin_state = digitalRead(data_pin); //Read state of pin
    if (pin_state) data_in = data_in | (1 << ii); //If the pin is high, store a 1 at bit ii
    digitalWrite(clock_pin, 1); //End shift by setting clock high
  }
  return data_in;
}

/**********************************************************************  
* Name: UpdateRGB
* 
* Purpose: This function ...
*
* Precondition: 
*     
*
* Postcondition: 
*      
************************************************************************/
void UpdateRGB(uint8_t current_weekday, uint8_t current_hour){
  uint8_t rr = 0;
  uint8_t dd = 0;
  uint8_t read_bit = 0;
  uint8_t states[8] = {0};
  uint8_t colors[8] = {0};
  int rgb_data = 0; //Data for 24bit daisy-chained shift registers
  switch(current_weekday){
    case DAY_MONDAY:
      for(rr = 0; rr < 8; rr++){
        states[rr] = monday_state_data[current_hour][rr];
      }
      break;
    case DAY_TUESDAY:
      for(rr = 0; rr < 8; rr++){
        states[rr] = tuesday_state_data[current_hour][rr];
      }
      break;
    case DAY_WEDNESDAY:
      for(rr = 0; rr < 8; rr++){
        states[rr] = wednesday_state_data[current_hour][rr];
      }
      break;
    case DAY_THURSDAY:
      for(rr = 0; rr < 8; rr++){
        states[rr] = thursday_state_data[current_hour][rr];
      }
      break;
    case DAY_FRIDAY:
      for(rr = 0; rr < 8; rr++){
        states[rr] = friday_state_data[current_hour][rr];
      }
      break;
    default:
      for(rr = 0; rr < 8; rr++){
        states[rr] = ROOM_OPEN;
      }
      break;
  }
  
  for(rr = 0; rr < 8; rr++){
    switch(states[rr]){
      case ROOM_OPEN:
        colors[rr] = RGB_GREEN;
        break;
      case ROOM_BOOKED:
        colors[rr] = RGB_RED;
        break;
      case ROOM_CANCELLED:
        colors[rr] = RGB_YELLOW;
        break;
      case ROOM_LATE:
        colors[rr] = RGB_PINK;
        break;
      default:
        colors[rr] = RGB_GREEN;
        break;      
    }
    for(dd = 0; dd < 24; dd++){
      bitWrite(rgb_data, dd, bitRead(colors[rr], 0));
      dd++;
      bitWrite(rgb_data, dd, bitRead(colors[rr], 1));
      dd++;
      bitWrite(rgb_data, dd, bitRead(colors[rr], 2));
    }
  }

  digitalWrite(rgb_tpic6c595_latch_pin, LOW);
  // shift out the bits:
  shiftOut(rgb_tpic6c595_data_pin, rgb_tpic6c595_clock_pin, MSBFIRST, rgb_data >> 16);
  shiftOut(rgb_tpic6c595_data_pin, rgb_tpic6c595_clock_pin, MSBFIRST, rgb_data >> 8);
  shiftOut(rgb_tpic6c595_data_pin, rgb_tpic6c595_clock_pin, MSBFIRST, rgb_data);
  //take the latch pin high so the LEDs will light up:
  digitalWrite(rgb_tpic6c595_latch_pin, HIGH);
}

/**********************************************************************  
* Name: setup
* 
* Purpose: This function ...
*
* Precondition: 
*     
*
* Postcondition: 
*      
************************************************************************/
void setup(){
  //Serial port and LED for debugging purposes
  Serial.begin(115200);
  pinMode(cd4021_data_pin, INPUT);
  pinMode(cd4021_clock_pin, OUTPUT);
  pinMode(cd4021_latch_pin, OUTPUT); 

  pinMode(tpic6c595_data_pin, OUTPUT);
  pinMode(tpic6c595_clock_pin, OUTPUT);
  pinMode(tpic6c595_latch_pin, OUTPUT);

  pinMode(rgb_tpic6c595_data_pin, OUTPUT);
  pinMode(rgb_tpic6c595_clock_pin, OUTPUT);
  pinMode(rgb_tpic6c595_latch_pin, OUTPUT);
  
  //Initialize SPIFFS
  if(!SPIFFS.begin(true)){
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  //Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }
  
  Serial.println(WiFi.localIP()); //Print ESP32 Local IP Address
  
  timeClient.begin(); //Start time client
  timeClient.update(); //Get current time
  Serial.println(timeClient.getFormattedTime());
  current_displayed_day = DAY_MONDAY; //Set initial day to Monday
  current_displayed_hour = HOUR_8TO9; //Set initial hour to 8 to 9
  day_state = days_of_the_week[current_displayed_day]; //Update day state string based on current day
  hour_state = hour_blocks[current_displayed_hour]; //Update hour state string based on current hour
  
  //Route for root / web page
  //Note: This is the initial page served to the user. Can be something besides index.html if required.
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    char received_ip[MAX_IP_LENGTH + 1] = "NONE"; //Space for client's IP address
    ReadClientsIP(request->client()->remoteIP(), received_ip); //Read client's IP address
    //Serial.print("/: The current connected IP is: "); //Prints information about the current connected IP for debugging
    //Serial.println(connected_ip);
    if(user_match == true && pass_match == true && (strcmp(connected_ip, received_ip) == 0)){
      UpdateDisplayedData();
      request->send(SPIFFS, "/index.html", String(), false, processor); //Serve control html to client if they are the current connected IP
    }
    else request->send(SPIFFS, "/monday.html", String(), false, processor); //Serve view html to client
  });
  
  //Route to load style.css file
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/style.css", "text/css"); //Serve css to client
  });

  //This processes the <form action="/login" ...> elements (the text fields on the login page)
  server.on("/login", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String received_username; //Space for received username
    String received_password; //Space for received password
    char * returned_string = NULL; //Pointer to encrypted string
    char received_ip[MAX_IP_LENGTH + 1] = "NONE"; //Space for client's IP address
    ReadClientsIP(request->client()->remoteIP(), received_ip); //Read client's IP address
    //Serial.print("/login: The current connected IP is: "); //Prints information about the current connected IP for debugging
    //Serial.println(connected_ip);
    if(strcmp(connected_ip, "NONE") == 0) //If there is no IP currently connected
    {
      strcpy(connected_ip, received_ip); //Store the received IP as the currently connected IP
      if(request->hasParam(param_login_username)) { //If the request contains the username parameter
        //Serial.println("Username string received!"); //Print an alert for debugging
        returned_string = EncryptStringSHA256(request->getParam(param_login_username)->value(), ctx, md_type, hashed_username); //Encrypt received username
        if(strcmp(returned_string, hashed_acceptable_username) == 0){ //If received username matches acceptable username
          Serial.println("User entered username that matches."); //Print an alert for debugging
          user_match = true; //Update bool for username
        }
        else{ //Received username did not match
          Serial.println("User entered username that does not match."); //Print an alert for debugging
          user_match = false; //Update bool for username
          strcpy(connected_ip, "NONE"); //Set currently connected IP to NONE
          tried_but_failed = true;
        }
      }
      if (request->hasParam(param_login_password)) { //If the request contains the password parameter
        //Serial.println("Password string received!"); //Print an alert for debugging
        returned_string = EncryptStringSHA256(request->getParam(param_login_password)->value(), ctx, md_type, hashed_password); //Encrypt received password
        if(strcmp(returned_string, hashed_acceptable_password) == 0){ //If received password matches acceptable password
          Serial.println("User entered password that matches."); //Print an alert for debugging
          pass_match = true; //Update bool for password
        }
        else{ //Received password did not match
          Serial.println("User entered password that does not match."); //Print an alert for debugging
          pass_match = false; //Update bool for password
          strcpy(connected_ip, "NONE"); //Set currently connected IP to NONE
          tried_but_failed = true;
        }
      }
      //Serial.print("Received username: "); //Print received status for debugging
      //if(received_username)Serial.println(received_username);
      //Serial.print("Received password: ");
      //if(received_password)Serial.println(received_password);
    }
    else request->send(SPIFFS, "/busy.html", String(), false, processor); //Serve busy html to client if they aren't the current connected IP
  });

  //This processes the <form action="/logout" ...> elements (the logout button on the index page)
  server.on("/logout", HTTP_GET, [] (AsyncWebServerRequest *request) {
    char received_ip[MAX_IP_LENGTH + 1] = "NONE"; //Space for client's IP address
    ReadClientsIP(request->client()->remoteIP(), received_ip); //Read client's IP address
    if(user_match == true && pass_match == true && (strcmp(connected_ip, received_ip) == 0)){
      if(request->hasParam(param_logout)) { //If the request contains the logout parameter
        //Serial.println("User requested to logout.");
        user_match = false; //Update username bool
        pass_match = false; //Update password bool
        strcpy(connected_ip, "NONE"); //Set currently connected IP to NONE
       } 
    }
    else request->send(200, "text/html", "Access denied, login first."); //Deny access to client if they aren't connected
  });

  //This processes the <form action="/fakehour" ...> elements (the trigger hour change button on the index page)
  server.on("/fakehour", HTTP_GET, [] (AsyncWebServerRequest *request) {
    char received_ip[MAX_IP_LENGTH + 1] = "NONE"; //Space for client's IP address
    ReadClientsIP(request->client()->remoteIP(), received_ip); //Read client's IP address
    if(user_match == true && pass_match == true && (strcmp(connected_ip, received_ip) == 0)){
      if(request->hasParam("fakehour")) previous_ntp_hour = 16; 
    }
    else request->send(200, "text/html", "Access denied, login first."); //Deny access to client if they aren't connected
  });

  //This processes the <form action="/view-monday" ...> elements (the view only mode buttons)
  server.on("/view-monday", HTTP_GET, [] (AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/monday.html", String(), false, processor); //Serve view html to client
  });

  //This processes the <form action="/view-tuesday" ...> elements (the view only mode buttons)
  server.on("/view-tuesday", HTTP_GET, [] (AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/tuesday.html", String(), false, processor); //Serve view html to client
  });

  //This processes the <form action="/view-wednesday" ...> elements (the view only mode buttons)
  server.on("/view-wednesday", HTTP_GET, [] (AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/wednesday.html", String(), false, processor); //Serve view html to client
  });

  //This processes the <form action="/view-thursday" ...> elements (the view only mode buttons)
  server.on("/view-thursday", HTTP_GET, [] (AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/thursday.html", String(), false, processor); //Serve view html to client
  });

  //This processes the <form action="/view-friday" ...> elements (the view only mode buttons)
  server.on("/view-friday", HTTP_GET, [] (AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/friday.html", String(), false, processor); //Serve view html to client
  });

  //This processes the <form action="/get" ...> elements (the weekday and hour block dropdown lists)
  server.on("/get", HTTP_GET, [] (AsyncWebServerRequest *request) {
    char received_ip[MAX_IP_LENGTH + 1] = "NONE"; //Space for client's IP address
    ReadClientsIP(request->client()->remoteIP(), received_ip); //Read client's IP address
    if(user_match == true && pass_match == true && (strcmp(connected_ip, received_ip) == 0)){
      uint8_t ii = 0; //Used in for loops
      String input_message; //Message from GET request
      char received_weekday[10]; //Stored weekday from processed GET request
      if (request->hasParam(param_weekday)) { //If the request contains the weekday parameter
        //Serial.println("Weekday change detected!"); //Print an alert for debugging
        input_message = request->getParam(param_weekday)->value(); //Store the String value of the parameter
        strcpy(received_weekday, input_message.c_str()); //Copy the String to a c string
        received_weekday[0] = toupper(received_weekday[0]); //Capitalize the first letter so it matches the constants
        day_state = received_weekday; //Store the received day state in string form
        if(day_state == "Monday") current_displayed_day = DAY_MONDAY; //Store the received day state in  int form
        else if(day_state == "Tuesday") current_displayed_day = DAY_TUESDAY;
        else if(day_state == "Wednesday")current_displayed_day = DAY_WEDNESDAY;
        else if(day_state == "Thursday")current_displayed_day = DAY_THURSDAY;
        else if(day_state == "Friday")current_displayed_day = DAY_FRIDAY;
        else current_displayed_day = DAY_MONDAY;
        
        //Serial.print("Received weekday: "); //Print received weekday for debugging
        //Serial.println(received_weekday);
      }
      else if (request->hasParam(param_hour_block)) { //If the request contains the hour block parameter
        //Serial.println("Hour change detected!"); //Print an alert for debugging
        input_message = request->getParam(param_hour_block)->value(); //Store the String value of the parameter
        if(strcmp(input_message.c_str(), "eight-to-nine") == 0){ //Store the received hour block state in string and int form
          hour_state = hour_blocks[HOUR_8TO9];
          current_displayed_hour = HOUR_8TO9;
        }
        else if(strcmp(input_message.c_str(), "nine-to-ten") == 0){
          hour_state = hour_blocks[HOUR_9TO10];
          current_displayed_hour = HOUR_9TO10;
        }
        else if(strcmp(input_message.c_str(), "ten-to-eleven") == 0){
          hour_state = hour_blocks[HOUR_10TO11];
          current_displayed_hour = HOUR_10TO11;
        }
        else if(strcmp(input_message.c_str(), "eleven-to-noon") == 0){
          hour_state = hour_blocks[HOUR_11TO12];
          current_displayed_hour = HOUR_11TO12;
        }
        else if(strcmp(input_message.c_str(), "noon-to-one") == 0){
          hour_state = hour_blocks[HOUR_12TO1];
          current_displayed_hour = HOUR_12TO1;
        }
        else if(strcmp(input_message.c_str(), "one-to-two") == 0){
          hour_state = hour_blocks[HOUR_1TO2];
          current_displayed_hour = HOUR_1TO2;
        }
        else if(strcmp(input_message.c_str(), "two-to-three") == 0){
          hour_state = hour_blocks[HOUR_2TO3];
          current_displayed_hour = HOUR_2TO3;
        }
        else if(strcmp(input_message.c_str(), "three-to-four") == 0){
          hour_state = hour_blocks[HOUR_3TO4];
          current_displayed_hour = HOUR_3TO4;
        }
        else if(strcmp(input_message.c_str(), "four-to-five") == 0){
          hour_state = hour_blocks[HOUR_4TO5];
          current_displayed_hour = HOUR_4TO5;
        }
        else if(strcmp(input_message.c_str(), "five-to-six") == 0){
          hour_state = hour_blocks[HOUR_5TO6];
          current_displayed_hour = HOUR_5TO6;
        }
        else {
          hour_state = hour_blocks[HOUR_8TO9];
          current_displayed_hour = HOUR_8TO9;
        }
        //Serial.print("Received hour block: "); //Print received hour block for debugging
        //Serial.println(hour_state);
      }
      UpdateDisplayedData();
    }
    else request->send(200, "text/html", "Access denied, login first."); //Deny access to client if they aren't connected
  });
  //This processes the <form action="/set" ...> elements (the change status dropdown lists)
  server.on("/set", HTTP_GET, [] (AsyncWebServerRequest *request) {
    char received_ip[MAX_IP_LENGTH + 1] = "NONE"; //Space for client's IP address
    ReadClientsIP(request->client()->remoteIP(), received_ip); //Read client's IP address
    if(user_match == true && pass_match == true && (strcmp(connected_ip, received_ip) == 0)){
      String input_message; //Message from GET request
      if(request->hasParam(param_pv104_status)) { //If the request contains the PV104 parameter
        //Serial.println("PV104 status changed!"); //Print an alert for debugging
        input_message = request->getParam(param_pv104_status)->value(); //Store the String value of the parameter
        StoreData(input_message, current_displayed_day, current_displayed_hour, PV_104);
      }
      else if (request->hasParam(param_pv107_status)) { //If the request contains the PV107 parameter
        //Serial.println("PV107 status changed!"); //Print an alert for debugging
        input_message = request->getParam(param_pv107_status)->value(); //Store the String value of the parameter
        StoreData(input_message, current_displayed_day, current_displayed_hour, PV_107);
      }
      else if (request->hasParam(param_pv108_status)) { //If the request contains the PV108 parameter
        //Serial.println("PV108 status changed!"); //Print an alert for debugging
        input_message = request->getParam(param_pv108_status)->value(); //Store the String value of the parameter
        StoreData(input_message, current_displayed_day, current_displayed_hour, PV_108);
      }
      else if (request->hasParam(param_pv110_status)) { //If the request contains the PV110 parameter
        //Serial.println("PV110 status changed!"); //Print an alert for debugging
        input_message = request->getParam(param_pv110_status)->value(); //Store the String value of the parameter
        StoreData(input_message, current_displayed_day, current_displayed_hour, PV_110);
      }
      else if (request->hasParam(param_pv119_status)) { //If the request contains the PV119 parameter
        //Serial.println("PV119 status changed!"); //Print an alert for debugging
        input_message = request->getParam(param_pv119_status)->value(); //Store the String value of the parameter
        StoreData(input_message, current_displayed_day, current_displayed_hour, PV_119);
      }
      else if (request->hasParam(param_pv120_status)) { //If the request contains the PV120 parameter
        //Serial.println("PV120 status changed!"); //Print an alert for debugging
        input_message = request->getParam(param_pv120_status)->value(); //Store the String value of the parameter
        StoreData(input_message, current_displayed_day, current_displayed_hour, PV_120);
      }
      else if (request->hasParam(param_pv147_status)) { //If the request contains the PV147 parameter
        //Serial.println("PV147 status changed!"); //Print an alert for debugging
        input_message = request->getParam(param_pv147_status)->value(); //Store the String value of the parameter
        StoreData(input_message, current_displayed_day, current_displayed_hour, PV_147);
      }
      else if (request->hasParam(param_pv153_status)) { //If the request contains the PV153 parameter
        //Serial.println("PV153 status changed!"); //Print an alert for debugging
        input_message = request->getParam(param_pv153_status)->value(); //Store the String value of the parameter
        StoreData(input_message, current_displayed_day, current_displayed_hour, PV_153);
      }
      //Serial.print("Received status: "); //Print received status for debugging
      //Serial.println(input_message); 
    }
    else request->send(200, "text/html", "Access denied, login first."); //Deny access to client if they aren't connected
  });
  
  //This processes the <form action="/control-directory" ...> elements (the board control login buttons)
  server.on("/control-directory", HTTP_GET, [] (AsyncWebServerRequest *request) {
    char received_ip[MAX_IP_LENGTH + 1] = "NONE"; //Space for client's IP address
    ReadClientsIP(request->client()->remoteIP(), received_ip); //Read client's IP address
    if(tried_but_failed == true){
      request->send(SPIFFS, "/login-attempted.html", String(), false, processor); //Serve login attempted html to client if they get user or pass wrong
      tried_but_failed = false;
    }
    else if(user_match == true && pass_match == true && (strcmp(connected_ip, received_ip) == 0)){
      UpdateDisplayedData();
      request->send(SPIFFS, "/index.html", String(), false, processor); //Serve control html to client if they are the current connected IP
    }
    else if(strcmp(connected_ip, "NONE") != 0) request->send(SPIFFS, "/busy.html", String(), false, processor); //Serve busy html to client
    else request->send(SPIFFS, "/login.html", String(), false, processor); //Serve login html to client
  });
  
  //This processes the <form action="/monday-control" ...> elements (Monday control buttons)
  server.on("/monday-control", HTTP_GET, [] (AsyncWebServerRequest *request) {
    char received_ip[MAX_IP_LENGTH + 1] = "NONE"; //Space for client's IP address
    ReadClientsIP(request->client()->remoteIP(), received_ip); //Read client's IP address
    if(user_match == true && pass_match == true && (strcmp(connected_ip, received_ip) == 0)){
      request->send(SPIFFS, "/monday-control.html", String(), false, processor); //Serve control html to client 
    }
    else request->send(200, "text/html", "Access denied, login first."); //Deny access to client if they aren't connected
  });


  //This processes the <form action="/update-monday" ...> elements (from the radio buttons)
  server.on("/update-monday", HTTP_GET, [] (AsyncWebServerRequest *request) {
    int hour = 0;
    int lab = 0;
    int index = 0;
    char received_ip[MAX_IP_LENGTH + 1] = "NONE"; //Space for client's IP address
    ReadClientsIP(request->client()->remoteIP(), received_ip); //Read client's IP address
    if(user_match == true && pass_match == true && (strcmp(connected_ip, received_ip) == 0)){
      String input_message; //Message from GET request
      index = 0;
      for(lab = 0; lab < 8; lab++){
        for(hour = 0; hour < 10; hour++){
          if(request->hasParam(params_control_monday[index])){
            input_message = request->getParam(params_control_monday[index])->value(); //Store the String value of the parameter
            if(input_message == "Open") StoreControlData(ROOM_OPEN, DAY_MONDAY, hour, lab);
            else if(input_message == "Booked") StoreControlData(ROOM_BOOKED, DAY_MONDAY, hour, lab);
            else if(input_message == "Cancelled") StoreControlData(ROOM_CANCELLED, DAY_MONDAY, hour, lab);
            else if(input_message == "Late") StoreControlData(ROOM_LATE, DAY_MONDAY, hour, lab);
            else StoreControlData(ROOM_OPEN, DAY_MONDAY, hour, lab);
          }
          index++;
        }
      }
      UpdateDisplayedData();
      request->send(SPIFFS, "/index.html", String(), false, processor);
    }
    else request->send(200, "text/html", "Access denied, login first."); //Deny access to client if they aren't connected
  });

  //This processes the <form action="/tuesday-control" ...> elements (Tuesday control buttons)
  server.on("/tuesday-control", HTTP_GET, [] (AsyncWebServerRequest *request) {
    char received_ip[MAX_IP_LENGTH + 1] = "NONE"; //Space for client's IP address
    ReadClientsIP(request->client()->remoteIP(), received_ip); //Read client's IP address
    if(user_match == true && pass_match == true && (strcmp(connected_ip, received_ip) == 0)){
      request->send(SPIFFS, "/tuesday-control.html", String(), false, processor); //Serve control html to client 
    }
    else request->send(200, "text/html", "Access denied, login first."); //Deny access to client if they aren't connected
  });


  //This processes the <form action="/update-tuesday" ...> elements (from the radio buttons)
  server.on("/update-tuesday", HTTP_GET, [] (AsyncWebServerRequest *request) {
    int hour = 0;
    int lab = 0;
    int index = 0;
    char received_ip[MAX_IP_LENGTH + 1] = "NONE"; //Space for client's IP address
    ReadClientsIP(request->client()->remoteIP(), received_ip); //Read client's IP address
    if(user_match == true && pass_match == true && (strcmp(connected_ip, received_ip) == 0)){
      String input_message; //Message from GET request
      index = 0;
      for(lab = 0; lab < 8; lab++){
        for(hour = 0; hour < 10; hour++){
          if(request->hasParam(params_control_tuesday[index])){
            input_message = request->getParam(params_control_tuesday[index])->value(); //Store the String value of the parameter
            if(input_message == "Open") StoreControlData(ROOM_OPEN, DAY_TUESDAY, hour, lab);
            else if(input_message == "Booked") StoreControlData(ROOM_BOOKED, DAY_TUESDAY, hour, lab);
            else if(input_message == "Cancelled") StoreControlData(ROOM_CANCELLED, DAY_TUESDAY, hour, lab);
            else if(input_message == "Late") StoreControlData(ROOM_LATE, DAY_TUESDAY, hour, lab);
            else StoreControlData(ROOM_OPEN, DAY_TUESDAY, hour, lab);
          }
          index++;
        }
      }
      UpdateDisplayedData();
      request->send(SPIFFS, "/index.html", String(), false, processor);
    }
    else request->send(200, "text/html", "Access denied, login first."); //Deny access to client if they aren't connected
  });


  //This processes the <form action="/wednesday-control" ...> elements (Wednesday control buttons)
  server.on("/wednesday-control", HTTP_GET, [] (AsyncWebServerRequest *request) {
    char received_ip[MAX_IP_LENGTH + 1] = "NONE"; //Space for client's IP address
    ReadClientsIP(request->client()->remoteIP(), received_ip); //Read client's IP address
    if(user_match == true && pass_match == true && (strcmp(connected_ip, received_ip) == 0)){
      request->send(SPIFFS, "/wednesday-control.html", String(), false, processor); //Serve control html to client 
    }
    else request->send(200, "text/html", "Access denied, login first."); //Deny access to client if they aren't connected
  });


  //This processes the <form action="/update-wednesday" ...> elements (from the radio buttons)
  server.on("/update-wednesday", HTTP_GET, [] (AsyncWebServerRequest *request) {
    int hour = 0;
    int lab = 0;
    int index = 0;
    char received_ip[MAX_IP_LENGTH + 1] = "NONE"; //Space for client's IP address
    ReadClientsIP(request->client()->remoteIP(), received_ip); //Read client's IP address
    if(user_match == true && pass_match == true && (strcmp(connected_ip, received_ip) == 0)){
      String input_message; //Message from GET request
      index = 0;
      for(lab = 0; lab < 8; lab++){
        for(hour = 0; hour < 10; hour++){
          if(request->hasParam(params_control_wednesday[index])){
            input_message = request->getParam(params_control_wednesday[index])->value(); //Store the String value of the parameter
            if(input_message == "Open") StoreControlData(ROOM_OPEN, DAY_WEDNESDAY, hour, lab);
            else if(input_message == "Booked") StoreControlData(ROOM_BOOKED, DAY_WEDNESDAY, hour, lab);
            else if(input_message == "Cancelled") StoreControlData(ROOM_CANCELLED, DAY_WEDNESDAY, hour, lab);
            else if(input_message == "Late") StoreControlData(ROOM_LATE, DAY_WEDNESDAY, hour, lab);
            else StoreControlData(ROOM_OPEN, DAY_WEDNESDAY, hour, lab);
          }
          index++;
        }
      }
      UpdateDisplayedData();
      request->send(SPIFFS, "/index.html", String(), false, processor);
    }
    else request->send(200, "text/html", "Access denied, login first."); //Deny access to client if they aren't connected
  });


  //This processes the <form action="/thursday-control" ...> elements (Thursday control buttons)
  server.on("/thursday-control", HTTP_GET, [] (AsyncWebServerRequest *request) {
    char received_ip[MAX_IP_LENGTH + 1] = "NONE"; //Space for client's IP address
    ReadClientsIP(request->client()->remoteIP(), received_ip); //Read client's IP address
    if(user_match == true && pass_match == true && (strcmp(connected_ip, received_ip) == 0)){
      request->send(SPIFFS, "/thursday-control.html", String(), false, processor); //Serve control html to client 
    }
    else request->send(200, "text/html", "Access denied, login first."); //Deny access to client if they aren't connected
  });


  //This processes the <form action="/update-thursday" ...> elements (from the radio buttons)
  server.on("/update-thursday", HTTP_GET, [] (AsyncWebServerRequest *request) {
    int hour = 0;
    int lab = 0;
    int index = 0;
    char received_ip[MAX_IP_LENGTH + 1] = "NONE"; //Space for client's IP address
    ReadClientsIP(request->client()->remoteIP(), received_ip); //Read client's IP address
    if(user_match == true && pass_match == true && (strcmp(connected_ip, received_ip) == 0)){
      String input_message; //Message from GET request
      index = 0;
      for(lab = 0; lab < 8; lab++){
        for(hour = 0; hour < 10; hour++){
          if(request->hasParam(params_control_thursday[index])){
            input_message = request->getParam(params_control_thursday[index])->value(); //Store the String value of the parameter
            if(input_message == "Open") StoreControlData(ROOM_OPEN, DAY_THURSDAY, hour, lab);
            else if(input_message == "Booked") StoreControlData(ROOM_BOOKED, DAY_THURSDAY, hour, lab);
            else if(input_message == "Cancelled") StoreControlData(ROOM_CANCELLED, DAY_THURSDAY, hour, lab);
            else if(input_message == "Late") StoreControlData(ROOM_LATE, DAY_THURSDAY, hour, lab);
            else StoreControlData(ROOM_OPEN, DAY_THURSDAY, hour, lab);
          }
          index++;
        }
      }
      UpdateDisplayedData();
      request->send(SPIFFS, "/index.html", String(), false, processor);
    }
    else request->send(200, "text/html", "Access denied, login first."); //Deny access to client if they aren't connected
  });


  //This processes the <form action="/friday-control" ...> elements (Friday control buttons)
  server.on("/friday-control", HTTP_GET, [] (AsyncWebServerRequest *request) {
    char received_ip[MAX_IP_LENGTH + 1] = "NONE"; //Space for client's IP address
    ReadClientsIP(request->client()->remoteIP(), received_ip); //Read client's IP address
    if(user_match == true && pass_match == true && (strcmp(connected_ip, received_ip) == 0)){
      request->send(SPIFFS, "/friday-control.html", String(), false, processor); //Serve control html to client 
    }
    else request->send(200, "text/html", "Access denied, login first."); //Deny access to client if they aren't connected
  });


  //This processes the <form action="/update-friday" ...> elements (from the radio buttons)
  server.on("/update-friday", HTTP_GET, [] (AsyncWebServerRequest *request) {
    int hour = 0;
    int lab = 0;
    int index = 0;
    char received_ip[MAX_IP_LENGTH + 1] = "NONE"; //Space for client's IP address
    ReadClientsIP(request->client()->remoteIP(), received_ip); //Read client's IP address
    if(user_match == true && pass_match == true && (strcmp(connected_ip, received_ip) == 0)){
      String input_message; //Message from GET request
      index = 0;
      for(lab = 0; lab < 8; lab++){
        for(hour = 0; hour < 10; hour++){
          if(request->hasParam(params_control_friday[index])){
            input_message = request->getParam(params_control_friday[index])->value(); //Store the String value of the parameter
            if(input_message == "Open") StoreControlData(ROOM_OPEN, DAY_FRIDAY, hour, lab);
            else if(input_message == "Booked") StoreControlData(ROOM_BOOKED, DAY_FRIDAY, hour, lab);
            else if(input_message == "Cancelled") StoreControlData(ROOM_CANCELLED, DAY_FRIDAY, hour, lab);
            else if(input_message == "Late") StoreControlData(ROOM_LATE, DAY_FRIDAY, hour, lab);
            else StoreControlData(ROOM_OPEN, DAY_FRIDAY, hour, lab);
          }
          index++;
        }
      }
      UpdateDisplayedData();
      request->send(SPIFFS, "/index.html", String(), false, processor);
    }
    else request->send(200, "text/html", "Access denied, login first."); //Deny access to client if they aren't connected
  });
  
  server.begin(); //Start server
}
/**********************************************************************  
* Name: loop
* 
* Purpose: This function ...
*
* Precondition: 
*     
*
* Postcondition: 
*      
************************************************************************/
void loop(){
  HourUpdate();
  MotionUpdate();
  //UpdateRGB(timeClient.getDay(), current_ntp_hour);
  delay(10);
}
