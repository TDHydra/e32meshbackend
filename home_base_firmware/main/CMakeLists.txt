idf_component_register(SRCS "main.c" "http_server.c" "esp_now_mesh.c" "unraid_client.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_http_server esp_wifi esp_now nvs_flash esp_eth lwip json)
