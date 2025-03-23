#if !defined(UTILITIES_H_)
#define UTILITIES_H_

#include "Arduino.h"
#include "Adafruit_Fingerprint.h"
#include "TFT_eSPI.h"
#include "SPI.h"

extern "C"{
  #include <string.h>
  #include "freertos/FreeRTOS.h"
  #include "freertos/task.h"
  #include "freertos/event_groups.h"
  #include "esp_system.h"
  #include "esp_wifi.h"
  #include "esp_event.h"
  #include "esp_log.h"
  #include "nvs_flash.h"
  #include "LittleFS.h"
  #include "golioth/client.h"
  #include "lwip/err.h"
  #include "lwip/sys.h"
}

#define SCREEN_W 320
#define SCREEN_H 480

#define MAINMENU              0
#define WIFI                  1
#define REGISTER_ATTENDANCE   2
#define ADD_NEW_STUDENT       3
#define DELETE_ENTRY          4
#define CONNECT_TO_SERVER     5

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

// Number length, buffer for storing it and character index
#define NUM_LEN 12


extern "C" void wifi_init_sta(void);

void drawKeyboard();
void drawMainmenu();
void typeString(const char *msg, int x, int y);

#endif // UTILITIES_H_


