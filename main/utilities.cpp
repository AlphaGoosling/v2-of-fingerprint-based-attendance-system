#include "utilities.h"

/********************************************************************************************************************************************
                                                         WIFI FUNCTIONS               
*********************************************************************************************************************************************/  
#define EXAMPLE_ESP_WIFI_SSID      "redmi-black"
#define EXAMPLE_ESP_WIFI_PASS      "77777777"
#define EXAMPLE_ESP_MAXIMUM_RETRY  CONFIG_ESP_MAXIMUM_RETRY

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

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

char *TAG = "wifi station";

static int s_retry_num = 0;

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

extern "C" void wifi_init_sta(void)
{
  s_wifi_event_group = xEventGroupCreate();

  ESP_ERROR_CHECK(esp_netif_init());

  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  esp_event_handler_instance_t instance_any_id;
  esp_event_handler_instance_t instance_got_ip;
  ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                      ESP_EVENT_ANY_ID,
                                                      &event_handler,
                                                      NULL,
                                                      &instance_any_id));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                      IP_EVENT_STA_GOT_IP,
                                                      &event_handler,
                                                      NULL,
                                                      &instance_got_ip));

  wifi_config_t wifi_config = {
    .sta = {
      .ssid = EXAMPLE_ESP_WIFI_SSID ,
      .password = EXAMPLE_ESP_WIFI_PASS,
      /* Authmode threshold resets to WPA2 as default if password matches WPA2 standards (password len => 8).
        * If you want to connect the device to deprecated WEP/WPA networks, Please set the threshold value
        * to WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK and set the password with length and format matching to
        * WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK standards.
        */
      .threshold = {.rssi = -90, .authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD, .rssi_5g_adjustment = 20},
      .sae_pwe_h2e = ESP_WIFI_SAE_MODE,
      .sae_h2e_identifier = EXAMPLE_H2E_IDENTIFIER,
    },
  };
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
  ESP_ERROR_CHECK(esp_wifi_start() );

  ESP_LOGI(TAG, "wifi_init_sta finished.");

  /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
    * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
  EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
          WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
          pdFALSE,
          pdFALSE,
          portMAX_DELAY);

  /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
    * happened. */
  if (bits & WIFI_CONNECTED_BIT) {
    ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
              EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
  } else if (bits & WIFI_FAIL_BIT) {
      ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
  } else {
      ESP_LOGE(TAG, "UNEXPECTED EVENT");
  }
}


/********************************************************************************************************************************************
                                                         DISPLAY FUNCTIONS               
*********************************************************************************************************************************************/  

extern TFT_eSPI tft;

char numberBuffer[NUM_LEN + 1] = "";
uint8_t numberIndex = 0;

uint8_t onScreen; // keeps track of what is displayed on screen

TFT_eSPI_Button keyboardKeys[40];    //keyboard buttons 
TFT_eSPI_Button mainMenuKeys[5]; //main menu buttons

const char keyboardKeyLabels[40] = {'1', '2', '3', '4', '5', '6', '7', '8', '9', '0', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', 
                                    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', '>', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', ' ', ' ', '<' };

char *mainMenuKeyLabels[5] = {"WiFi", "Register Attendance", "Add new Student", "Delete entry", "Connect to Server"};

void drawKeyboard(){
   // Draw keyboard background
  tft.fillRect(0, 300, 320, 180, TFT_DARKGREY);

  // Draw the keys
  tft.setFreeFont(LABEL1_FONT);
  char keyboardKeyLabel[3] = {' ', '\0', ' '}; // Created to convert char to char * for initButton function
  for (uint8_t row = 0; row < 4; row++) {
    for (uint8_t col = 0; col < 10; col++) {
      uint8_t b = col + row * 10;
      
      keyboardKeyLabel[0] = keyboardKeyLabels[b];
      if (b == 37){
        keyboardKeyLabel[0] = '_'; keyboardKeyLabel[1] = '_'; keyboardKeyLabel[2] = '\0';
        keyboardKeys[b].initButton(&tft, KEY_X * 2 + col * (KEY_W + KEY_SPACING_X),
                        KEY_Y + row * (KEY_H + KEY_SPACING_Y), // x, y, w, h, outline, fill, text
                        KEY_W * 2 + KEY_SPACING_X , KEY_H, TFT_WHITE, TFT_BLUE, TFT_WHITE,
                        keyboardKeyLabel, KEY_TEXTSIZE);
      }
      else if (b == 38){
        continue;
      }
      else if (b == 39){
        keyboardKeyLabel[0] = '<'; keyboardKeyLabel[1] = '\0'; keyboardKeyLabel[2] = ' ';
        keyboardKeys[b].initButton(&tft, KEY_X + col * (KEY_W + KEY_SPACING_X) - KEY_SPACING_X/2,
                        KEY_Y + row * (KEY_H + KEY_SPACING_Y), 
                        KEY_W, KEY_H + KEY_SPACING_X, TFT_WHITE, TFT_RED, TFT_WHITE,
                        keyboardKeyLabel, KEY_TEXTSIZE);
      }
      else if (b == 29){
        keyboardKeys[b].initButton(&tft, KEY_X + col * (KEY_W + KEY_SPACING_X),
                        KEY_Y + row * (KEY_H + KEY_SPACING_Y), 
                        KEY_W, KEY_H, TFT_WHITE, TFT_GREEN, TFT_WHITE,
                        keyboardKeyLabel, KEY_TEXTSIZE);
      }
      else{
        keyboardKeys[b].initButton(&tft, KEY_X + col * (KEY_W + KEY_SPACING_X),
                        KEY_Y + row * (KEY_H + KEY_SPACING_Y), 
                        KEY_W, KEY_H, TFT_WHITE, TFT_BLUE, TFT_WHITE,
                        keyboardKeyLabel, KEY_TEXTSIZE);
      }
      keyboardKeys[b].drawButton();
    }
  }
}


void drawMainmenu(){
  onScreen = MAINMENU;
  tft.fillScreen((TFT_BLACK));
  tft.setFreeFont(MAINMENU_FONT);
  typeString("MAIN MENU", 85 , 25);
  
  for (uint8_t i = 0; i < 5; i++){
    mainMenuKeys[i]. initButton(&tft, SCREEN_W / 2 , (SCREEN_H * (i+1)/ 6 ) + 40,
                            MAINKEY_W, MAINKEY_H, TFT_WHITE, TFT_DARKGREEN, TFT_WHITE,
                            mainMenuKeyLabels[i], KEY_TEXTSIZE);
    mainMenuKeys[i].drawButton();
  }
}

void drawWifiMenu(){
  onScreen = WIFI_MENU;
  tft.fillScreen((TFT_BLACK));
  tft.setFreeFont(MAINMENU_FONT);
  
  typeString("WIFI", 120 , 25);

  tft.fillRoundRect(40, 70, 240, 225, 10, 0x3186);
  tft.setFreeFont(&FreeSans9pt7b);
  tft.textbgcolor = 0x3186;
  String WifiOptions[] = {"Wi-Fi", "Status", "ssid", "password"};
  for (u8_t i = 0; i < 4; i++){
    tft.drawString(WifiOptions[i], 60, 90 + 40*i);
  }
  tft.textcolor = TFT_GREEN;
  tft.setFreeFont(&FreeSerifItalic9pt7b);
  tft.drawString("#password must be between 8", 50, 250);
  tft.drawString("and 12 characters long", 55, 270);

  tft.fillRoundRect(150, 160, 120, 35, 5, 0x3186);
  tft.drawRoundRect(150, 160, 120, 35, 5, TFT_DARKGREY);
  tft.fillRoundRect(150, 200, 120, 35, 5, 0x3186);
  tft.drawRoundRect(150, 200, 120, 35, 5, TFT_DARKGREY);

  drawKeyboard();
}

void typeString(const char *msg, int x, int y) {      
  tft.fillRect(x, y, (320-x),(120-y), TFT_BLACK); 

  uint16_t endOfString = x; //endOfString is used to keep track of the end of the string (in pixels) that we have displayed so far
  u16_t i = 0,  j = 0; // i is used to iterate through the message string // j is used to mark the location of the last space
  char message[2] ={' ', '\0'}; //created just to convert each char in msg to char * for tft.drawString method 

  while (msg[i] != '\0' ){
    if (msg[i] == ' '){
      j = i;
    }
    message[0] = msg[i];
    endOfString += tft.drawString(message, endOfString, y);
    printf("%c \n", msg[i]);
    if (endOfString > 320){   //if the string goes beyond the edge of the screen
      tft.fillRect(j, y, (320-j),(120-y), TFT_BLACK);
      i = j; 
      y += 30;
      endOfString = x;
      //continue;
    }
    i++;
  }
}

void typeString(const char *msg) {     
  int x = 40; int y = 60; 
  tft.fillRect(x, y, (320-x),(120-y), TFT_BLACK); 

  uint16_t endOfString = x; //endOfString is used to keep track of the end of the string (in pixels) that we have displayed so far
  u16_t i = 0,  j = 0; // i is used to iterate through the message string // j is used to mark the location of the last space
  char message[2] ={' ', '\0'}; //created just to convert each char in msg to char * for tft.drawString method 

  while (msg[i] != '\0' ){
    if (msg[i] == ' '){
      j = i;
    }
    message[0] = msg[i];
    endOfString += tft.drawString(message, endOfString, y);
    printf("%c \n", msg[i]);
    if (endOfString > 320){   //if the string goes beyond the edge of the screen
      tft.fillRect(j, y, (320-j),(120-y), TFT_BLACK);
      i = j; 
      y += 30;
      endOfString = x;
      //continue;
    }
    i++;
  }
}

void typeString(int id, int x, int y) {     
  const char *msg = (const char *)id;
  tft.fillRect(x, y, (320-x),(120-y), TFT_BLACK); 

  uint16_t endOfString = x; //endOfString is used to keep track of the end of the string (in pixels) that we have displayed so far
  u16_t i = 0,  j = 0; // i is used to iterate through the message string // j is used to mark the location of the last space
  char message[2] ={' ', '\0'}; //created just to convert each char in msg to char * for tft.drawString method 

  while (msg[i] != '\0' ){
    if (msg[i] == ' '){
      j = i;
    }
    message[0] = msg[i];
    endOfString += tft.drawString(message, endOfString, y);
    printf("%c \n", msg[i]);
    if (endOfString > 320){   //if the string goes beyond the edge of the screen
      tft.fillRect(j, y, (320-j),(120-y), TFT_BLACK);
      i = j; 
      y += 30;
      endOfString = x;
      //continue;
    }
    i++;
  }
}

/********************************************************************************************************************************************
                                                      FINGERPRINT SCANNER FUNCTIONS               
*********************************************************************************************************************************************/  
extern Adafruit_Fingerprint finger;
extern HardwareSerial fingerprintSerial;

uint8_t getFingerprintEnroll(int id) {
  
  int p = -1;
  typeString("Waiting for valid finger to enroll as #"); typeString(id, 0, 80);
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    vTaskDelay(100/ portTICK_PERIOD_MS);
    switch (p) {
    case FINGERPRINT_OK:
      typeString("Image taken");
      break;
    case FINGERPRINT_NOFINGER:
      typeString(".");
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
      typeString("Communication error");
      break;
    case FINGERPRINT_IMAGEFAIL:
      typeString("Imaging error");
      break;
    default:
      typeString("Unknown error");
      break;
    }
  }

  // OK success!

  p = finger.image2Tz(1);
  switch (p) {
    case FINGERPRINT_OK:
      typeString("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      typeString("Image too messy");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      typeString("Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      typeString("Could not find fingerprint features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      typeString("Could not find fingerprint features");
      return p;
    default:
      typeString("Unknown error");
      return p;
  }

  typeString("Remove finger");
  vTaskDelay(200 / portTICK_PERIOD_MS);
  p = 0;
  while (p != FINGERPRINT_NOFINGER) {
    p = finger.getImage();
    vTaskDelay(100/ portTICK_PERIOD_MS);
  }
  typeString("ID "); typeString(id, 0, 80);
  p = -1;
  typeString("Place same finger again");
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    vTaskDelay(100/ portTICK_PERIOD_MS);
    switch (p) {
    case FINGERPRINT_OK:
      typeString("Image taken");
      break;
    case FINGERPRINT_NOFINGER:
      typeString(".");
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
      typeString("Communication error");
      break;
    case FINGERPRINT_IMAGEFAIL:
      typeString("Imaging error");
      break;
    default:
      typeString("Unknown error");
      break;
    }
  }

  // OK success!

  p = finger.image2Tz(2);
  switch (p) {
    case FINGERPRINT_OK:
      typeString("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      typeString("Image too messy");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      typeString("Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      typeString("Could not find fingerprint features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      typeString("Could not find fingerprint features");
      return p;
    default:
      typeString("Unknown error");
      return p;
  }

  // OK converted!
  typeString("Creating model for #");  typeString(id, 0, 80);

  p = finger.createModel();
  if (p == FINGERPRINT_OK) {
    typeString("Prints matched!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    typeString("Communication error");
    return p;
  } else if (p == FINGERPRINT_ENROLLMISMATCH) {
    typeString("Fingerprints did not match");
    return p;
  } else {
    typeString("Unknown error");
    return p;
  }

  typeString("ID "); typeString(id, 0, 80);
  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK) {
    typeString("Stored!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    typeString("Communication error");
    return p;
  } else if (p == FINGERPRINT_BADLOCATION) {
    typeString("Could not store in that location");
    return p;
  } else if (p == FINGERPRINT_FLASHERR) {
    typeString("Error writing to flash");
    return p;
  } else {
    typeString("Unknown error");
    return p;
  }

  return true;
}

uint8_t getFingerprintID() {
  uint8_t p = finger.getImage();
  switch (p) {
    case FINGERPRINT_OK:
      typeString("Image taken");
      break;
    case FINGERPRINT_NOFINGER:
      typeString("No finger detected");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      typeString("Communication error");
      return p;
    case FINGERPRINT_IMAGEFAIL:
      typeString("Imaging error");
      return p;
    default:
      typeString("Unknown error");
      return p;
  }

  // OK success!

  p = finger.image2Tz();
  switch (p) {
    case FINGERPRINT_OK:
      typeString("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      typeString("Image too messy");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      typeString("Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      typeString("Could not find fingerprint features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      typeString("Could not find fingerprint features");
      return p;
    default:
      typeString("Unknown error");
      return p;
  }

  // OK converted!
  p = finger.fingerSearch();
  if (p == FINGERPRINT_OK) {
    typeString("Found a print match!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    typeString("Communication error");
    return p;
  } else if (p == FINGERPRINT_NOTFOUND) {
    typeString("Did not find a match");
    return p;
  } else {
    typeString("Unknown error");
    return p;
  }

  // found a match!
  typeString("Found ID #"); typeString(finger.fingerID, 0, 80);
  typeString(" with confidence of "); typeString(finger.confidence, 0, 80);

  return finger.fingerID;
}

// returns -1 if failed, otherwise returns ID #
int getFingerprintIDez() {
  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK)  return -1;

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK)  return -1;

  p = finger.fingerFastSearch();
  if (p != FINGERPRINT_OK)  return -1;

  // found a match!
  typeString("Found ID #"); typeString(finger.fingerID, 0, 80);
  typeString(" with confidence of "); typeString(finger.confidence, 0, 80);
  return finger.fingerID;
}

void printHex(int num, int precision) {
  char tmp[16];
  char format[128];

  sprintf(format, "%%.%dX", precision);

  sprintf(tmp, format, num);
  typeString(tmp);
}

uint8_t downloadFingerprintTemplate(uint16_t id)
{
  typeString("------------------------------------");
  typeString("Attempting to load #"); typeString(id, 0, 80);
  uint8_t p = finger.loadModel(id);
  switch (p) {
    case FINGERPRINT_OK:
      typeString("Template "); typeString(id, 0, 80); typeString(" loaded");
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
      typeString("Communication error");
      return p;
    default:
      typeString("Unknown error "); typeString(p, 0, 80);
      return p;
  }

  // OK success!

  typeString("Attempting to get #"); typeString(id, 0, 80);
  p = finger.getModel();
  switch (p) {
    case FINGERPRINT_OK:
      typeString("Template "); typeString(id, 0, 80); typeString(" transferring:");
      break;
    default:
      typeString("Unknown error "); typeString(p, 0, 80);
      return p;
  }

  // one data packet is 267 bytes. in one data packet, 11 bytes are 'usesless' :D
  uint8_t bytesReceived[534]; // 2 data packets
  memset(bytesReceived, 0xff, 534);

  uint32_t starttime = millis();
  int i = 0;
  while (i < 534 && (millis() - starttime) < 20000) {
    if (fingerprintSerial.available()) {
      bytesReceived[i++] = fingerprintSerial.read();
    }
  }
  typeString(i, 0, 80); typeString(" bytes read.");
  typeString("Decoding packet...");

  uint8_t fingerTemplate[512]; // the real template
  memset(fingerTemplate, 0xff, 512);

  // filtering only the data packets
  int uindx = 9, index = 0;
  memcpy(fingerTemplate + index, bytesReceived + uindx, 256);   // first 256 bytes
  uindx += 256;       // skip data
  uindx += 2;         // skip checksum
  uindx += 9;         // skip next header
  index += 256;       // advance pointer
  memcpy(fingerTemplate + index, bytesReceived + uindx, 256);   // second 256 bytes

  for (int i = 0; i < 512; ++i) {
    //typeString("0x");
    printHex(fingerTemplate[i], 2);
    //typeString(", ");
  }
  typeString("\ndone.");

  return p;

  /*
  uint8_t templateBuffer[256];
  memset(templateBuffer, 0xff, 256);  //zero out template buffer
  int index=0;
  uint32_t starttime = millis();
  while ((index < 256) && ((millis() - starttime) < 1000))
  {
  if (mySerial.available())
  {
    templateBuffer[index] = mySerial.read();
    index++;
  }
  }

  typeString(index); typeString(" bytes read");

  //dump entire templateBuffer.  This prints out 16 lines of 16 bytes
  for (int count= 0; count < 16; count++)
  {
  for (int i = 0; i < 16; i++)
  {
    typeString("0x");
    typeString(templateBuffer[count*16+i], HEX);
    typeString(", ");
  }
  typeString();
  }*/
}