#include "utilities.h"
extern TFT_eSPI tft;
extern uint8_t onScreen;
extern struct golioth_client *client;
extern TaskHandle_t NetworkTaskHandler;
extern TaskHandle_t DisplayTaskHandler;
extern u8_t loadedFile;
/********************************************************************************************************************************************
                                                         WIFI FUNCTIONS               
*********************************************************************************************************************************************/

extern bool wifiOn;
extern bool displayTaskIsSuspended;

extern char wifiSsid[32];
extern char wifiPassword[64];

extern char *TAG_W;
extern char *TAG_D;

static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data){
  
  while (!displayTaskIsSuspended) { vTaskDelay(2 / portTICK_PERIOD_MS); } // block until displayTask calls vTaskDelay, "to UI debounce", and gets suspended
  
  vTaskSuspend(DisplayTaskHandler);
  ESP_LOGI(TAG_W, "Suspended display task");
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    ESP_LOGI(TAG_W, "Attempting to connect to wifi network");
    esp_wifi_connect(); 
  } 

  else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) { 
    
    ESP_LOGI(TAG_W, "Wifi has disconnected");
    wifiOn = false;
  
    if (onScreen == WIFI_MENU){
      for (u8_t i = 1; i <= 7; i++){
        tft.fillRoundRect(104, 86, 45, 24, 12, TFT_DARKGREEN + ((0x780F/7)*i));
        tft.drawRoundRect(104, 86, 45, 24, 12, TFT_BLACK);
        tft.fillCircle(136 - ((21/7)*i), 97, 8, TFT_WHITE);
        vTaskDelay(20 / portTICK_PERIOD_MS);
      }
    }
    vTaskSuspend(NetworkTaskHandler);
    golioth_client_stop(client); 
    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_LOGI(TAG_W, "Wifi has stopped");

    if (onScreen == WIFI_MENU){
      tft.textcolor = TFT_RED;
      tft.setFreeFont(&FreeSans9pt7b);
      tft.fillRect(110, 130, 180, 22, TFT_DARKERGREY);
      tft.textbgcolor = TFT_DARKERGREY;
      tft.drawString("DISCONNECTED", 110, 130);
    }
  } 
  
  else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ESP_LOGI(TAG_W, "Connected to Wifi network");
    ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
    ESP_LOGI(TAG_W, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));

    wifiOn = true;
    golioth_client_start(client);
    vTaskResume(NetworkTaskHandler);

    if (onScreen == WIFI_MENU){
      tft.textcolor = TFT_GREEN;
      tft.setFreeFont(&FreeSans9pt7b);
      tft.fillRect(110, 130, 180, 22, TFT_DARKERGREY);
      tft.textbgcolor = TFT_DARKERGREY;
      tft.drawString("CONNECTED", 115, 130);
    }
  }
  vTaskResume(DisplayTaskHandler);
  ESP_LOGI(TAG_W, "Resumed display task");
}

extern "C" int setWifiCredentials(){
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
  
  strncpy((char*)((&wifi_config)->sta.ssid), wifiSsid, 32);
  strncpy((char*)((&wifi_config)->sta.password), wifiPassword, 64);

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
  esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
  ESP_LOGI(TAG_W, "return value of esp_wifi_set_config is %i", err);

  if (err == ESP_ERR_WIFI_PASSWORD || err == ESP_ERR_INVALID_ARG){
    tft.fillRect(25, 245, 265, 45, TFT_DARKERGREY);
    tft.textcolor = TFT_GREEN;
    tft.textbgcolor = TFT_DARKERGREY;
    tft.setFreeFont(&FreeSerifItalic9pt7b);
    tft.drawString("#invalid wifi ssid or password", 30, 250);
    return 1;
  }
  ESP_LOGI(TAG_W, "wifi ssid is %s", wifi_config.sta.ssid);
  ESP_LOGI(TAG_W, "wifi password is %s", wifi_config.sta.password);
  return 0;
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
  ESP_LOGI(TAG_W, "wifi_init_sta finished.\n");
}

/********************************************************************************************************************************************
                                                         DISPLAY FUNCTIONS               
*********************************************************************************************************************************************/  

extern u8_t attendanceFileNum;
extern u8_t studentlistNum;
extern bool keyboardOnScreen;
extern bool wifiOn;

TFT_eSPI_Button keyboardKeys[40];    //keyboard buttons 
TFT_eSPI_Button mainMenuKeys[5]; //main menu buttons
TFT_eSPI_Button studentClasses[8];
TFT_eSPI_Button attendanceFiles[8];
TFT_eSPI_Button BackButton;
TFT_eSPI_Button PasswordField;
TFT_eSPI_Button WifiSsidField;
TFT_eSPI_Button WifiOnOffButton;
TFT_eSPI_Button TakeAttendanceButton;
TFT_eSPI_Button LoadClassButton;
TFT_eSPI_Button FinishButton;
TFT_eSPI_Button SendFileButton;
TFT_eSPI_Button DeleteFileButton;
TFT_eSPI_Button SurnameField;
TFT_eSPI_Button FirstNameField;
TFT_eSPI_Button StdNoField;
TFT_eSPI_Button EnterFingerprintButton;
TFT_eSPI_Button AddStudentButton;
TFT_eSPI_Button AddFileButton;


char keyboardKeyLabels[40] = {'1', '2', '3', '4', '5', '6', '7', '8', '9', '0', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', 
                                    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', '>', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', ' ', ' ', '<' };

char *mainMenuKeyLabels[5] = {"WiFi", "Register Attendance", "New Student/File", "Delete File", "Sync With Server"};

extern char studentClassLists[8][16];
extern char attendanceFileNames[8][16];


void drawKeyboard(){
  keyboardOnScreen = true;
  tft.fillRect(0, 300, 320, 180, TFT_DARKGREY); // Draw keyboard background

  // Draw the keys
  tft.setFreeFont(&FreeSansOblique12pt7b);
  char keyboardKeyLabel[2] = "\n";      //used to convert keyboardKeyLabels char to char * for initButton method
  for (uint8_t row = 0; row < 4; row++) {
    for (uint8_t col = 0; col < 10; col++) {

      uint8_t i = col + row * 10;
      keyboardKeys[i].setLabelDatum(-1, 2, CC_DATUM);
      keyboardKeyLabel[0] = keyboardKeyLabels[i]; 
      if((i < 29)||(i > 29 && i < 37)){          // All the number and letter keys —— draw them with the default settings
        keyboardKeys[i].initButton(&tft, KEY_X + col * (KEY_W + KEY_SPACING_X),
                        KEY_Y + row * (KEY_H + KEY_SPACING_Y), 
                        KEY_W, KEY_H, TFT_WHITE, TFT_BLUE, TFT_WHITE,
                        keyboardKeyLabel, KEY_TEXTSIZE);
      }       
      else if (i == 29){                 // The return key —— make it green
        keyboardKeys[i].initButton(&tft, KEY_X + col * (KEY_W + KEY_SPACING_X),
                        KEY_Y + row * (KEY_H + KEY_SPACING_Y), 
                        KEY_W, KEY_H, TFT_WHITE, TFT_GREEN, TFT_WHITE,
                        keyboardKeyLabel, KEY_TEXTSIZE);
      }   
      else if (i == 37){              // The space bar —— make it twice as wide
        keyboardKeys[i].initButton(&tft, KEY_X * 2 + col * (KEY_W + KEY_SPACING_X),
                        KEY_Y + row * (KEY_H + KEY_SPACING_Y), 
                        KEY_W * 2 + KEY_SPACING_X , KEY_H, TFT_WHITE, TFT_BLUE, TFT_WHITE,
                        "__", KEY_TEXTSIZE);
      }
      else if (i == 38){             // Space for the would be 38th key is taken by space bar
        continue;
      }
      else if (i == 39){             // The backspace key —— make it red
        keyboardKeys[i].initButton(&tft, KEY_X + col * (KEY_W + KEY_SPACING_X),
                        KEY_Y + row * (KEY_H + KEY_SPACING_Y), 
                        KEY_W, KEY_H, TFT_WHITE, TFT_RED, TFT_WHITE,
                        keyboardKeyLabel, KEY_TEXTSIZE);
      }
      else{
        ESP_LOGE(TAG_D, "keyboard key does not exist");
      }
      keyboardKeys[i].drawButton();
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

  tft.fillRoundRect(20, 70, 280, 225, 10, TFT_DARKERGREY);
  tft.setFreeFont(&FreeSans9pt7b);
  tft.textbgcolor = TFT_DARKERGREY;
  String WifiOptions[] = {"Wi-Fi", "Status", "Ssid", "Password"};
  for (u8_t i = 0; i < 4; i++){
    tft.drawString(WifiOptions[i], 32, 90 + 40*i);
  }
  
  tft.textcolor = TFT_GREEN;
  tft.setFreeFont(&FreeSerifItalic9pt7b);
  tft.drawString("#password must be between 8 and 15", 30, 250);
  tft.drawString("characters long", 35, 270);

  WifiOnOffButton.initButton(&tft, 126, 98, 45, 24, TFT_DARKERGREY, TFT_DARKERGREY, TFT_DARKERGREY, " ", KEY_TEXTSIZE);
  WifiOnOffButton.drawButton();
  tft.fillRoundRect(104, 86, 45, 24, 12, wifiOn? TFT_DARKGREEN : TFT_DARKGREY);
  tft.drawRoundRect(104, 86, 45, 24, 12, TFT_BLACK);
  tft.fillCircle(wifiOn? 136 : 115, 97, 8, TFT_WHITE);

  tft.textcolor = wifiOn ? TFT_GREEN : TFT_RED;
  tft.setFreeFont(&FreeSans9pt7b);
  tft.drawString(wifiOn? "CONNECTED" : "DISCONNECTED", 115, 130);

  WifiSsidField.initButton(&tft, 204, 176, 170, 30, TFT_DARKGREY, TFT_DARKERGREY, TFT_WHITE, " ", KEY_TEXTSIZE);
  WifiSsidField.drawButton();

  PasswordField.initButton(&tft, 204, 216, 170, 30, TFT_DARKGREY, TFT_DARKERGREY, TFT_WHITE, " ", KEY_TEXTSIZE);
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
  tft.drawString("TAKE ATTENDANCE", 40, 30);

  Serial.print("Number of class list files in littlefs storage: "); Serial.println(studentlistNum); 
  tft.setFreeFont(&FreeSans9pt7b);

  tft.fillRoundRect(35, 70, 250, 95 + 32 * studentlistNum, 10, TFT_DARKERGREY);
  for (u8_t i = 0; i < studentlistNum; i++){
    Serial.print("Class file "); Serial.print(i + 1); Serial.print(": "); Serial.println(studentClassLists
    [i]);
    studentClasses[i].setLabelDatum(-54, -5, TL_DATUM);
    studentClasses[i].initButton(&tft, 111, 90 + i * 32, 150, 30, TFT_DARKERGREY, TFT_DARKERGREY, TFT_WHITE, studentClassLists
    [i], KEY_TEXTSIZE);
    studentClasses[i].drawButton();
  }

  for (u8_t i = 0; i < studentlistNum; i++){
    tft. drawRect(40, 85 + i * 32, 9, 9, TFT_WHITE);
    tft.drawLine(40, 105 + i * 32, 200, 105 + i * 32, TFT_WHITE);
  }

  tft.setFreeFont(&FreeSansBold9pt7b);
  LoadClassButton.setLabelDatum(0, 2, CC_DATUM);
  LoadClassButton.initButton(&tft, 160, 100 + 32 * studentlistNum, 160, 30, TFT_WHITE, TFT_DARKGREEN, TFT_WHITE, "Load Class File", KEY_TEXTSIZE);
  LoadClassButton.drawButton();

  RegAttMenuMsg(NONE);

  tft.setFreeFont(&FreeSansBold9pt7b);
  TakeAttendanceButton.setLabelDatum(0, 2, CC_DATUM);
  TakeAttendanceButton.initButton(&tft, 160, 200 + 32 * studentlistNum, 210, 40, TFT_WHITE, TFT_DARKGREEN, TFT_WHITE, "Take Attendance", KEY_TEXTSIZE);
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

  tft.fillRoundRect(20, 70, 280, 250 + 32 * studentlistNum, 10, TFT_DARKERGREY);
  tft.setFreeFont(&FreeSans9pt7b);
  tft.textbgcolor = TFT_DARKERGREY;
  String studentOption[] = {"Surname:", "First Name:", "Std No:", "Fingerprint"};
  for (u8_t i = 0; i < 4; i++){
    tft.drawString(studentOption[i], 32, 90 + 40*i);
  }

  SurnameField.initButton(&tft, 209, 96, 165, 30, TFT_DARKGREY, TFT_DARKERGREY, TFT_WHITE, " ", KEY_TEXTSIZE);
  SurnameField.drawButton();

  FirstNameField.initButton(&tft, 209, 136, 165, 30, TFT_DARKGREY, TFT_DARKERGREY, TFT_WHITE, " ", KEY_TEXTSIZE);
  FirstNameField.drawButton();

  StdNoField.initButton(&tft, 209, 176, 165, 30, TFT_DARKGREY, TFT_DARKERGREY, TFT_WHITE, " ", KEY_TEXTSIZE);
  StdNoField.drawButton();

  tft.setFreeFont(&FreeSansBold9pt7b);
  EnterFingerprintButton.setLabelDatum(0, 2, CC_DATUM);
  EnterFingerprintButton.initButton(&tft, 210, 216, 160, 30, TFT_WHITE, TFT_DARKGREEN, TFT_WHITE, "Enter Fingerprint", KEY_TEXTSIZE);
  EnterFingerprintButton.drawButton();

  tft.setFreeFont(&FreeSans9pt7b);
  for (u8_t i = 0; i < studentlistNum; i++){
    Serial.print("Class file "); Serial.print(i + 1); Serial.print(": "); Serial.println(studentClassLists[i]);
    studentClasses[i].setLabelDatum(-54, -5, TL_DATUM);
    studentClasses[i].initButton(&tft, 111, 260 + i * 32, 150, 30, TFT_DARKERGREY, TFT_DARKERGREY, TFT_WHITE, studentClassLists[i], KEY_TEXTSIZE);
    studentClasses[i].drawButton();
  }

  for (u8_t i = 0; i < studentlistNum; i++){
    tft. drawRect(40, 255 + i * 32, 9, 9, TFT_WHITE);
    tft.drawLine(40, 275 + i * 32, 200, 275 + i * 32, TFT_WHITE);
  }

  tft.textcolor = TFT_GREEN;
  tft.setFreeFont(&FreeSerifItalic9pt7b);
  tft.drawString("#Select file to add student to ", 30, 255 + 32 * studentlistNum);

  tft.setFreeFont(&FreeSansBold9pt7b);
  AddStudentButton.setLabelDatum(0, 2, CC_DATUM);
  AddStudentButton.initButton(&tft, 160, 294 + 32 * studentlistNum, 160, 30, TFT_WHITE, TFT_DARKGREEN, TFT_WHITE, "Add Student", KEY_TEXTSIZE);
  AddStudentButton.drawButton();

  tft.setFreeFont(&FreeSansBold9pt7b);
  AddFileButton.setLabelDatum(-30, 2, CC_DATUM);
  AddFileButton.initButton(&tft, 95, 455, 160, 30, TFT_WHITE, TFT_DARKGREEN, TFT_WHITE, "Add File", KEY_TEXTSIZE);
  AddFileButton.drawButton();

  tft.setFreeFont(&FreeSans9pt7b);
  BackButton.initButton(&tft, 275, 455, 60, 30, TFT_DARKGREY, 0xf9c7, TFT_WHITE, "Back", KEY_TEXTSIZE);
  BackButton.drawButton();


  //drawKeyboard();
}

void drawDeleteStudentEntry(){
  onScreen = DELETE_FILE;
  tft.fillScreen((TFT_BLACK));
  tft.textcolor = TFT_WHITE;
  tft.textbgcolor = TFT_BLACK;
  tft.setFreeFont(MAINMENU_FONT);
  tft.drawString("DELETE FILE", 90, 30);

  tft.setFreeFont(&FreeSans9pt7b);

  tft.fillRoundRect(35, 70, 250, 45 + 32 * (attendanceFileNum + studentlistNum), 10, TFT_DARKERGREY);
  for (u8_t i = 0; i < attendanceFileNum; i++){
    attendanceFiles[i].setLabelDatum(-54, -5, TL_DATUM);
    attendanceFiles[i].initButton(&tft, 111, 90 + i * 32, 150, 30, TFT_DARKERGREY, TFT_DARKERGREY, TFT_WHITE, attendanceFileNames[i], KEY_TEXTSIZE);
    attendanceFiles[i].drawButton();
  }
  for (u8_t i = 0; i < studentlistNum; i++){
    studentClasses[i].setLabelDatum(-54, -5, TL_DATUM);
    studentClasses[i].initButton(&tft, 111, 90 + (i + attendanceFileNum) * 32, 150, 30, TFT_DARKERGREY, TFT_DARKERGREY, TFT_WHITE, studentClassLists[i], KEY_TEXTSIZE);
    studentClasses[i].drawButton();
  }

  for (u8_t i = 0; i < (attendanceFileNum + studentlistNum); i++){
    tft. drawRect(40, 85 + i * 32, 9, 9, TFT_WHITE);
    tft.drawLine(40, 105 + i * 32, 200, 105 + i * 32, TFT_WHITE);
  }

  tft.textcolor = TFT_GREEN;
  tft.textbgcolor = TFT_DARKERGREY;
  tft.setFreeFont(&FreeSerifItalic9pt7b);
  tft.drawString("#Select the file you want to delete", 43, 94 + 32 * (attendanceFileNum + studentlistNum));

  tft.setFreeFont(&FreeSansBold9pt7b);
  DeleteFileButton.setLabelDatum(0, 2, CC_DATUM);
  DeleteFileButton.initButton(&tft, 160, 150 + 32 * (attendanceFileNum + studentlistNum), 210, 40, TFT_WHITE, TFT_DARKGREEN, TFT_WHITE, "Delete File", KEY_TEXTSIZE);
  DeleteFileButton.drawButton();

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

  Serial.print("Number of attendance data files in littlefs storage: "); Serial.println(attendanceFileNum); 
  tft.setFreeFont(&FreeSans9pt7b);

  tft.fillRoundRect(35, 70, 250, 65 + 32 * attendanceFileNum, 10, TFT_DARKERGREY);
  for (u8_t i = 0; i < attendanceFileNum; i++){
    Serial.print("Attendance file "); Serial.print(i + 1); Serial.print(": "); Serial.println(attendanceFileNames[i]);
    attendanceFiles[i].setLabelDatum(-54, -5, TL_DATUM);
    attendanceFiles[i].initButton(&tft, 111, 90 + i * 32, 150, 30, TFT_DARKERGREY, TFT_DARKERGREY, TFT_WHITE, attendanceFileNames[i], KEY_TEXTSIZE);
    attendanceFiles[i].drawButton();
  }

  for (u8_t i = 0; i < attendanceFileNum; i++){
    tft. drawRect(40, 85 + i * 32, 9, 9, TFT_WHITE);
    tft.drawLine(40, 105 + i * 32, 200, 105 + i * 32, TFT_WHITE);
  }

  tft.textcolor = TFT_GREEN;
  tft.textbgcolor = TFT_DARKERGREY;
  tft.setFreeFont(&FreeSerifItalic9pt7b);
  tft.drawString("#Select an attendance file to be", 43, 94 + 32 * attendanceFileNum);
  tft.drawString("sent to the server", 43, 114 + 32 * attendanceFileNum);

  tft.setFreeFont(&FreeSansBold9pt7b);
  SendFileButton.setLabelDatum(0, 2, CC_DATUM);
  SendFileButton.initButton(&tft, 160, 170 + 32 * attendanceFileNum, 210, 40, TFT_WHITE, TFT_DARKGREEN, TFT_WHITE, "Send File", KEY_TEXTSIZE);
  SendFileButton.drawButton();

  tft.setFreeFont(&FreeSans9pt7b);
  BackButton.initButton(&tft, 275, 455, 60, 30, TFT_DARKGREY, 0xf9c7, TFT_WHITE, "Back", KEY_TEXTSIZE);
  BackButton.drawButton();
}

void drawAttendanceModeMenu(){
  onScreen = ATTENDANCE_MODE;
  tft.fillScreen(TFT_BLACK);
  tft.textcolor = TFT_WHITE;
  tft.textbgcolor = TFT_BLACK;
  tft.setFreeFont(MAINMENU_FONT);
  tft.drawString("ATTENDANCE MODE", 40, 30);
  tft.fillRoundRect(20, 70, 280, 225, 10, TFT_DARKERGREY);

  tft.setFreeFont(&FreeSans9pt7b);
  tft.textbgcolor = TFT_DARKERGREY;
  tft.drawString("Place fingerprint on sensor...", 40, 90 );
  
  tft.setFreeFont(&FreeSans9pt7b);
  FinishButton.initButton(&tft, 160, 455, 100, 30, TFT_DARKGREY, 0xf9c7, TFT_WHITE, "Finish", KEY_TEXTSIZE);
  FinishButton.drawButton();
}

void RegAttMenuMsg(u8_t activefile){
  tft.textcolor = TFT_GREEN;
  tft.textbgcolor = TFT_DARKERGREY;
  tft.setFreeFont(&FreeSerifItalic9pt7b);
  tft.fillRect(41, 122 + 32 * studentlistNum, 240, 40, TFT_DARKERGREY);
  if(activefile == NONE){
    tft.drawString("#Select a students list to be used", 43, 124 + 32 * studentlistNum);
    tft.drawString("to register attendance", 43, 144 + 32 * studentlistNum);
  }
  else{
    int textLength = tft.drawString(studentClassLists[activefile], 41, 124 + 32 * studentlistNum);
    tft.drawString(" has been loaded", 41 + textLength, 124 + 32 * studentlistNum);
  }
}

/********************************************************************************************************************************************
                                                      FINGERPRINT SCANNER FUNCTIONS               
*********************************************************************************************************************************************/  
extern Adafruit_Fingerprint finger;
extern HardwareSerial fingerprintSerial;
uint8_t f_buf[512];

void enroll_storeTemplateToBuf(int id){

  Serial.println("Waiting for valid finger....");
  while (finger.getImage() != FINGERPRINT_OK) { // press down a finger take 1st image 
    vTaskDelay(200 / portTICK_PERIOD_MS);
  }
  Serial.println("Image taken");

  if (finger.image2Tz(1) == FINGERPRINT_OK) { //creating the charecter file for 1st image 
    Serial.println("Image converted");
  } else {
    Serial.println("Conversion error");
    return;
  }

  Serial.println("Remove finger");
  vTaskDelay(2000 / portTICK_PERIOD_MS);
  uint8_t p = 0;
  while (p != FINGERPRINT_NOFINGER) {
    p = finger.getImage();
    vTaskDelay(200 / portTICK_PERIOD_MS);
  }

  Serial.println("Place same finger again, waiting....");
  while (finger.getImage() != FINGERPRINT_OK) { // press the same finger again to take 2nd image
    vTaskDelay(200 / portTICK_PERIOD_MS);
  }
  Serial.println("Image taken");


  if (finger.image2Tz(2) == FINGERPRINT_OK) { //creating the charecter file for 2nd image 
    Serial.println("Image converted");
  } else {
    Serial.println("Conversion error");
    return;
  }


  Serial.println("Creating model...");

  if (finger.createModel() == FINGERPRINT_OK) {  //creating the template from the 2 charecter files and saving it to char buffer 1
    Serial.println("Prints matched!");
    Serial.println("Template created");
  } else {
    Serial.println("Template not build");
    return;
  }

  Serial.println("Attempting to get template..."); 
  if (finger.getModel() == FINGERPRINT_OK) {  //requesting sensor to transfer the template data to upper computer (this microcontroller)
    Serial.println("Transferring Template...."); 
  } else {
    Serial.println("Failed to transfer template");
    return;
  }
  
  if (finger.get_template_buffer(512, f_buf) == FINGERPRINT_OK) { //read the template data from sensor and save it to buffer f_buf
    Serial.println("Template data (comma sperated HEX):");
    for (int k = 0; k < (512/finger.packet_len); k++) { //printing out the template data in seperate rows, where row-length = packet_length
      for (int l = 0; l < finger.packet_len; l++) {
        Serial.print("0x");
        Serial.print(f_buf[(k * finger.packet_len) + l], HEX);
        Serial.print(",");
      }
      Serial.println("");
    }
  }
  Serial.print("ID "); Serial.println(id);
  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK) {
    Serial.println("Stored!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    return;
  } else if (p == FINGERPRINT_BADLOCATION) {
    Serial.println("Could not store in that location");
    return;
  } else if (p == FINGERPRINT_FLASHERR) {
    Serial.println("Error writing to flash");
    return;
  } else {
    Serial.println("Unknown error");
    return;
  }
}

void writeTemplateDataToSensor(u8_t id, u8_t *fingerprint_template) {
  int template_buf_size=512; //usually hobby grade sensors have 512 byte template data, watch datasheet to know the info
  
  Serial.println("Ready to write template to sensor...");

  if (id == 0) {// ID #0 not allowed, try again!
    return;
  }
  Serial.print("Writing template against ID #"); Serial.println(id);

  if (finger.write_template_to_sensor(template_buf_size, fingerprint_template)) { //telling the sensor to download the template data to it's char buffer from upper computer (this microcontroller's "fingerTemplate" buffer)
    Serial.println("now writing to sensor...");
  } else {
    Serial.println("writing to sensor failed");
    return;
  }

  Serial.print("ID "); Serial.println(id);
  if (finger.storeModel(id) == FINGERPRINT_OK) { //saving the template against the ID you entered or manually set
    Serial.print("Successfully stored against ID#");Serial.println(id);
  } else {
    Serial.println("Storing error");
    return ;
  }
} 

uint8_t getFingerprintID() {
  uint8_t p = finger.getImage();
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image taken");
      break;
    case FINGERPRINT_NOFINGER:
      Serial.println("No finger detected");
      return 255;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return 255;
    case FINGERPRINT_IMAGEFAIL:
      Serial.println("Imaging error");

      tft.fillRoundRect(20, 70, 280, 225, 10, TFT_DARKERGREY);
      tft.setFreeFont(&FreeSans9pt7b);
      tft.textbgcolor = TFT_DARKERGREY;
      tft.drawString("Place fingerprint on sensor...", 40, 90 );
      tft.drawString("Imaging error", 40, 120 );

      return 255;
    default:
      Serial.println("Unknown error");
      return 255;
  }

  // OK success!

  p = finger.image2Tz();
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");

      tft.fillRoundRect(20, 70, 280, 225, 10, TFT_DARKERGREY);
      tft.setFreeFont(&FreeSans9pt7b);
      tft.textbgcolor = TFT_DARKERGREY;
      tft.drawString("Place fingerprint on sensor...", 40, 90 );
      tft.drawString("Image too messy", 40, 120 );

      return 255;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return 255;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");

      tft.fillRoundRect(20, 70, 280, 225, 10, TFT_DARKERGREY);
      tft.setFreeFont(&FreeSans9pt7b);
      tft.textbgcolor = TFT_DARKERGREY;
      tft.drawString("Place fingerprint on sensor...", 40, 90 );
      tft.drawString("Could not find fingerprint features", 40, 120 );

      return 255;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");

      tft.fillRoundRect(20, 70, 280, 225, 10, TFT_DARKERGREY);
      tft.setFreeFont(&FreeSans9pt7b);
      tft.textbgcolor = TFT_DARKERGREY;
      tft.drawString("Place fingerprint on sensor...", 40, 90 );
      tft.drawString("Could not find fingerprint features", 40, 120 );

      return 255;
    default:
      Serial.println("Unknown error");
      return 255;
  }

  // OK converted!
  p = finger.fingerSearch();
  if (p == FINGERPRINT_OK) {
    Serial.println("Found a print match!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    return 255;
  } else if (p == FINGERPRINT_NOTFOUND) {
    Serial.println("Did not find a match");

    tft.fillRoundRect(20, 70, 280, 225, 10, TFT_DARKERGREY);
    tft.setFreeFont(&FreeSans9pt7b);
    tft.textbgcolor = TFT_DARKERGREY;
    tft.drawString("Place fingerprint on sensor...", 40, 90 );
    tft.drawString("Did not find a match", 40, 120 );

    return 255;
  } else {
    Serial.println("Unknown error");
    return 255;
  }

  // found a match!
  Serial.print("Found ID #"); Serial.println(finger.fingerID);
  Serial.print(" with confidence of "); Serial.println(finger.confidence);

  return finger.fingerID;
}


/********************************************************************************************************************************************
                                                        FILE MANAGEMENT FUNCTIONS               
*********************************************************************************************************************************************/ 
extern JsonDocument student;
extern JsonDocument attendance_doc; 

u8_t listDir(fs::FS &fs, const char *dirname, uint8_t levels, char Buffer[][16]) {
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
        listDir(fs, file.path(), levels - 1, Buffer);
      }
    } 
    else {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      strncpy(Buffer[fileNum], file.name(), 15); //put names of files in the directory in the list
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

