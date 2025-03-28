idf_component_register(SRCS "main.cpp" "utilities.cpp" "golioth_credentials.cpp"
                     INCLUDE_DIRS "."  
                     REQUIRES "arduino"
                     PRIV_REQUIRES "golioth_sdk" "TFT_eSPI" "Adafruit-Fingerprint-Sensor-Library" "ArduinoJson"
                       )

littlefs_create_partition_image(storage ../flash_data FLASH_IN_PROJECT)
