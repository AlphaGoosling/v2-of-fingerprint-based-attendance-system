#if !defined(UTILITIES_H_)
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
  #include "lwip/err.h"
  #include "lwip/sys.h"
}

#define SCREEN_W 320
#define SCREEN_H 480

#define MAINMENU              0
#define WIFI_MENU             1
#define REGISTER_ATTENDANCE   2
#define ADD_NEW_STUDENT       3
#define DELETE_STUDENT_ENTRY  4
#define SYNC_WITH_SERVER      5

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

// Using two fonts since numbers are nice when bold
#define LABEL1_FONT   &FreeSansOblique12pt7b // Key label font 1
#define MAINMENU_FONT &FreeSerifBold12pt7b
#define FSS9 &FreeSerifBold12pt7b

// Numeric display box size and location
#define DISP_X 1
#define DISP_Y 210
#define DISP_W 318
#define DISP_H 50
#define DISP_TSIZE 3
#define DISP_TCOLOR TFT_CYAN

// Word length, buffer for storing int and character index
#define CHAR_LEN 15

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

extern "C" void wifi_init_sta(void);

void drawKeyboard();
void drawMainmenu();
void drawWifiMenu();
void drawRegisterAttendanceMenu();
void drawAddNewStudentMenu();
void drawDeleteStudentEntry();
void drawSyncWithServer();
u8_t listDir(fs::FS &fs, const char *dirname, uint8_t levels);
void createDir(fs::FS &fs, const char *path);
void removeDir(fs::FS &fs, const char *path);
void readFile(fs::FS &fs, const char *path);
void writeFile(fs::FS &fs, const char *path, const char *message);
void appendFile(fs::FS &fs, const char *path, const char *message);
void renameFile(fs::FS &fs, const char *path1, const char *path2);
void deleteFile(fs::FS &fs, const char *path);

#endif // UTILITIES_H_


