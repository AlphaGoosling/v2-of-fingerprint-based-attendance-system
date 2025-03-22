#include "utilities.h"

extern char *TAG;

TFT_eSPI tft = TFT_eSPI();

TaskHandle_t DisplayTaskHandler = NULL;
TaskHandle_t NetworkTaskHandler = NULL;

void Display_Task(void *arg);
void Network_Task(void *arg);

HardwareSerial fingerprintSerial(2);

Adafruit_Fingerprint finger = Adafruit_Fingerprint(&fingerprintSerial);

extern "C" void app_main()
{
  xTaskCreate(Display_Task, "Display", 4096, NULL, 5, &DisplayTaskHandler);
  Serial.begin(115200);
  vTaskDelay(20/portTICK_PERIOD_MS);
  //xTaskCreate(Network_Task, "Network", 4096, NULL, 5, &NetworkTaskHandler);
}

void Display_Task(void *arg){
  Serial.println("starting display");
  tft.init();
  tft.setRotation(0);

  uint16_t calData[5] = { 225, 3565, 221, 3705, 7 };
  tft.setTouch(calData);

  //drawMainmenu();
  drawKeyboard();
  
  while(1){
    vTaskDelay(200/portTICK_PERIOD_MS);
    printf("Aint much, but honest work");
  }
}

void Network_Task(void *arg){
  //Initialize NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);
  ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
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

  struct golioth_client *client = golioth_client_create(&config);
  assert(client);

  uint16_t counter = 0;
  while(1) {
    GLTH_LOGI(TAG, "Hello, Golioth! #%d", counter);
    ++counter;
    vTaskDelay(5000 / portTICK_PERIOD_MS);
  }  
}