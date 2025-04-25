#include "utilities.h"
extern TFT_eSPI tft;
extern uint8_t onScreen;
extern struct golioth_client *client;
extern TaskHandle_t NetworkTaskHandler;
/********************************************************************************************************************************************
                                                         WIFI FUNCTIONS               
*********************************************************************************************************************************************/

extern bool wifiOn;

char wifiSsid[32] = "redmi-black";
char wifiPassword[64] = "77777777";

char *TAG_W = "wifi station";

static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data){
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    esp_wifi_connect(); 
  } 

  else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) { 
    wifiOn = false;
    if (onScreen == WIFI_MENU){
      tft.textcolor = TFT_RED;
      tft.setFreeFont(&FreeSans9pt7b);
      tft.fillRect(110, 130, 180, 22, 0x3186);
      tft.textbgcolor = 0x3186;
      tft.drawString("DISCONNECTED", 110, 130);
    }
    ESP_ERROR_CHECK(esp_wifi_stop());
  } 
  
  else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
    ESP_LOGI(TAG_W, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));

    wifiOn = true;
    if (onScreen == WIFI_MENU){
      tft.textcolor = TFT_GREEN;
      tft.setFreeFont(&FreeSans9pt7b);
      tft.fillRect(110, 130, 180, 22, 0x3186);
      tft.textbgcolor = 0x3186;
      tft.drawString("CONNECTED", 115, 130);
    }
  }
}

extern "C" void setWifiCredentials(){
  wifi_config_t wifi_config = {
    .sta = {
      .ssid = "admin" ,
      .password = "adminpassword",
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
  if (!wifiOn){
    strncpy((char*)((&wifi_config)->sta.ssid), wifiSsid, 32);
    strncpy((char*)((&wifi_config)->sta.password), wifiPassword, 64);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );

    ESP_LOGI(TAG_W, "wifi ssid is %s\n", wifi_config.sta.ssid);
    ESP_LOGI(TAG_W, "wifi password is %s\n", wifi_config.sta.password);
  }
  else{
    ESP_LOGE(TAG_W, "Wifi needs to be off before changing configuration");
    ESP_LOGE(TAG_W, "Wifi Credentials not set");
    return;
  }
}

extern "C" void wifi_init_sta(void)
{
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
  setWifiCredentials();
  ESP_LOGI(TAG_W, "wifi_init_sta finished.");
}

/********************************************************************************************************************************************
                                                         DISPLAY FUNCTIONS               
*********************************************************************************************************************************************/  
extern u8_t attendanceFileNum;
bool keyboardOnScreen = false;
extern bool wifiOn;

TFT_eSPI_Button keyboardKeys[40];    //keyboard buttons 
TFT_eSPI_Button mainMenuKeys[5]; //main menu buttons
TFT_eSPI_Button studentClasses[8];
TFT_eSPI_Button BackButton;
TFT_eSPI_Button PasswordField;
TFT_eSPI_Button WifiSsidField;
TFT_eSPI_Button WifiOnOffButton;
TFT_eSPI_Button TakeAttendanceButton;

char keyboardKeyLabels[40] = {'1', '2', '3', '4', '5', '6', '7', '8', '9', '0', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', 
                                    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', '>', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', ' ', ' ', '<' };

char *mainMenuKeyLabels[5] = {"WiFi", "Register Attendance", "Add new Entry", "Delete Entry", "Sync With Server"};

char studentClassLabels[8][16];


void drawKeyboard(){
  keyboardOnScreen = true;
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
  tft.setFreeFont(&FreeSans9pt7b);
  BackButton.initButton(&tft, 32, 16, 60, 30, TFT_DARKGREY, 0xf9c7, TFT_WHITE, "Back", KEY_TEXTSIZE);
  BackButton.drawButton();
}


void drawMainmenu(){
  onScreen = MAINMENU;
  tft.fillScreen((TFT_BLACK));
  tft.textcolor = TFT_WHITE;
  tft.textbgcolor = TFT_BLACK;
  tft.setFreeFont(MAINMENU_FONT);
  tft.drawString("MAIN MENU", 85 , 25);
  
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
  tft.textcolor = TFT_WHITE;
  tft.textbgcolor = TFT_BLACK;
  tft.setFreeFont(MAINMENU_FONT);
  
  tft.drawString("WIFI", 120 , 25);

  tft.fillRoundRect(20, 70, 280, 225, 10, 0x3186);
  tft.setFreeFont(&FreeSans9pt7b);
  tft.textbgcolor = 0x3186;
  String WifiOptions[] = {"Wi-Fi", "Status", "Ssid", "Password"};
  for (u8_t i = 0; i < 4; i++){
    tft.drawString(WifiOptions[i], 32, 90 + 40*i);
  }
  
  tft.textcolor = TFT_GREEN;
  tft.setFreeFont(&FreeSerifItalic9pt7b);
  tft.drawString("#password must be between 8 and 15", 30, 250);
  tft.drawString("characters long", 35, 270);

  WifiOnOffButton.initButton(&tft, 126, 98, 45, 24, 0x3186, 0x3186, 0x3186, " ", KEY_TEXTSIZE);
  WifiOnOffButton.drawButton();
  tft.fillRoundRect(104, 86, 45, 24, 12, wifiOn? TFT_DARKGREEN : TFT_DARKGREY);
  tft.drawRoundRect(104, 86, 45, 24, 12, TFT_BLACK);
  tft.fillCircle(wifiOn? 136 : 115, 97, 8, TFT_WHITE);

  tft.textcolor = wifiOn ? TFT_GREEN : TFT_RED;
  tft.setFreeFont(&FreeSans9pt7b);
  tft.drawString(wifiOn? "CONNECTED" : "DISCONNECTED", 115, 130);

  WifiSsidField.initButton(&tft, 204, 176, 170, 30, TFT_DARKGREY, 0x3186, TFT_WHITE, " ", KEY_TEXTSIZE);
  WifiSsidField.drawButton();

  PasswordField.initButton(&tft, 204, 216, 170, 30, TFT_DARKGREY, 0x3186, TFT_WHITE, " ", KEY_TEXTSIZE);
  PasswordField.drawButton();

  tft.setFreeFont(&FreeSans9pt7b);
  BackButton.initButton(&tft, 275, 455, 60, 30, TFT_DARKGREY, 0xf9c7, TFT_WHITE, "Back", KEY_TEXTSIZE);
  BackButton.drawButton();
}

void drawRegisterAttendanceMenu(){
  onScreen = REGISTER_ATTENDANCE;
  tft.fillScreen((TFT_BLACK));
  tft.textcolor = TFT_WHITE;
  tft.textbgcolor = TFT_BLACK;
  tft.setFreeFont(MAINMENU_FONT);
  //tft.setTextDatum(TL_DATUM); 
  tft.drawString("LOAD CLASS FILE", 12, 30);

  tft.setFreeFont(&FreeSans9pt7b);
  Serial.print("Number of attendance data files in littlefs storage: "); Serial.println(attendanceFileNum); Serial.println(" ");

  tft.fillRoundRect(40, 70, 240, 80 + 40 * attendanceFileNum, 10, 0x3186);
  for (u8_t i = 0; i < attendanceFileNum; i++){
    Serial.print("studentClassLabels[i]: "); Serial.println(studentClassLabels[i]);
    studentClasses[i].initButton(&tft, 95, 90 + i * 40, 100, 30, 0x3186, 0x3186, TFT_WHITE, studentClassLabels[i], KEY_TEXTSIZE);
    studentClasses[i].drawButton();
  }

  for (u8_t i = 0; i < attendanceFileNum; i++){
    tft.drawLine(45, 97 + i * 40, 147, 97 + i * 40, TFT_WHITE);
  }

  tft.textcolor = TFT_GREEN;
  tft.textbgcolor = 0x3186;
  tft.setFreeFont(&FreeSerifItalic9pt7b);
  tft.drawString("#Select a students list to be used", 48, 65 + 40 * attendanceFileNum);
  tft.drawString("to register attendance", 48, 85 + 40 * attendanceFileNum);

  tft.setFreeFont(&FreeSans9pt7b);
  TakeAttendanceButton.initButton(&tft, 275, 270 + 40 * attendanceFileNum, 120, 30, TFT_DARKGREY, TFT_GREEN, TFT_WHITE, "Take Attendance", KEY_TEXTSIZE);
  TakeAttendanceButton.drawButton();

  tft.setFreeFont(&FreeSans9pt7b);
  BackButton.initButton(&tft, 275, 455, 60, 30, TFT_DARKGREY, 0xf9c7, TFT_WHITE, "Back", KEY_TEXTSIZE);
  BackButton.drawButton();
}

void drawAddNewStudentMenu(){
  onScreen = ADD_NEW_STUDENT;
  tft.fillScreen((TFT_BLACK));
  tft.textcolor = TFT_WHITE;
  tft.textbgcolor = TFT_BLACK;
  tft.setFreeFont(MAINMENU_FONT);
  tft.drawString("ADD NEW STUDENT", 40 , 30);

  tft.fillRoundRect(40, 70, 240, 225, 10, 0x3186);
  tft.setFreeFont(&FreeSans9pt7b);
  tft.textbgcolor = 0x3186;
  String WifiOptions[] = {"First Name", "Last Name", "Std No."};
  for (u8_t i = 0; i < 3; i++){
    tft.drawString(WifiOptions[i], 50, 100 + 50*i);
  }
  tft.textcolor = TFT_GREEN;
  tft.setFreeFont(&FreeSerifItalic9pt7b);
  tft.drawString("#password must be between 8", 50, 250);
  tft.drawString("and 12 characters long", 55, 270);

  TFT_eSPI_Button FirstNameField;
  FirstNameField.initButton(&tft, 205, 110, 125, 30, TFT_DARKGREY, 0x3186, TFT_WHITE, " ", KEY_TEXTSIZE);
  FirstNameField.drawButton();

  TFT_eSPI_Button LastNameField;
  LastNameField.initButton(&tft, 205, 160, 125, 30, TFT_DARKGREY, 0x3186, TFT_WHITE, " ", KEY_TEXTSIZE);
  LastNameField.drawButton();

  TFT_eSPI_Button StdNumberField;
  StdNumberField.initButton(&tft, 205, 210, 125, 30, TFT_DARKGREY, 0x3186, TFT_WHITE, " ", KEY_TEXTSIZE);
  StdNumberField.drawButton();

  tft.setFreeFont(&FreeSans9pt7b);
  BackButton.initButton(&tft, 275, 455, 60, 30, TFT_DARKGREY, 0xf9c7, TFT_WHITE, "Back", KEY_TEXTSIZE);
  BackButton.drawButton();


  //drawKeyboard();
}

void drawDeleteStudentEntry(){
  onScreen = DELETE_STUDENT_ENTRY ;
  tft.fillScreen((TFT_BLACK));
  tft.textcolor = TFT_WHITE;
  tft.textbgcolor = TFT_BLACK;
  tft.setFreeFont(MAINMENU_FONT);
  tft.drawString("DELETE STUDENT", 55, 30);

  tft.fillRoundRect(40, 70, 240, 225, 10, 0x3186);
  tft.setFreeFont(&FreeSans9pt7b);
  tft.textbgcolor = 0x3186;
  tft.drawString("Student number :", 50, 90);

  TFT_eSPI_Button StudentNumberField;
  tft.setTextDatum(TL_DATUM);
  StudentNumberField.initButton(&tft, 150, 123, 200, 30, TFT_DARKGREY, 0x3186, TFT_WHITE, " ", KEY_TEXTSIZE);
  StudentNumberField.drawButton();

  tft.setFreeFont(&FreeSans9pt7b);
  BackButton.initButton(&tft, 275, 455, 60, 30, TFT_DARKGREY, 0xf9c7, TFT_WHITE, "Back", KEY_TEXTSIZE);
  BackButton.drawButton();
}

void drawSyncWithServer(){
  onScreen = SYNC_WITH_SERVER;
  tft.fillScreen((TFT_BLACK));
  tft.textcolor = TFT_WHITE;
  tft.textbgcolor = TFT_BLACK;
  tft.setFreeFont(MAINMENU_FONT);
  tft.drawString("SYNC WITH SERVER", 40, 30);

  tft.fillRoundRect(40, 130, 240, 100, 10, 0x3186);
  tft.setFreeFont(&FreeSans9pt7b);
  tft.textbgcolor = 0x3186;
  tft.drawString("Synchronizing . . .", 50, 168);

  tft.setFreeFont(&FreeSans9pt7b);
  BackButton.initButton(&tft, 275, 455, 60, 30, TFT_DARKGREY, 0xf9c7, TFT_WHITE, "Back", KEY_TEXTSIZE);
  BackButton.drawButton();
}


/********************************************************************************************************************************************
                                                      FINGERPRINT SCANNER FUNCTIONS               
*********************************************************************************************************************************************/  
extern Adafruit_Fingerprint finger;
extern HardwareSerial fingerprintSerial;

uint8_t getFingerprintEnroll(int id) {
  
  int p = -1;
  Serial.print("Waiting for valid finger to enroll as #"); Serial.println(id);
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    vTaskDelay(100/ portTICK_PERIOD_MS);
    switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image taken");
      break;
    case FINGERPRINT_NOFINGER:
      Serial.println(".");
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      break;
    case FINGERPRINT_IMAGEFAIL:
      Serial.println("Imaging error");
      break;
    default:
      Serial.println("Unknown error");
      break;
    }
  }

  // OK success!

  p = finger.image2Tz(1);
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }

  Serial.println("Remove finger");
  vTaskDelay(200 / portTICK_PERIOD_MS);
  p = 0;
  while (p != FINGERPRINT_NOFINGER) {
    p = finger.getImage();
    vTaskDelay(100/ portTICK_PERIOD_MS);
  }
  Serial.print("ID "); Serial.println(id);
  p = -1;
  Serial.println("Place same finger again");
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    vTaskDelay(100/ portTICK_PERIOD_MS);
    switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image taken");
      break;
    case FINGERPRINT_NOFINGER:
      Serial.println(".");
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      break;
    case FINGERPRINT_IMAGEFAIL:
      Serial.println("Imaging error");
      break;
    default:
      Serial.println("Unknown error");
      break;
    }
  }

  // OK success!

  p = finger.image2Tz(2);
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }

  // OK converted!
  Serial.print("Creating model for #");  Serial.println(id);

  p = finger.createModel();
  if (p == FINGERPRINT_OK) {
    Serial.println("Prints matched!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    return p;
  } else if (p == FINGERPRINT_ENROLLMISMATCH) {
    Serial.println("Fingerprints did not match");
    return p;
  } else {
    Serial.println("Unknown error");
    return p;
  }

  Serial.print("ID "); Serial.println(id);
  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK) {
    Serial.println("Stored!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    return p;
  } else if (p == FINGERPRINT_BADLOCATION) {
    Serial.println("Could not store in that location");
    return p;
  } else if (p == FINGERPRINT_FLASHERR) {
    Serial.println("Error writing to flash");
    return p;
  } else {
    Serial.println("Unknown error");
    return p;
  }

  return true;
}

uint8_t getFingerprintID() {
  uint8_t p = finger.getImage();
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image taken");
      break;
    case FINGERPRINT_NOFINGER:
      Serial.println("No finger detected");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_IMAGEFAIL:
      Serial.println("Imaging error");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }

  // OK success!

  p = finger.image2Tz();
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }

  // OK converted!
  p = finger.fingerSearch();
  if (p == FINGERPRINT_OK) {
    Serial.println("Found a print match!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    return p;
  } else if (p == FINGERPRINT_NOTFOUND) {
    Serial.println("Did not find a match");
    return p;
  } else {
    Serial.println("Unknown error");
    return p;
  }

  // found a match!
  Serial.print("Found ID #"); Serial.println(finger.fingerID);
  Serial.print(" with confidence of "); Serial.println(finger.confidence);

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
  Serial.print("Found ID #"); Serial.println(finger.fingerID);
  Serial.print(" with confidence of "); Serial.println(finger.confidence);
  return finger.fingerID;
}

void printHex(int num, int precision) {
  char tmp[16];
  char format[128];

  sprintf(format, "%%.%dX", precision);

  sprintf(tmp, format, num);
  Serial.println(tmp);
}

uint8_t downloadFingerprintTemplate(uint16_t id)
{
  Serial.println("------------------------------------");
  Serial.print("Attempting to load #"); Serial.println(id);
  uint8_t p = finger.loadModel(id);
  switch (p) {
    case FINGERPRINT_OK:
      Serial.print("Template "); Serial.print(id); Serial.println(" loaded");
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    default:
      Serial.print("Unknown error "); Serial.println(p);
      return p;
  }

  // OK success!

  Serial.print("Attempting to get #"); Serial.println(id);
  p = finger.getModel();
  switch (p) {
    case FINGERPRINT_OK:
      Serial.print("Template "); Serial.print(id); Serial.println(" transferring:");
      break;
    default:
      Serial.print("Unknown error "); Serial.println(p);
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
  Serial.print(i); Serial.println(" bytes read.");
  Serial.println("Decoding packet...");

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
    //Serial.println("0x");
    printHex(fingerTemplate[i], 2);
    //Serial.println(", ");
  }
  Serial.println("\ndone.");

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

  Serial.println(index); Serial.println(" bytes read");

  //dump entire templateBuffer.  This prints out 16 lines of 16 bytes
  for (int count= 0; count < 16; count++)
  {
  for (int i = 0; i < 16; i++)
  {
    Serial.println("0x");
    Serial.println(templateBuffer[count*16+i], HEX);
    Serial.println(", ");
  }
  Serial.println();
  }*/
}

/********************************************************************************************************************************************
                                                        FILE MANAGEMENT FUNCTIONS               
*********************************************************************************************************************************************/  

u8_t listDir(fs::FS &fs, const char *dirname, uint8_t levels) {
  u8_t fileNum = 0;
  Serial.printf("Listing directory: %s\n", dirname);

  fs::File root = fs.open(dirname);
  if (!root) {
    Serial.println("Failed to open directory");
    return 0;
  }
  if (!root.isDirectory()) {
    Serial.println("Not a directory");
    return 0;
  }

  fs::File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      Serial.print("  DIR : ");
      Serial.print(file.name());
      time_t t = file.getLastWrite();
      struct tm *tmstruct = localtime(&t);
      Serial.printf(
        "  LAST WRITE: %d-%02d-%02d %02d:%02d:%02d\n", (tmstruct->tm_year) + 1900, (tmstruct->tm_mon) + 1, tmstruct->tm_mday, tmstruct->tm_hour,
        tmstruct->tm_min, tmstruct->tm_sec
      );
      if (levels) {
        listDir(fs, file.path(), levels - 1);
      }
    } 
    else {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      strncpy( studentClassLabels[fileNum], file.name(), 11); //put names of files in the directory in the list
      Serial.print("  SIZE: ");
      Serial.print(file.size());
      time_t t = file.getLastWrite();
      struct tm *tmstruct = localtime(&t);
      Serial.printf(
        "  LAST WRITE: %d-%02d-%02d %02d:%02d:%02d\n", (tmstruct->tm_year) + 1900, (tmstruct->tm_mon) + 1, tmstruct->tm_mday, tmstruct->tm_hour,
        tmstruct->tm_min, tmstruct->tm_sec
      );
      fileNum++;
    }
    file = root.openNextFile();
  }
  return fileNum; //return total number of files in directory
}

void createDir(fs::FS &fs, const char *path) {
  Serial.printf("Creating Dir: %s\n", path);
  if (fs.mkdir(path)) {
    Serial.println("Dir created");
  } else {
    Serial.println("mkdir failed");
  }
}

void removeDir(fs::FS &fs, const char *path) {
  Serial.printf("Removing Dir: %s\n", path);
  if (fs.rmdir(path)) {
    Serial.println("Dir removed");
  } else {
    Serial.println("rmdir failed");
  }
}

void readFile(fs::FS &fs, const char *path) {
  Serial.printf("Reading file: %s\n", path);

  fs::File file = fs.open(path);
  if (!file) {
    Serial.println("Failed to open file for reading");
    return;
  }

  Serial.print("Read from file: ");
  while (file.available()) {
    Serial.write(file.read());
  }
  file.close();
}

void writeFile(fs::FS &fs, const char *path, const char *message) {
  Serial.printf("Writing file: %s\n", path);

  fs::File file = fs.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  if (file.print(message)) {
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
  }
  file.close();
}

void appendFile(fs::FS &fs, const char *path, const char *message) {
  Serial.printf("Appending to file: %s\n", path);

  fs::File file = fs.open(path, FILE_APPEND);
  if (!file) {
    Serial.println("Failed to open file for appending");
    return;
  }
  if (file.print(message)) {
    Serial.println("Message appended");
  } else {
    Serial.println("Append failed");
  }
  file.close();
}

void renameFile(fs::FS &fs, const char *path1, const char *path2) {
  Serial.printf("Renaming file %s to %s\n", path1, path2);
  if (fs.rename(path1, path2)) {
    Serial.println("File renamed");
  } else {
    Serial.println("Rename failed");
  }
}

void deleteFile(fs::FS &fs, const char *path) {
  Serial.printf("Deleting file: %s\n", path);
  if (fs.remove(path)) {
    Serial.println("File deleted");
  } else {
    Serial.println("Delete failed");
  }
}

