idf_component_register(SRCS "rf_receiver.c" "rf_timer.c" "main.c" "rf_transmitter.c"
                    PRIV_REQUIRES esp_driver_gpio esp_driver_gptimer esp_timer
                    INCLUDE_DIRS "")
