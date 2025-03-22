idf_component_register(SRCS "main.cpp" "utilities.cpp" "golioth_credentials.cpp"
                     INCLUDE_DIRS "."  
                     REQUIRES "arduino"
                     PRIV_REQUIRES "golioth_sdk" "TFT_eSPI" "Adafruit-Fingerprint-Sensor-Library"   
                       )
