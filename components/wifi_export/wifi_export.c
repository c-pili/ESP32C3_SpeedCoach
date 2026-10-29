#include "wifi_export.h"

#include <dirent.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>

#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "storage_paths.h"

#define AP_SSID "SpeedCoach_Wi-Fi"
#define AP_PASSWORD "SpeedCoach"
static const char *TAG = "wifi_export";
static httpd_handle_t s_server;
static bool s_wifi_initialized;
static bool s_netif_initialized;

static esp_err_t index_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_sendstr_chunk(req, "<!doctype html><meta charset=utf-8><meta name=viewport content='width=device-width'><title>SpeedCoach</title><h1>Entrainements SpeedCoach</h1><p>Choisissez un fichier TCX :</p><ul>");
    DIR *activities = opendir(ACTIVITY_DIR);
    if (activities) {
        struct dirent *activity;
        while ((activity = readdir(activities)) != NULL) {
            if (activity->d_name[0] == '.') continue;
            /* Activity folder names are generated locally; skip unexpected
             * overlong entries so the URL, path, and link text stay bounded. */
            if (strnlen(activity->d_name, 128) >= 128) continue;
            char relative[160], full[256];
            snprintf(relative, sizeof(relative), "%.127s/Workout.tcx", activity->d_name);
            snprintf(full, sizeof(full), ACTIVITY_DIR "/%s", relative);
            struct stat st;
            if (stat(full, &st) != 0 || !S_ISREG(st.st_mode)) continue;
            char row[384];
            snprintf(row, sizeof(row), "<li><a href='/download?file=%.139s'>%.127s.tcx</a></li>",
                     relative, activity->d_name);
            httpd_resp_sendstr_chunk(req, row);
        }
        closedir(activities);
    }
    httpd_resp_sendstr_chunk(req, "</ul>");
    return httpd_resp_sendstr_chunk(req, NULL);
}

static esp_err_t download_handler(httpd_req_t *req)
{
    char query[256], relative[192], path[256];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK ||
        httpd_query_key_value(query, "file", relative, sizeof(relative)) != ESP_OK ||
        strstr(relative, "..") || strchr(relative, '\\') || !strstr(relative, "/Workout.tcx")) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid file");
        return ESP_FAIL;
    }
    snprintf(path, sizeof(path), ACTIVITY_DIR "/%s", relative);
    FILE *file = fopen(path, "rb");
    if (!file) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Activity not found");
        return ESP_FAIL;
    }
    char filename[128];
    const char *slash = strchr(relative, '/');
    size_t name_len = slash ? (size_t)(slash - relative) : 0;
    if (!name_len || name_len >= sizeof(filename)) { fclose(file); return ESP_FAIL; }
    memcpy(filename, relative, name_len); filename[name_len] = '\0';
    char disposition[180];
    snprintf(disposition, sizeof(disposition), "attachment; filename=\"%s.tcx\"", filename);
    httpd_resp_set_type(req, "application/vnd.garmin.tcx+xml");
    httpd_resp_set_hdr(req, "Content-Disposition", disposition);
    char chunk[1024]; size_t n;
    while ((n = fread(chunk, 1, sizeof(chunk), file)) > 0) {
        if (httpd_resp_send_chunk(req, chunk, n) != ESP_OK) { fclose(file); return ESP_FAIL; }
    }
    fclose(file);
    return httpd_resp_send_chunk(req, NULL, 0);
}

esp_err_t wifi_export_start(void)
{
    if (s_server) return ESP_OK;
    if (!s_netif_initialized) {
        esp_err_t net_err = esp_netif_init();
        if (net_err != ESP_OK && net_err != ESP_ERR_INVALID_STATE) return net_err;
        s_netif_initialized = true;
    }
    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;
    if (!s_wifi_initialized) {
        if (!esp_netif_create_default_wifi_ap()) return ESP_FAIL;
        wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
        err = esp_wifi_init(&init);
        if (err != ESP_OK) return err;
        s_wifi_initialized = true;
    }
    wifi_config_t config = {0};
    memcpy(config.ap.ssid, AP_SSID, sizeof(AP_SSID) - 1);
    memcpy(config.ap.password, AP_PASSWORD, sizeof(AP_PASSWORD) - 1);
    config.ap.ssid_len = sizeof(AP_SSID) - 1;
    config.ap.channel = 1;
    config.ap.max_connection = 3;
    config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    err = esp_wifi_set_mode(WIFI_MODE_AP);
    if (err == ESP_OK) err = esp_wifi_set_config(WIFI_IF_AP, &config);
    if (err == ESP_OK) err = esp_wifi_start();
    if (err != ESP_OK) return err;
    httpd_config_t http_config = HTTPD_DEFAULT_CONFIG();
    err = httpd_start(&s_server, &http_config);
    if (err != ESP_OK) { esp_wifi_stop(); return err; }
    httpd_uri_t index = {.uri = "/", .method = HTTP_GET, .handler = index_handler};
    httpd_uri_t download = {.uri = "/download", .method = HTTP_GET, .handler = download_handler};
    httpd_register_uri_handler(s_server, &index);
    httpd_register_uri_handler(s_server, &download);
    ESP_LOGI(TAG, "AP started: %s (password: %s), http://192.168.4.1", AP_SSID, AP_PASSWORD);
    return ESP_OK;
}

esp_err_t wifi_export_stop(void)
{
    if (s_server) { httpd_stop(s_server); s_server = NULL; }
    return s_wifi_initialized ? esp_wifi_stop() : ESP_OK;
}
