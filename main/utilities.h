#ifndef UTILITIES_H_
#define UTILITIES_H_

#include "Arduino.h"
#include "Adafruit_Fingerprint.h"
#include "TFT_eSPI.h"
#include "SPI.h"
#include "ArduinoJson.h"
#include <time.h>
#include "FS.h"
#include "LittleFS.h"

extern "C"{
  #include "stdio.h"
  #include "stdlib.h"
  #include "string.h"
  #include "sys/stat.h"
  #include "unistd.h"
  #include "freertos/FreeRTOS.h"
  #include "freertos/task.h"
  #include "freertos/event_groups.h"
  #include "esp_log.h"
  #include "esp_err.h"
  #include "esp_system.h"
  #include "esp_wifi.h"
  #include "esp_event.h"
  #include "nvs_flash.h"
  #include "golioth/client.h"
  #include "golioth/stream.h"
  #include "lwip/err.h"
  #include "lwip/sys.h"
}

#define SCREEN_W 320
#define SCREEN_H 480

#define MAINMENU              0
#define WIFI_MENU             1
#define REGISTER_ATTENDANCE   2
#define ADD_NEW_STUDENT       3
#define DELETE_FILE           4
#define SYNC_WITH_SERVER      5
#define ATTENDANCE_MODE       6
#define ADD_FILE              7

#define NONE                  255
#define WIFI_SSID_BOX         254
#define WIFI_PASSWORD_BOX     253
#define SURNAME_FIELD         252
#define FIRST_NAME_FIELD      251
#define STD_NO_FIELD          250
#define NEW_FILE_FIELD        249



//Main menu key sizes
#define MAINKEY_H 40
#define MAINKEY_W 240
// Keypad start position, key sizes and spacing
#define KEY_X 16 // Centre of key
#define KEY_Y 330
#define KEY_W 28 // Width and height
#define KEY_H 30
#define KEY_SPACING_X 4 // X and Y gap
#define KEY_SPACING_Y 10
#define KEY_TEXTSIZE 1   // Font size multiplier

#define MAINMENU_FONT &FreeSerifBold12pt7b

// Word length, buffer for storing int and character index
#define CHAR_LEN 15

#define NUM_OF_STUDENTS 15 //number of students that can be stored in memory at once

extern "C" void wifi_init_sta(void);
extern "C" int setWifiCredentials();

void drawKeyboard();
void drawMainmenu();
void drawWifiMenu();
void drawRegisterAttendanceMenu();
void drawAddNewStudentMenu();
void drawDeleteFile();
void drawSyncWithServer();
void drawAttendanceModeMenu();
void drawAddFileMenu();
void RegAttMenuMsg(u8_t activefile);
void drawHalf();

u8_t enroll_storeTemplateToBuf();
void writeTemplateDataToSensor(u8_t id, u8_t *fingerprint_template);
uint8_t getFingerprintID();

u8_t listDir(fs::FS &fs, const char *dirname, uint8_t levels, char Buffer[][16]);
void writeFile(fs::FS &fs, const char *path, const char *message);
void deleteFile(fs::FS &fs, const char *path);

#endif // UTILITIES_H_


#if CONFIG_ESP_WPA3_SAE_PWE_HUNT_AND_PECK
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_HUNT_AND_PECK
#define EXAMPLE_H2E_IDENTIFIER ""
#elif CONFIG_ESP_WPA3_SAE_PWE_HASH_TO_ELEMENT
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_HASH_TO_ELEMENT
#define EXAMPLE_H2E_IDENTIFIER CONFIG_ESP_WIFI_PW_ID
#elif CONFIG_ESP_WPA3_SAE_PWE_BOTH
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_BOTH
#define EXAMPLE_H2E_IDENTIFIER CONFIG_ESP_WIFI_PW_ID
#endif
#if CONFIG_ESP_WIFI_AUTH_OPEN
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_OPEN
#elif CONFIG_ESP_WIFI_AUTH_WEP
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WEP
#elif CONFIG_ESP_WIFI_AUTH_WPA_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WAPI_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WAPI_PSK
#endif


