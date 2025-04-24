#include "utilities.h"

#define FORMAT_LITTLEFS_IF_FAILED true

struct golioth_client *client;

bool wifiOn = false;

u8_t attendanceFileNum = 0;

long timezone = 1;
byte daysavetime = 1;

TFT_eSPI tft = TFT_eSPI();

char charBuffer[CHAR_LEN + 1] = "";
uint8_t charIndex = 0;

uint8_t onScreen; // keeps track of what is displayed on screen
extern bool keyboardOnScreen;
extern TFT_eSPI_Button keyboardKeys[40];    //keyboard buttons 
extern TFT_eSPI_Button mainMenuKeys[5]; //main menu buttons
extern TFT_eSPI_Button studentClasses[8];
extern TFT_eSPI_Button BackButton;
extern TFT_eSPI_Button WifiOnOffButton;
extern TFT_eSPI_Button WifiSsidField;
extern TFT_eSPI_Button PasswordField;
extern TFT_eSPI_Button TakeAttendanceButton;
extern char keyboardKeyLabels[40];

TaskHandle_t DisplayTaskHandler = NULL;
TaskHandle_t NetworkTaskHandler = NULL;

void Display_Task(void *arg);
void Network_Task(void *arg);

HardwareSerial fingerprintSerial(2);

Adafruit_Fingerprint finger = Adafruit_Fingerprint(&fingerprintSerial);


/********************************************************************************************************************************************
                                                      Main Function              
*********************************************************************************************************************************************/  
extern "C" void app_main()
{
  Serial.begin(115200);
  vTaskDelay(20/portTICK_PERIOD_MS);
  fingerprintSerial.begin(115200, SERIAL_8N1, 16, 17);
  vTaskDelay(20/portTICK_PERIOD_MS);
  finger.begin(57600);
  vTaskDelay(20/portTICK_PERIOD_MS);

  while(!finger.verifyPassword()){
    Serial.println("Did not find fingerprint sensor :(");
    vTaskDelay(4000 / portTICK_PERIOD_MS); 
  }
  Serial.println("Found fingerprint sensor!");
  Serial.println(F("Reading sensor parameters"));
  finger.getParameters();
  Serial.print(F("Status: 0x")); Serial.println(finger.status_reg, HEX);
  Serial.print(F("Sys ID: 0x")); Serial.println(finger.system_id, HEX);
  Serial.print(F("Capacity: ")); Serial.println(finger.capacity);
  Serial.print(F("Security level: ")); Serial.println(finger.security_level);
  Serial.print(F("Device address: ")); Serial.println(finger.device_addr, HEX);
  Serial.print(F("Packet len: ")); Serial.println(finger.packet_len);
  Serial.print(F("Baud rate: ")); Serial.println(finger.baud_rate);

  //finger.getTemplateCount();

  if (!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)) {
    Serial.println("LittleFS Mount Failed");
    return;
  }
  Serial.println("List of files in root directory");
  attendanceFileNum = listDir(LittleFS, "/", 1);

  xTaskCreate(Network_Task, "Network", 4096, NULL, 4, &NetworkTaskHandler);
  vTaskDelay(500 / portTICK_PERIOD_MS);
  xTaskCreate(Display_Task, "Display", 4096, NULL, 3, &DisplayTaskHandler);
}

/********************************************************************************************************************************************
                                                      Display Task             
*********************************************************************************************************************************************/  
void Display_Task(void *arg){
  Serial.println("starting display");
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);

  uint16_t calData[5] = { 896, 2082, 237, 3618, 7 };
  tft.setTouch(calData);

  drawMainmenu();

  static int xwidth = 0;
  while(1){
    uint16_t t_x = 0, t_y = 0; // To store the touch coordinates
    static uint16_t box_x = 0, box_y = 0; // To store the coordinates of the box where text can be entered  
    u8_t box = NONE; //To keep track of the last textbox clicked by the user
    bool pressed = tft.getTouch(&t_x, &t_y); // Pressed will be set true is there is a valid touch on the screen

    vTaskDelay(100 / portTICK_PERIOD_MS); // UI debouncing

    if(!pressed) continue;
    switch (onScreen){
/**************************************************************************************/
      printf("User pressed :  %i , %i \n", t_x, t_y);
      case MAINMENU:
        //Checking if any key coordinate boxes contain the touch coordinates
        for (uint8_t i = 0; i < 5; i++) {
          if (pressed && mainMenuKeys[i].contains(t_x, t_y)) {
            mainMenuKeys[i].press(true);  // tell the button it is pressed
          } else {
            mainMenuKeys[i].press(false);  // tell the button it is NOT pressed
          }
        }
        // Checking if any key has changed state
        for (uint8_t i = 0; i < 5; i++) {
          if (mainMenuKeys[i].justReleased()) mainMenuKeys[i].drawButton();     // draw normal

          if (mainMenuKeys[i].justPressed()) {
            mainMenuKeys[i].drawButton(true);  // draw invert

            // Del button, so delete last char
            switch(i) {
              case 0:
                drawWifiMenu();
                break;
              case 1:
                drawRegisterAttendanceMenu();
                break;
              case 2:
                drawAddNewStudentMenu();
                break;
              case 3:
                drawDeleteStudentEntry();
                break;
              case 4:
                drawSyncWithServer();
                break;
              default:
                Serial.println("Error in Main Menu buttons");
            }
          }
        }
        break;

/**************************************************************************************/
      case WIFI_MENU:
        if (pressed && WifiOnOffButton.contains(t_x, t_y)) {WifiOnOffButton.press(true);} 
        else {WifiOnOffButton.press(false);}
      
        if (pressed && WifiSsidField.contains(t_x, t_y)) {WifiSsidField.press(true);} 
        else { WifiSsidField.press(false);}

        if (pressed && PasswordField.contains(t_x, t_y)) {PasswordField.press(true);}
        else {PasswordField.press(false);}      
        

        if (WifiOnOffButton.justPressed()) {
          if(!wifiOn){
            for (u8_t i = 1; i <= 7; i++){
              tft.fillRoundRect(104, 86, 45, 24, 12, TFT_DARKGREY - ((0x780F/7)*i));
              tft.drawRoundRect(104, 86, 45, 24, 12, TFT_BLACK);
              tft.fillCircle(115 + ((21/7)*i), 97, 8, TFT_WHITE);
              vTaskDelay(20 / portTICK_PERIOD_MS);
            }
            esp_wifi_start();
            golioth_client_start(client);
            vTaskResume(NetworkTaskHandler);
          } 
          else {
            for (u8_t i = 1; i <= 7; i++){
              tft.fillRoundRect(104, 86, 45, 24, 12, TFT_DARKGREEN + ((0x780F/7)*i));
              tft.drawRoundRect(104, 86, 45, 24, 12, TFT_BLACK);
              tft.fillCircle(136 - ((21/7)*i), 97, 8, TFT_WHITE);
              vTaskDelay(20 / portTICK_PERIOD_MS);
            }
            golioth_client_stop(client); 
            vTaskSuspend(NetworkTaskHandler);
            esp_wifi_disconnect();
          }  
        }

        if(WifiSsidField.justPressed()) {
          charIndex = 0;
          charBuffer[charIndex] = 0;
          box_x = (WifiSsidField.topleftcorner() >> 16); 
          box_y = (WifiSsidField.topleftcorner());
          tft.fillRect(box_x + 4, box_y + 7, 164, 18, 0x3186);
          printf("Pressed ssid field\n"); printf("%i, %i\n", box_x, box_y);
          if(!keyboardOnScreen)drawKeyboard(); 
        }

        if(PasswordField.justPressed()) {
          charIndex = 0;
          charBuffer[charIndex] = 0;
          box_x = (PasswordField.topleftcorner() >> 16); 
          box_y = ((PasswordField.topleftcorner() << 16) >> 16);
          tft.fillRect(box_x + 4, box_y + 7, 164, 18, 0x3186);
          printf("Pressed password field\n"); printf("%i, %i\n", box_x, box_y);
          if(!keyboardOnScreen)drawKeyboard();
        }
        break;

/**************************************************************************************/
      case REGISTER_ATTENDANCE:
        for (uint8_t i = 0; i < attendanceFileNum; i++) {
          if (pressed && studentClasses[i].contains(t_x, t_y)) {
            studentClasses[i].press(true);
          } else {
            studentClasses[i].press(false);
          }
        }
        // Checking if any key has changed state
        for (uint8_t i = 0; i < attendanceFileNum; i++) {

          if (studentClasses[i].justPressed()) {
            studentClasses[i].drawButton(true);  // draw invert

            //Load student data from file clicked into json object and then into fingerprint sensor
            //display info on screen that taking attendance is in progress add exit button
            //prompt the user to touch the fingerprint sensor 
            //when they do display their name and student number with the words attendance confirmed
            //do again until 
          }
          else{
            studentClasses[i].drawButton(false);  // draw normal
          }
          tft.drawLine(45, 97 + i * 40, 147, 97 + i * 40, TFT_WHITE);
        }
        break;
      /**************************************************************************************/
      case ADD_NEW_STUDENT:
      
        break;
      /**************************************************************************************/
      case DELETE_STUDENT_ENTRY:
      
        break;
      /**************************************************************************************/
      case SYNC_WITH_SERVER:
      
        break;
      default:
        printf("Error! Unrecorgnised Main Menu key");
        break;
    }

    /**************************************************************************************/
    if (keyboardOnScreen == true){
      for (uint8_t i = 0; i < 40; i++) {
        if (pressed && keyboardKeys[i].contains(t_x, t_y)) {
          keyboardKeys[i].press(true);  // tell the button it is pressed
        } else {
          keyboardKeys[i].press(false);  // tell the button it is NOT pressed
        }
      }
  
      // Checking if any key has changed state
      for (uint8_t i = 0; i < 40; i++) {
        
        if (keyboardKeys[i].justReleased()) keyboardKeys[i].drawButton();     // draw normal
  
        if (keyboardKeys[i].justPressed()) {
          keyboardKeys[i].drawButton(true);  // draw invert
  
          // if a numberpad button, append the relevant # to the charBuffer
          if (i != 29 && i != 39 ) {
            if (charIndex < CHAR_LEN) {
              charBuffer[charIndex] = keyboardKeyLabels[i];
              charIndex++;
              charBuffer[charIndex] = 0; // zero terminate
            }
          }
  
          // Del button, so delete last char
          if (i == 39) {
            charBuffer[charIndex] = 0;
            if (charIndex > 0) {
              charIndex--;
              charBuffer[charIndex] = 0;//' ';
            }
          }
  
          if (i == 29) {
            tft.fillRect(0, 0, 65, 32, TFT_BLACK);
            tft.fillRect(0, 300, 320, 180, TFT_BLACK);
            keyboardOnScreen = false;
            tft.setFreeFont(&FreeSans9pt7b);
            BackButton.initButton(&tft, 275, 455, 60, 30, TFT_DARKGREY, 0xf9c7, TFT_WHITE, "Back", KEY_TEXTSIZE);
            BackButton.drawButton();
          }
  
          // Update the text display field being written to
          tft.setTextColor(TFT_WHITE);    
          tft.setFreeFont(&FreeSans9pt7b);
          tft.textbgcolor = 0x3186;

          // Draw the string, the value returned is the width in pixels
          tft.fillRect(box_x + 4, box_y + 7, xwidth, 18, 0x3186);
          xwidth = tft.drawString(charBuffer, box_x + 4, box_y + 7);
        }
      }
    }

    /**************************************************************************************/
    if (pressed && BackButton.contains(t_x, t_y)) {
      BackButton.press(true);  // tell the button it is pressed
    } 
    else {
      BackButton.press(false);  // tell the button it is NOT pressed
    }

    if (BackButton.justReleased()) BackButton.drawButton();     // draw normal

    else if (BackButton.justPressed()) {
      keyboardOnScreen = false;
      BackButton.drawButton(true);
      drawMainmenu();
      vTaskDelay(50 / portTICK_PERIOD_MS);
    }
  }
}

/********************************************************************************************************************************************
                                                      Network Task             
*********************************************************************************************************************************************/  
void Network_Task(void *arg){
  char *TAG_W = "wifi station";
  //Initialize NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);
  ESP_LOGI(TAG_W, "ESP_WIFI_MODE_STA");
  wifi_init_sta();

  extern char* psk_id;
  extern char* psk;

  const struct golioth_client_config config = {
    .credentials = {
      .auth_type = GOLIOTH_TLS_AUTH_TYPE_PSK,
      .psk = {
        .psk_id = psk_id,
        .psk_id_len = strlen(psk_id),
        .psk = psk,
        .psk_len = strlen(psk),
      }
    }
  };

  client = golioth_client_create(&config);
  assert(client);
  /*
  configTime(3600 * timezone, daysavetime * 3600, "time.nist.gov", "0.pool.ntp.org", "1.pool.ntp.org");
  struct tm tmstruct;
  tmstruct.tm_year = 0;
  getLocalTime(&tmstruct, 3000);
  Serial.printf(
    "\nNow is : %d-%02d-%02d %02d:%02d:%02d\n", (tmstruct.tm_year) + 1900, (tmstruct.tm_mon) + 1, tmstruct.tm_mday, tmstruct.tm_hour + 1, tmstruct.tm_min,
    tmstruct.tm_sec
  );
  Serial.println("");*/
  golioth_client_stop(client);
  vTaskSuspend(NULL);
  uint16_t counter = 0;
  while(1) {
    //GLTH_LOGI(TAG_W, "Hello, Golioth! #%d", counter);
    printf("Golioth counter: %i\n", counter);
    ++counter;
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }  
}

 /*
            EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
              WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
              pdFALSE,
              pdFALSE,
              portMAX_DELAY);
    
            if (bits & WIFI_CONNECTED_BIT) {
              ESP_LOGI(TAG_W, "connected to ap SSID:%s password:%s",
                        wifiSsid, wifiPassword);
              
            } else if (bits & WIFI_FAIL_BIT) {
                ESP_LOGI(TAG_W, "Failed to connect to SSID:%s, password:%s",
                          wifiSsid, wifiPassword);
            } else {
                ESP_LOGE(TAG_W, "UNEXPECTED EVENT");
            }
            */