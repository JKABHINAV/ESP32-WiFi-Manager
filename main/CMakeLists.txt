idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES spiffs esp_http_server nvs_flash esp_wifi esp_netif esp_event)

# This command works here because the component has loaded.
# Notice the "../" because your spiffs_image folder is one level up from the main folder.
spiffs_create_partition_image(storage "../spiffs_image" FLASH_IN_PROJECT)