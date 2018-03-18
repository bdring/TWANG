#include "HardwareSerial.h"
#include <avr/wdt.h>
#include <EEPROM.h>

/// Defaults

// change this whenever the saved settings are not compatible with a change
// it force a load from defaults.
#define SETTINGS_VERSION 1  

// LED
#define BRIGHTNESS           150

// JOYSTICK
#define JOYSTICK_ORIENTATION 1     // 0, 1 or 2 to set the angle of the joystick
#define JOYSTICK_DIRECTION   1     // 0/1 to flip joystick direction
#define ATTACK_THRESHOLD     30000 // The threshold that triggers an attack
#define JOYSTICK_DEADZONE    7     // Angle to ignore

#define LIVES_PER_LEVEL		3

#define MAX_VOLUME           10

enum stripTypes{
	strip_APA102 = 0,
	strip_WS2812 = 1
};

enum ErrorNums{
	ERR_SETTING_NUM,
	ERR_SETTING_RANGE
};

typedef struct {
	uint8_t settings_version; // stores the settings format version 	
	
	//due to the fastLED classes there is not much we can change dynamically
	uint8_t led_brightness; 	
	
	uint8_t joystick_deadzone;
	uint16_t attack_threshold;
	
	uint8_t audio_volume;
	
	uint8_t lives_per_level;	
	
	// saved statistics
	uint16_t games_played;
	uint32_t total_points;
	uint16_t high_score;
	uint16_t boss_kills;
	
}settings_t;

settings_t user_settings;

#define READ_BUFFER_LEN 10
#define CARRIAGE_RETURN 13
char readBuffer[10];
uint8_t readIndex = 0;

void reset_cpu();
void processSerial(char inChar);
void show_settings_menu();
void reset_settings();
void change_setting(char *line);
void settings_eeprom_write();
void settings_eeprom_write();
void printError(int reason);
void show_game_stats();


void reset_cpu()
{
  wdt_enable(WDTO_15MS);
  while(1)
  {
    // wait for it...boom
  }
}

void processSerial(char inChar)
{
	readBuffer[readIndex] = inChar;
		if (readBuffer[readIndex] == '?') {
			readIndex = 0;
			Serial.print("TWANG VERSION: "); Serial.println(VERSION);
			show_settings_menu();
		}
		else if (readBuffer[readIndex] == CARRIAGE_RETURN) {
			if (readIndex < 3) {
				// not enough characters
				readIndex = 0;
			}
			else {				
				readBuffer[readIndex] = 0; // mark it as the end of the string
				change_setting(readBuffer);	
				readIndex = 0;
			}
		}
		else if (readIndex >= READ_BUFFER_LEN) {
			readIndex = 0; // too many characters. Reset and try again
		}
		else
			readIndex++;
}

void show_settings_menu() {
	Serial.println("\r\n====== TWANG Settings Menu ========");
	Serial.println("=    Current values are shown     =");
	Serial.println("=   Send new values like 13=66    =");
	Serial.println("=     with a carriage return      =");
	Serial.println("===================================");
	
	Serial.print("\r\n10=");	
	Serial.print(user_settings.led_brightness);
	Serial.println(" (LED Brightness 5-255)");
	
	Serial.print("20=");
	Serial.print(user_settings.joystick_deadzone);
	Serial.println(" (Joystick Deadzone 3-12)");
	
	Serial.print("21=");
	Serial.print(user_settings.attack_threshold);
	Serial.println(" (Attack Sensitivity 20000-35000)");
	
	Serial.print("30=");
	Serial.print(user_settings.audio_volume);
	Serial.println(" (Audio Volume 0-10)");
	
	Serial.print("40=");
	Serial.print(user_settings.lives_per_level);
	Serial.println(" (Lives per Level (3-9))");		
	
	Serial.println("\r\n(Send 90=1 to reset everything to defaults)");	
	
	show_game_stats();
}

void show_game_stats()
{
	Serial.println("\r\n ===== Play statistics ======");
	Serial.print("Games played: ");Serial.println(user_settings.games_played);
	if (user_settings.games_played > 0) {
		Serial.print("Average Score: ");Serial.println(user_settings.total_points / user_settings.games_played);
	}
	Serial.print("High Score: ");Serial.println(user_settings.high_score);
	Serial.print("Boss kills: ");Serial.println(user_settings.boss_kills);
}

void reset_settings() {
	user_settings.settings_version = SETTINGS_VERSION;
	
	
	user_settings.led_brightness = BRIGHTNESS;	
	
	user_settings.joystick_deadzone = JOYSTICK_DEADZONE;
	user_settings.attack_threshold = ATTACK_THRESHOLD;
	
	user_settings.audio_volume = MAX_VOLUME;
	
	user_settings.lives_per_level = LIVES_PER_LEVEL;
	
	user_settings.games_played = 0;
	user_settings.total_points = 0;
	user_settings.high_score = 0;	
	user_settings.boss_kills = 0;
	
	settings_eeprom_write();
	
}

void change_setting(char *line) {
  // line formate should be ss=nn
  // ss is always a 2 character integerr
  // nn starts at index 3 and can be up to a 5 character unsigned integer
  
  //12=12345
  //01234567
  
  char setting_val[6];
  uint16_t newValue;
  
  if (readBuffer[2] != '='){  // check if the equals sign is there
	Serial.print("Missing '=' in command");
	readIndex = 0;
	return;
  }
  
  // move the value characters into a char array while verifying they are digits
  for(int i=0; i<5; i++) {
	if (i+3 < readIndex) {
		if (isDigit(readBuffer[i+3]))
			setting_val[i] = readBuffer[i+3];
		else {
			Serial.println("Invalid setting value");
			return;
		}
			
	}
	else
		setting_val[i] = 0;
  }
  
  newValue = atoi(setting_val); // convert hte val section to an integer
  
  if (readBuffer[0] == '1'){ // led setting  =================================================
	if (readBuffer[1] == '0') { // strip type
		if(newValue >=5 && newValue <=255)
			user_settings.led_brightness = newValue;
		else {			
			printError(ERR_SETTING_RANGE);
			return;
		}
	}	
	else {
		printError(ERR_SETTING_NUM);
		return;
	}	
  }
  else if (readBuffer[0] == '2'){ // joystick setting =============================================
	if (readBuffer[1] == '0') { 
		if(newValue >=3 && newValue <=12)
			user_settings.joystick_deadzone = newValue;
		else {
			printError(ERR_SETTING_RANGE);
			return;
		}
	}
	else if (readBuffer[1] == '1') { // direction
		if(newValue >=20000 && newValue <=35000)
			user_settings.attack_threshold = newValue;
		else {
			printError(ERR_SETTING_RANGE);
			return;
		}	
	}
	else {
		printError(ERR_SETTING_NUM);
		return;
	}	
  }
  else if (readBuffer[0] == '3'){ // audio setting ==========================================
	if (readBuffer[1] == '0') { // axis
		if (newValue < 11)
			user_settings.audio_volume = newValue;
		else {
			printError(ERR_SETTING_RANGE);
			return;
		}
	}
	else {
		printError(ERR_SETTING_NUM);
		return;
	}
  }
  else if (readBuffer[0] == '4'){ // game setting ===========================================
	if (readBuffer[1] == '0') { // axis
		if (newValue >= 3 && newValue <= 9)
			user_settings.lives_per_level = newValue;
		else {
			printError(ERR_SETTING_RANGE);
			return;
		}
	}
	else {
		printError(ERR_SETTING_NUM);
		return;
	}
  }
  else if (readBuffer[0] == '9'){ // ===== reset
	if (readBuffer[1] == '0') { // reset
		reset_settings();
	}
	else if (readBuffer[1] == '1') { // reboot
		reset_cpu();
	}
	else {
		printError(ERR_SETTING_NUM);
		return;
	}
  }
  else {
	  printError(ERR_SETTING_NUM);
	  return;
  }
	  
  
  settings_eeprom_write();    
  reset_cpu();
  
}

void settings_eeprom_read()
{
	uint8_t ver = EEPROM.read(0);
	uint8_t temp[sizeof(user_settings)];

	if (ver != SETTINGS_VERSION) {
		Serial.println("Error: Reading EEPROM settings failed");
		Serial.println("Loading defaults");
		reset_settings();		
		return;
	}		
	else {
		Serial.print("Correct settings version: "); Serial.println(ver);
	}
		
	
	for (int i=0; i<sizeof(user_settings); i++)
	{
		temp[i] = EEPROM.read(i);
	}
	
	memcpy((char*)&user_settings, temp, sizeof(user_settings));
	
}

void settings_eeprom_write() {
	uint8_t temp[sizeof(user_settings)];	
	memcpy(temp, (char*)&user_settings, sizeof(user_settings));
	
	for (int i=0; i<sizeof(user_settings); i++)
	{
		EEPROM.write(i, temp[i]);
	}	
}

void printError(int reason) {
	switch(reason) {
		case ERR_SETTING_NUM:
			Serial.print("Error: Invalid setting number");
		break;
		case ERR_SETTING_RANGE:
			Serial.print("Error: Setting out of range");
		break;
		default:
			Serial.print("Error:");Serial.println(reason);
		break;
	}
}
	
