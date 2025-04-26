idf_component_register(SRCS "rf_utility.c" "rf_timer.c" "rf_receiver.c" "main.c"
                    PRIV_REQUIRES esp_driver_gpio esp_driver_gptimer esp_timer
                    INCLUDE_DIRS "")
