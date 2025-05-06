#include "utilities.h"

#define FORMAT_LITTLEFS_IF_FAILED true

extern char *TAG_W;
extern char *TAG_D;

struct golioth_client *client;

bool wifiOn = false;
bool displayTaskIsSuspended; // Used to prevent the wifi event handler from trying to use SPI to display stuff on the screen while displayTask may be using it
extern char wifiSsid[32];
extern char wifiPassword[64];

char studentClassLists[8][16];
char attendanceFileNames[8][16];
u8_t studentlistNum = 0;
u8_t attendanceFileNum = 0;
u8_t loadedFile = NONE;
JsonDocument attendance_doc;
char first_name[NUM_OF_STUDENTS][CHAR_LEN + 1];
char surname[NUM_OF_STUDENTS][CHAR_LEN + 1];
u32_t student_number[NUM_OF_STUDENTS];
bool attended[NUM_OF_STUDENTS];
u8_t studentIndex = 0;

struct tm tmstruct;
long timezone = 3;
byte daysavetime = 1;

TFT_eSPI tft = TFT_eSPI();

char charBuffer[CHAR_LEN + 1] = "";
uint8_t charIndex = 0;

uint8_t onScreen; // keeps track of what is displayed on screen
bool keyboardOnScreen = false;
extern TFT_eSPI_Button keyboardKeys[40];    //keyboard buttons 
extern TFT_eSPI_Button mainMenuKeys[5]; //main menu buttons
extern TFT_eSPI_Button studentClasses[8];
extern TFT_eSPI_Button attendanceFiles[8];
extern TFT_eSPI_Button BackButton;
extern TFT_eSPI_Button WifiOnOffButton;
extern TFT_eSPI_Button WifiSsidField;
extern TFT_eSPI_Button PasswordField;
extern TFT_eSPI_Button TakeAttendanceButton;
extern TFT_eSPI_Button LoadClassButton;
extern TFT_eSPI_Button FinishButton;
extern TFT_eSPI_Button SendFileButton;

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
  studentlistNum = listDir(LittleFS, "/", 0, studentClassLists);

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
        for (uint8_t i = 0; i < studentlistNum; i++) {
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
        for (uint8_t i = 0; i < studentlistNum; i++) {

          if (studentClasses[i].justPressed()) {
            tft.setFreeFont(&FreeSans9pt7b);
            studentClasses[i].drawButton(true);  // draw inverted
          }

          if (studentClasses[i].justReleased()) {
            tft.setFreeFont(&FreeSans9pt7b);
            studentClasses[i].drawButton(false);  // draw normally
            activeFile = i;
          }
          tft. drawRect(40, 85 + i * 32, 9, 9, TFT_WHITE);
          tft.drawLine(40, 105 + i * 32, 160, 105 + i * 32, TFT_WHITE);
        } 

        for (uint8_t i = 0; i < studentlistNum; i++) {
          (i == activeFile) ? (tft.fillRect(42, 87 + i * 32, 5, 5, TFT_WHITE)) : (tft.fillRect(42, 87 + i * 32, 5, 5, TFT_DARKERGREY)); //radiobutton
        } 

        if (LoadClassButton.justPressed()) {
          tft.setFreeFont(&FreeSansBold9pt7b);
          LoadClassButton.setLabelDatum(0, 2, CC_DATUM);
          LoadClassButton.drawButton(true); 
          if (activeFile == NONE){
            for(u8_t i = 0; i < 5; i++){
              tft.fillRect(41, 122 + 32 * studentlistNum, 240, 40, TFT_DARKERGREY);
              vTaskDelay(25 / portTICK_PERIOD_MS);
              tft.textcolor = TFT_RED;
              tft.textbgcolor = TFT_DARKERGREY;
              tft.setFreeFont(&FreeSerifItalic9pt7b);
              tft.drawString("#Please select a student list to be", 43, 124 + 32 * studentlistNum);
              tft.drawString("used before pressing the button", 43, 144 + 32 * studentlistNum); 
              vTaskDelay(25 / portTICK_PERIOD_MS);
            }
            continue;
          }
          finger.emptyDatabase();
          
          String path = "/";
          path.concat(studentClassLists[activeFile]);
          fs::File file = LittleFS.open(path);
          if (!file) {
            Serial.println("Failed to open file for reading");
            return;
          }
          u8_t i = 0;
          while (true) {
            JsonDocument student;
          
            DeserializationError err = deserializeJson(student, file);
            if (err) break;
          
            strncpy(first_name[i], (student["first_name"]), CHAR_LEN + 1);
            strncpy(surname[i], (student["surname"]), CHAR_LEN + 1);
            student_number[i] = student["student_number"];
            attended[i] = student["attended"];
            JsonArray fingerprint_template = student["fingerprint_template"];
            u8_t fingerprint_template_buffer[512];
            for (u16_t i = 0; i < 512; i++){
              fingerprint_template_buffer[i] = fingerprint_template[i];
            }
            
            writeTemplateDataToSensor(i+1, fingerprint_template_buffer);
            vTaskDelay(20 / portTICK_PERIOD_MS);

            Serial.print(first_name[i]);
            Serial.print(" ");
            Serial.println(surname[i]);
            Serial.println(" ");
            i++;
            studentIndex++;
          }
          
          file.close();
          loadedFile = activeFile;
          RegAttMenuMsg();   //Signal to the user that you have loaded the student data
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
          for(u8_t i = 0; i < NUM_OF_STUDENTS; i++){
            attended[i]= false;
          } 
          if (loadedFile == NONE){
            for(u8_t i = 0; i < 5; i++){
              tft.fillRect(41, 122 + 32 * studentlistNum, 240, 40, TFT_DARKERGREY);
              vTaskDelay(25 / portTICK_PERIOD_MS);
              tft.textcolor = TFT_RED;
              tft.textbgcolor = TFT_DARKERGREY;
              tft.setFreeFont(&FreeSerifItalic9pt7b);
              tft.drawString("#Please load a student list to be", 43, 124 + 32 * studentlistNum);
              tft.drawString("used, before pressing the button", 43, 144 + 32 * studentlistNum); 
              vTaskDelay(25 / portTICK_PERIOD_MS);
            }
            tft.setFreeFont(&FreeSansBold9pt7b);
            TakeAttendanceButton.setLabelDatum(0, 2, CC_DATUM);
            TakeAttendanceButton.drawButton(false);
            continue;
          }
          drawAttendanceModeMenu();
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
      {
        static u8_t selectedFile = NONE;
        for (uint8_t i = 0; i < attendanceFileNum; i++) {
          if (pressed && attendanceFiles[i].contains(t_x, t_y)) {
            attendanceFiles[i].press(true);
          } else {
            attendanceFiles[i].press(false);
          }
        }
        if (pressed && SendFileButton.contains(t_x, t_y)) {
          SendFileButton.press(true);
        } else {
          SendFileButton.press(false);
        }

        // Checking if any key has changed state
        for (uint8_t i = 0; i < attendanceFileNum; i++) {

          if (attendanceFiles[i].justPressed()) {
            tft.setFreeFont(&FreeSans9pt7b);
            attendanceFiles[i].drawButton(true);  // draw inverted
          }

          if (attendanceFiles[i].justReleased()) {
            tft.setFreeFont(&FreeSans9pt7b);
            attendanceFiles[i].drawButton(false);  // draw normally
            selectedFile = i;
          }
          tft. drawRect(40, 85 + i * 32, 9, 9, TFT_WHITE);
          tft.drawLine(40, 105 + i * 32, 160, 105 + i * 32, TFT_WHITE);
        } 

        for (uint8_t i = 0; i < attendanceFileNum; i++) {
          (i == selectedFile) ? (tft.fillRect(42, 87 + i * 32, 5, 5, TFT_WHITE)) : (tft.fillRect(42, 87 + i * 32, 5, 5, TFT_DARKERGREY)); //radiobutton
        } 
      
        if (SendFileButton.justReleased()) {
          tft.setFreeFont(&FreeSansBold9pt7b);
          SendFileButton.drawButton(false);  // draw normally
        }
        if (SendFileButton.justPressed()) {
          tft.setFreeFont(&FreeSansBold9pt7b);
          SendFileButton.drawButton(true);  // draw inverted

          if (selectedFile == NONE){
            for(u8_t i = 0; i < 5; i++){
              tft.fillRect(41, 92 + 32 * attendanceFileNum, 240, 40, TFT_DARKERGREY);
              vTaskDelay(25 / portTICK_PERIOD_MS);
              tft.textcolor = TFT_RED;
              tft.textbgcolor = TFT_DARKERGREY;
              tft.setFreeFont(&FreeSerifItalic9pt7b);
              tft.drawString("Please select an attendance file to", 43, 94 + 32 * attendanceFileNum);
              tft.drawString("be sent before pressing the button", 43, 114 + 32 * attendanceFileNum); 
              vTaskDelay(25 / portTICK_PERIOD_MS);
            }
            continue;
          }
          if (!wifiOn){
            for(u8_t i = 0; i < 5; i++){
              tft.fillRect(41, 92 + 32 * attendanceFileNum, 240, 40, TFT_DARKERGREY);
              vTaskDelay(25 / portTICK_PERIOD_MS);
              tft.textcolor = TFT_RED;
              tft.textbgcolor = TFT_DARKERGREY;
              tft.setFreeFont(&FreeSerifItalic9pt7b);
              tft.drawString("Please turn wifi On before", 43, 94 + 32 * attendanceFileNum);
              tft.drawString("attempting to send files to server", 43, 114 + 32 * attendanceFileNum);
              vTaskDelay(25 / portTICK_PERIOD_MS);
            }
            continue;
          }
          
          String path = "/sessions/";
          path.concat(attendanceFileNames[selectedFile]);
          fs::File jsonFile = LittleFS.open(path);
          if (!jsonFile) {
            Serial.println("Failed to open file for reading");
            continue;
          }
          String jsonPayload;
          while (jsonFile.available()) {
            jsonPayload.concat(jsonFile.readString());
          }
          jsonFile.close();
          Serial.print(jsonPayload);
          int err = golioth_stream_set_async(client, attendanceFileNames[selectedFile], GOLIOTH_CONTENT_TYPE_JSON, (u8_t *)jsonPayload.c_str(), jsonPayload.length(), NULL, NULL);
          Serial.print("Sending Status: ");Serial.println(err);

          tft.fillRect(41, 92 + 32 * attendanceFileNum, 240, 40, TFT_DARKERGREY);
          tft.textcolor = TFT_GREEN;
          tft.textbgcolor = TFT_DARKERGREY;
          tft.setFreeFont(&FreeSerifItalic9pt7b);
          int textLength = tft.drawString("Done. ", 43, 94 + 32 * attendanceFileNum);
          textLength += tft.drawString(attendanceFileNames[selectedFile], 43 + textLength, 94 + 32 * attendanceFileNum);
          tft.drawString(" has", 43 + textLength, 94 + 32 * attendanceFileNum);
          tft.drawString("been sent to the server", 43, 114 + 32 * attendanceFileNum);
        }
      }
        break;

      /**************************************************************************************/
      case ATTENDANCE_MODE:
        wasPressed = true;

        if (pressed && FinishButton.contains(t_x, t_y)) {
          FinishButton.press(true); 
        } else {
          FinishButton.press(false); 
        }
        
        if (FinishButton.justPressed()) {
          //FinishButton.drawButton(true);

          tft.fillRoundRect(20, 70, 280, 225, 10, TFT_DARKERGREY);
          tft.setFreeFont(&FreeSans9pt7b);
          tft.textbgcolor = TFT_DARKERGREY;
          tft.drawString("Saving...", 80, 140 );
          
          String currentTime = String(tmstruct.tm_hour, 2); (((currentTime += ':') +=  tmstruct.tm_min) += ':') += tmstruct.tm_sec;
          String currentDate = String((tmstruct.tm_year) + 1900); (((currentDate += '/') += (tmstruct.tm_mon + 1)) += '/') += tmstruct.tm_mday;
          attendance_doc["teacher's name"]  = "Dr. Mukisa Fictional";
          attendance_doc["subject"]         = "mathematics";
          attendance_doc["session_time"]    = currentTime;
          attendance_doc["session_date"]    = currentDate;

          JsonArray students = attendance_doc["students"].to<JsonArray>();
          while (studentIndex != 0){
            u8_t i = 0;
            JsonObject students_0 = students.add<JsonObject>();
            students_0["first_name"] = first_name[i];
            students_0["surname"] = surname[i];
            students_0["student_number"] = student_number[i];
            students_0["attended"] = attended[i];
            i++;

            if(i >= studentIndex) break;
            JsonObject students_1 = students.add<JsonObject>();
            students_1["first_name"] = first_name[i];
            students_1["surname"] = surname[i];
            students_1["student_number"] = student_number[i];
            students_1["attended"] = attended[i];
            i++;

            if(i >= studentIndex) break;
            JsonObject students_2 = students.add<JsonObject>();
            students_2["first_name"] = first_name[i];
            students_2["surname"] = surname[i];
            students_2["student_number"] = student_number[i];
            students_2["attended"] = attended[i];
            i++;

            if(i >= studentIndex) break;
            JsonObject students_3 = students.add<JsonObject>();
            students_3["first_name"] = first_name[i];
            students_3["surname"] = surname[i];
            students_3["student_number"] = student_number[i];
            students_3["attended"] = attended[i];
            i++;

            if(i >= studentIndex) break;
            JsonObject students_4 = students.add<JsonObject>();
            students_4["first_name"] = first_name[i];
            students_4["surname"] = surname[i];
            students_4["student_number"] = student_number[i];
            students_4["attended"] = attended[i];
            i++;

            if(i >= studentIndex) break;
            JsonObject students_5 = students.add<JsonObject>();
            students_5["first_name"] = first_name[i];
            students_5["surname"] = surname[i];
            students_5["student_number"] = student_number[i];
            students_5["attended"] = attended[i];
            i++;

            if(i >= studentIndex) break;
            JsonObject students_6 = students.add<JsonObject>();
            students_6["first_name"] = first_name[i];
            students_6["surname"] = surname[i];
            students_6["student_number"] = student_number[i];
            students_6["attended"] = attended[i];
            i++;

            if(i >= studentIndex) break;
            JsonObject students_7 = students.add<JsonObject>();
            students_7["first_name"] = first_name[i];
            students_7["surname"] = surname[i];
            students_7["student_number"] = student_number[i];
            students_7["attended"] = attended[i];
            i++;

            if(i >= studentIndex) break;
            JsonObject students_8 = students.add<JsonObject>();
            students_8["first_name"] = first_name[i];
            students_8["surname"] = surname[i];
            students_8["student_number"] = student_number[i];
            students_8["attended"] = attended[i];
            i++;

            if(i >= studentIndex) break;
            JsonObject students_9 = students.add<JsonObject>();
            students_9["first_name"] = first_name[i];
            students_9["surname"] = surname[i];
            students_9["student_number"] = student_number[i];
            students_9["attended"] = attended[i];
            i++;

            if(i >= studentIndex) break;
            JsonObject students_10 = students.add<JsonObject>();
            students_10["first_name"] = first_name[i];
            students_10["surname"] = surname[i];
            students_10["student_number"] = student_number[i];
            students_10["attended"] = attended[i];
            i++;

            if(i >= studentIndex) break;
            JsonObject students_11 = students.add<JsonObject>();
            students_11["first_name"] = first_name[i];
            students_11["surname"] = surname[i];
            students_11["student_number"] = student_number[i];
            students_11["attended"] = attended[i];
            i++;

            if(i >= studentIndex) break;
            JsonObject students_12 = students.add<JsonObject>();
            students_12["first_name"] = first_name[i];
            students_12["surname"] = surname[i];
            students_12["student_number"] = student_number[i];
            students_12["attended"] = attended[i];
            i++;

            if(i >= studentIndex) break;
            JsonObject students_13 = students.add<JsonObject>();
            students_13["first_name"] = first_name[i];
            students_13["surname"] = surname[i];
            students_13["student_number"] = student_number[i];
            students_13["attended"] = attended[i];
            i++;

            if(i >= studentIndex) break;
            JsonObject students_14 = students.add<JsonObject>();
            students_14["first_name"] = first_name[i];
            students_14["surname"] = surname[i];
            students_14["student_number"] = student_number[i];
            students_14["attended"] = attended[i];
            i++;

            if(i >= studentIndex) break;
            JsonObject students_15 = students.add<JsonObject>();
            students_15["first_name"] = first_name[i];
            students_15["surname"] = surname[i];
            students_15["student_number"] = student_number[i];
            students_15["attended"] = attended[i];
            i++;

            if(i >= studentIndex) break;
            ESP_LOGE(TAG_W, "This loop should not reaching this point");
          }    

          attendance_doc.shrinkToFit();  // optional

          String path = "/sessions/ses_";
          unsigned int seed = millis();
          path.concat(rand_r(&seed)%1000000);
          path.concat(".json");
          fs::File attendance_file = LittleFS.open(path, FILE_WRITE);
          if (!attendance_file) {
            Serial.println("Failed to open file for writing");
            return;
          }
          serializeJson(attendance_doc, attendance_file);

          attendance_file.close();

          drawRegisterAttendanceMenu();
        }

        if(finger.getImage() == FINGERPRINT_NOFINGER) continue;
        u8_t id = getFingerprintID();
        if (id == 255) continue;
        else
        { 
          id--;   //changing the id into the appropriate index into the buffers
          attended[id] = true;
          tft.fillRoundRect(20, 70, 280, 225, 10, TFT_DARKERGREY);

          tft.setFreeFont(&FreeSans9pt7b);
          tft.textbgcolor = TFT_DARKERGREY;
          tft.drawString("Place fingerprint on sensor...", 40, 90 );

          int textLength = tft.drawString(surname[id], 41, 125 );
          textLength += tft.drawString(" ", 40 + textLength, 125 );
          tft.drawString(first_name[id], 40 + textLength, 125 );

          String student_number_t = String(student_number[id]);
          tft.drawString(student_number_t, 40, 160);

          tft.drawString("attended", 40, 195);
          tft.fillRoundRect(130, 193, 20, 20, 3, TFT_DARKGREEN);
          tft.drawWideLine(137,209,133,204,3,TFT_WHITE,TFT_DARKGREEN);
          tft.drawWideLine(137,209,146,197,3,TFT_WHITE,TFT_DARKGREEN);
        }
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

  golioth_client_stop(client);
  vTaskSuspend(NULL);

  configTime(3600 * timezone, daysavetime * 3600, "time.nist.gov", "0.pool.ntp.org", "1.pool.ntp.org");
  tmstruct.tm_year = 0;
  getLocalTime(&tmstruct, 3000);
  Serial.printf(
    "\nNow is : %d-%02d-%02d %02d:%02d:%02d\n", (tmstruct.tm_year) + 1900, (tmstruct.tm_mon) + 1, tmstruct.tm_mday, tmstruct.tm_hour, tmstruct.tm_min,
    tmstruct.tm_sec
  );
  Serial.println("");

  uint16_t counter = 0;
  while(1) {
    GLTH_LOGI(TAG_W, "Hello, Golioth! #%d", counter);
    ++counter;
    vTaskDelay(60000 / portTICK_PERIOD_MS);
  }  
}
