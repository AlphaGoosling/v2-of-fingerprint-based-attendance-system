#include "utilities.h"

extern char *TAG;

TFT_eSPI tft = TFT_eSPI();

extern char numberBuffer[NUM_LEN + 1];
extern uint8_t numberIndex;
extern const char keyboardKeyLabels[40];
extern TFT_eSPI_Button keyboardKeys[40];

TaskHandle_t DisplayTaskHandler = NULL;
TaskHandle_t NetworkTaskHandler = NULL;

void Display_Task(void *arg);
void Network_Task(void *arg);

HardwareSerial fingerprintSerial(2);

Adafruit_Fingerprint finger = Adafruit_Fingerprint(&fingerprintSerial);

extern "C" void app_main()
{
  Serial.begin(115200);
  vTaskDelay(20/portTICK_PERIOD_MS);
  fingerprintSerial.begin(115200, SERIAL_8N1, 16, 17);
  vTaskDelay(20/portTICK_PERIOD_MS);
  finger.begin(57600);
  vTaskDelay(20/portTICK_PERIOD_MS);

  xTaskCreate(Display_Task, "Display", 4096, NULL, 5, &DisplayTaskHandler);
  //xTaskCreate(Network_Task, "Network", 4096, NULL, 5, &NetworkTaskHandler);

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

   TAG = "esp_littlefs";

  ESP_LOGI(TAG, "Initializing LittleFS");

  esp_vfs_littlefs_conf_t conf = {
      .base_path = "/littlefs",
      .partition_label = "storage",
      .format_if_mount_failed = true,
      .dont_mount = false,
  };

  // Use settings defined above to initialize and mount LittleFS filesystem.
  // Note: esp_vfs_littlefs_register is an all-in-one convenience function.
  esp_err_t ret = esp_vfs_littlefs_register(&conf);

  if (ret != ESP_OK) {
      if (ret == ESP_FAIL) {
          ESP_LOGE(TAG, "Failed to mount or format filesystem");
      } else if (ret == ESP_ERR_NOT_FOUND) {
          ESP_LOGE(TAG, "Failed to find LittleFS partition");
      } else {
          ESP_LOGE(TAG, "Failed to initialize LittleFS (%s)", esp_err_to_name(ret));
      }
      return;
  }

  size_t total = 0, used = 0;
  ret = esp_littlefs_info(conf.partition_label, &total, &used);
  if (ret != ESP_OK) {
      ESP_LOGE(TAG, "Failed to get LittleFS partition information (%s)", esp_err_to_name(ret));
      esp_littlefs_format(conf.partition_label);
  } else {
      ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
  }
  
  //File io starts here

  FILE *file = fopen("/littlefs/Mec3-LH.txt", "r");
  if (file == NULL) {
      ESP_LOGE(TAG, "Failed to open file for reading");
      return;
  }

  char line[128] = {0};
  fgets(line, sizeof(line), file);
  fclose(file);
  // strip newline
  char* pos = strpbrk(line, "\r\n");
  if (pos) {
      *pos = '\0';
  }
  ESP_LOGI(TAG, "Read from file: '%s'", line);


  // All done, unmount partition and disable LittleFS
  //esp_vfs_littlefs_unregister(conf.partition_label);
  //ESP_LOGI(TAG, "LittleFS unmounted");
}

void Display_Task(void *arg){
  Serial.println("starting display");
  tft.init();
  tft.setRotation(0);

  uint16_t calData[5] = { 225, 3565, 221, 3705, 7 };
  tft.setTouch(calData);

  drawMainmenu();
  //drawKeyboard();
  //drawWifiMenu();
  
  while(1){
    vTaskDelay(2000/portTICK_PERIOD_MS);
    printf("Aint much, but honest work");
    
  }
}

void Network_Task(void *arg){
  char *TAG = "wifi station";
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