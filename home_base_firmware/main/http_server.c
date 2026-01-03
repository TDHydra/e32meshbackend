#include <esp_http_server.h>
#include <esp_log.h>
#include <cJSON.h>
#include "protocol.h"

static const char *TAG = "http_server";

// Handler for GET /api/v1/status
static esp_err_t status_get_handler(httpd_req_t *req)
{
    const char* resp_str = "{\"status\": \"online\", \"role\": \"home_base\"}";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// Handler for GET /api/v1/devices
static esp_err_t devices_get_handler(httpd_req_t *req)
{
    // In a real app, query the mesh node list
    const char* resp_str = "[{\"id\": \"dev1\", \"status\": \"online\"}]";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// Register URI handlers
void start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    ESP_LOGI(TAG, "Starting web server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t status_uri = {
            .uri       = "/api/v1/status",
            .method    = HTTP_GET,
            .handler   = status_get_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &status_uri);

        httpd_uri_t devices_uri = {
            .uri       = "/api/v1/devices",
            .method    = HTTP_GET,
            .handler   = devices_get_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &devices_uri);
    }
}
