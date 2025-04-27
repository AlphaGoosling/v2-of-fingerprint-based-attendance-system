#include "utilities.h"

#define FORMAT_LITTLEFS_IF_FAILED true

extern char *TAG_W;
extern char *TAG_D;

struct golioth_client *client;

bool wifiOn = false;
bool displayTaskIsSuspended; // Used to prevent the wifi event handler from trying to use SPI to display stuff on the screen while displayTask may be using it
extern char wifiSsid[32];
extern char wifiPassword[64];

extern char studentClassLabels[8][16];
u8_t attendanceFileNum = 0;
u8_t loadedFile = NONE;
JsonDocument attendance_doc;
const char* first_name[NUM_OF_STUDENTS];
const char* surname[NUM_OF_STUDENTS];
u32_t student_number[NUM_OF_STUDENTS];
bool attended[NUM_OF_STUDENTS];

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
extern TFT_eSPI_Button LoadClassButton;
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
  u8_t activeElement = NONE; //To keep track of the last selectable element clicked by the user
  bool wasPressed = false; // wasPressed will be set to true if the last State of the boolean "pressed" was true
  while(1){
    uint16_t t_x = 0, t_y = 0; // To store the touch coordinates
    static uint16_t box_x = 0, box_y = 0; // To store the coordinates of the text field when one is pressed 
    bool pressed = tft.getTouch(&t_x, &t_y); // Pressed will be set true if there is a valid touch on the screen

    displayTaskIsSuspended = true;         
    vTaskDelay(20 / portTICK_PERIOD_MS); // UI debouncing
    displayTaskIsSuspended = false;

    if(!pressed && !wasPressed) continue; //if display is pressed and was not previously pressed then drop the loop
    (pressed == false && wasPressed == true) ? wasPressed = false : wasPressed = true;

    switch (onScreen){
/**************************************************************************************/
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

          if (mainMenuKeys[i].justPressed()) {
            mainMenuKeys[i].drawButton(true);  

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

            if(setWifiCredentials() == 0){ // check whether the wifi credentials passed are correct
              esp_wifi_start();
            } else {
              ESP_LOGE(TAG_D, "Invalid wifi ssid or password");
              if (onScreen == WIFI_MENU){
                for (u8_t i = 1; i <= 7; i++){
                  tft.fillRoundRect(104, 86, 45, 24, 12, TFT_DARKGREEN + ((0x780F/7)*i));
                  tft.drawRoundRect(104, 86, 45, 24, 12, TFT_BLACK);
                  tft.fillCircle(136 - ((21/7)*i), 97, 8, TFT_WHITE);
                  vTaskDelay(20 / portTICK_PERIOD_MS);
                }
              }
            } 
          } 
          else {
            tft.fillRoundRect(104, 86, 45, 24, 12, TFT_WHITE);
            vTaskDelay(50 / portTICK_PERIOD_MS);
            tft.fillRoundRect(104, 86, 45, 24, 12, TFT_DARKGREEN);
            tft.drawRoundRect(104, 86, 45, 24, 12, TFT_BLACK);
            tft.fillCircle(136, 97, 8, TFT_WHITE);
            esp_wifi_disconnect();
          }  
        }

        if(WifiSsidField.justPressed()) {
          activeElement = WIFI_SSID_BOX;
          charIndex = 0;
          charBuffer[charIndex] = 0;
          box_x = (WifiSsidField.topleftcorner() >> 16); 
          box_y = (WifiSsidField.topleftcorner());
          tft.fillRect(box_x + 4, box_y + 7, 164, 18, TFT_DARKERGREY);
          printf("Pressed ssid field\n"); printf("%i, %i\n", box_x, box_y);
          if(!keyboardOnScreen)drawKeyboard(); 
        }

        if(PasswordField.justPressed()) {
          activeElement = WIFI_PASSWORD_BOX;
          charIndex = 0;
          charBuffer[charIndex] = 0;
          box_x = (PasswordField.topleftcorner() >> 16); 
          box_y = ((PasswordField.topleftcorner() << 16) >> 16);
          tft.fillRect(box_x + 4, box_y + 7, 164, 18, TFT_DARKERGREY);
          printf("Pressed password field\n"); printf("%i, %i\n", box_x, box_y);
          if(!keyboardOnScreen)drawKeyboard();
        }
        break;

/**************************************************************************************/
      case REGISTER_ATTENDANCE:
        static u8_t activeFile = (loadedFile == NONE ? NONE : activeFile);
        for (uint8_t i = 0; i < attendanceFileNum; i++) {
          if (pressed && studentClasses[i].contains(t_x, t_y)) {
            studentClasses[i].press(true);
          } else {
            studentClasses[i].press(false);
          }
        }
        if (pressed && LoadClassButton.contains(t_x, t_y)) {
            LoadClassButton.press(true);
        } else {
          LoadClassButton.press(false);
        }
        if (pressed && TakeAttendanceButton.contains(t_x, t_y)) {
          TakeAttendanceButton.press(true);
        } else {
          TakeAttendanceButton.press(false);
        }

        // Checking if any key has changed state
        for (uint8_t i = 0; i < attendanceFileNum; i++) {

          if (studentClasses[i].justPressed()) {
            tft.setFreeFont(&FreeSans9pt7b);
            studentClasses[i].drawButton(true);  // draw inverted
          }

          if (studentClasses[i].justReleased()) {
            tft.setFreeFont(&FreeSans9pt7b);
            studentClasses[i].drawButton(false);  // draw normally
            activeFile = i;
          }
          tft. drawRect(45, 85 + i * 32, 9, 9, TFT_WHITE);
          tft.drawLine(45, 105 + i * 32, 160, 105 + i * 32, TFT_WHITE);
        } 

        for (uint8_t i = 0; i < attendanceFileNum; i++) {
          (i == activeFile) ? (tft.fillRect(47, 87 + i * 32, 5, 5, TFT_WHITE)) : (tft.fillRect(47, 87 + i * 32, 5, 5, TFT_DARKERGREY)); //radiobutton
        } 

        if (LoadClassButton.justPressed()) {
          tft.setFreeFont(&FreeSansBold9pt7b);
          LoadClassButton.setLabelDatum(0, 2, CC_DATUM);
          LoadClassButton.drawButton(true); 
          if (activeFile == NONE)
          {
            for(u8_t i = 0; i < 5; i++){
              tft.fillRect(46, 122 + 32 * attendanceFileNum, 230, 40, TFT_DARKERGREY);
              vTaskDelay(40 / portTICK_PERIOD_MS);
              tft.textcolor = TFT_GREEN;
              tft.textbgcolor = TFT_DARKERGREY;
              tft.setFreeFont(&FreeSerifItalic9pt7b);
              tft.drawString("#Please select a student list to be", 48, 124 + 32 * attendanceFileNum);
              tft.drawString("used before pressing the button", 48, 144 + 32 * attendanceFileNum); 
              vTaskDelay(40 / portTICK_PERIOD_MS);
            }
          }
          
          String path = "/";
          path.concat(studentClassLabels[activeFile]);
          fs::File file = LittleFS.open(path);
          if (!file) {
            Serial.println("Failed to open file for reading");
            return;
          }
          u8_t i = 1;
          while (true) {
            JsonDocument student;
          
            DeserializationError err = deserializeJson(student, file);
            if (err) break;
          
            // the zeroth index in all the following buffers is not used for the sake of simplicity——i is the fingerprint id in all cases
            first_name[i] = student["first_name"];
            surname[i] = student["surname"];
            student_number[i] = student["student_number"];
            attended[i] = student["attended"];
            u8_t fingerprint_template[512];
            strncpy((char *)fingerprint_template, student["fingerprint_template"], 512);
            writeTemplateDataToSensor(i, fingerprint_template);

            Serial.print(first_name[i]);
            Serial.print(" ");
            Serial.println(surname[i]);
            Serial.println(" ");
            i++;
          }
          
          file.close();
          //Signal to the user that you have
        }
        if (LoadClassButton.justReleased()) {
          tft.setFreeFont(&FreeSansBold9pt7b);
          LoadClassButton.setLabelDatum(0, 2, CC_DATUM);
          LoadClassButton.drawButton(false);  
        }
        
        if (TakeAttendanceButton.justPressed()) {
          tft.setFreeFont(&FreeSansBold9pt7b);
          TakeAttendanceButton.setLabelDatum(0, 2, CC_DATUM);
          TakeAttendanceButton.drawButton(true); 
          //display info on screen that taking attendance is in progress add exit button
          //prompt the user to touch the fingerprint sensor 
          //when they do display their name and student number with the words attendance confirmed
          //do again until
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
      tft.setFreeFont(&FreeSansOblique12pt7b);
      for (uint8_t i = 0; i < 40; i++) {
        if (pressed && keyboardKeys[i].contains(t_x, t_y)) {
          keyboardKeys[i].press(true);  
        } else {
          keyboardKeys[i].press(false);  
        }
      }
  
      // Checking if any key has changed state
      for (uint8_t i = 0; i < 40; i++) {

        if (keyboardKeys[i].justReleased()) keyboardKeys[i].drawButton();    
  
        if (keyboardKeys[i].justPressed()) { 
          
          keyboardKeys[i].drawButton(true);     

          // if it is a number button or space bar, append it to the charBuffer directly
          if (i < 10 || i == 37 ) {
            if (charIndex < CHAR_LEN) {
              charBuffer[charIndex] = keyboardKeyLabels[i];
              charIndex++;
              charBuffer[charIndex] = 0; // zero terminate
            }
          }
          // if it is a alphabet letter button, change it to lowercase and then append it to the charBuffer
          else if ((i > 9 && i < 29)||(i > 29 && i < 37)) {
            if (charIndex < CHAR_LEN) {
              charBuffer[charIndex] = (keyboardKeyLabels[i] + 0x20);
              charIndex++;
              charBuffer[charIndex] = 0; // zero terminate
            }
          }

          // Del button, so delete last char
          else if (i == 39) {
            charBuffer[charIndex] = 0;
            if (charIndex > 0) {
              charIndex--;
              charBuffer[charIndex] = 0;//' ';
            }
          }
          
          // Return key / Enter key, transfer value in charBuffer to relevant Buffer
          else if (i == 29) {
            switch (activeElement)
            {
            case NONE:
              /* Do nothing */
              break;

            case WIFI_SSID_BOX:
              strncpy(wifiSsid, charBuffer, (CHAR_LEN + 1));
              break;

            case WIFI_PASSWORD_BOX:
            strncpy(wifiPassword, charBuffer, (CHAR_LEN + 1));
              break;
            
            default:
              ESP_LOGE(TAG_D, "Unrecorgnised textbox clicked in wifi menu");
              break;
            }
            tft.fillRect(0, 0, 65, 32, TFT_BLACK);
            tft.fillRect(0, 300, 320, 180, TFT_BLACK);
            keyboardOnScreen = false;
            tft.setFreeFont(&FreeSans9pt7b);
            BackButton.initButton(&tft, 275, 455, 60, 30, TFT_DARKGREY, 0xf9c7, TFT_WHITE, "Back", KEY_TEXTSIZE);
            BackButton.drawButton();
          }
          else{
            ESP_LOGE(TAG_D, "keyboard key does not exist");
          }
  
          // Update the text display field being written to
          tft.setTextColor(TFT_WHITE);    
          tft.setFreeFont(&FreeSans9pt7b);
          tft.textbgcolor = TFT_DARKERGREY;

          // Draw the string, the value returned is the width in pixels
          tft.fillRect(box_x + 4, box_y + 7, xwidth, 18, TFT_DARKERGREY);
          xwidth = tft.drawString(charBuffer, box_x + 4, box_y + 7);
        }
      }
    }

    /**************************************************************************************/
    if(onScreen != MAINMENU){
      if (pressed && BackButton.contains(t_x, t_y)) {
        BackButton.press(true);  
      } else {
        BackButton.press(false);  
      }

      if (BackButton.justReleased()) BackButton.drawButton();     

      else if (BackButton.justPressed()) {
        keyboardOnScreen = false;
        BackButton.drawButton(true);
        drawMainmenu();
        vTaskDelay(100 / portTICK_PERIOD_MS);
      }
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
