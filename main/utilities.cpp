#include "utilities.h"


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

const char *TAG = "wifi station";

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
                                    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', '>', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '_', '_', '<' };

char *mainMenuKeyLabels[5] = {"WiFi", "Register Attendance", "Add new Student", "Delete entry", "Connect to Server"};

void drawKeyboard()
{
  tft.fillScreen((TFT_BLACK));

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


void drawMainmenu()
{
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

// Print something in the mini typeString bar
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
