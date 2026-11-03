#include "wifi_export.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>

#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "storage_paths.h"
#include "sensor_hub.h"

#define AP_SSID "SpeedCoach_Wi-Fi"
#define AP_PASSWORD "SpeedCoach"
static const char *TAG = "wifi_export";
static httpd_handle_t s_server;
static bool s_wifi_initialized;
static bool s_netif_initialized;
static wifi_config_callback_t s_config_cb;
static wifi_activity_ready_callback_t s_activity_ready_cb;
static wifi_activity_callback_t s_activity_cb;
static wifi_battery_callback_t s_battery_cb;
static wifi_sensor_start_callback_t s_sensor_start_cb;
static bool s_sensor_start_attempted;

void wifi_export_set_callbacks(wifi_config_callback_t config_cb,
                               wifi_activity_ready_callback_t ready_cb,
                               wifi_activity_callback_t start_cb,
                               wifi_battery_callback_t battery_cb,
                               wifi_sensor_start_callback_t sensor_start_cb)
{
    s_config_cb = config_cb;
    s_activity_ready_cb = ready_cb;
    s_activity_cb = start_cb;
    s_battery_cb = battery_cb;
    s_sensor_start_cb = sensor_start_cb;
}

static esp_err_t index_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_sendstr_chunk(req, "<!doctype html><meta charset=utf-8><meta name=viewport content='width=device-width'><title>SpeedCoach</title><h1>SpeedCoach</h1><p>Batterie : <span id=b>lecture...</span></p>");
    if (!s_config_cb || !s_activity_ready_cb || !s_activity_cb) {
        httpd_resp_sendstr_chunk(req, "<p>Initialisation des séances… cette page va se recharger.</p><script>setTimeout(()=>location.reload(),2000)</script>");
    } else {
        httpd_resp_sendstr_chunk(req, "<form action=/config><h2>Préparer une séance</h2><label>Type <select name=mode><option value=0>Libre</option><option value=1>Temps unique</option><option value=2>Distance unique</option><option value=3>Intervalles</option></select></label><p>Durée (min) <input name=duration type=number min=1 value=30></p><p>Distance (m) <input name=distance type=number min=1 value=1000></p><p>Travail (s) <input name=work type=number min=1 value=60></p><p>Repos (s) <input name=rest type=number min=1 value=60></p><p>Répétitions <input name=reps type=number min=1 value=8></p><label>Affichage <select name=display><option value=0>Allure, temps, cadence, distance</option><option value=1>Vitesse, temps, cadence, distance</option><option value=2>Courbe d'accélération en direct</option></select></label><p><button>Enregistrer la séance</button></p></form><p><a href=/start>Démarrer cette séance</a></p>");
    }
    httpd_resp_sendstr_chunk(req, "<p>Suunto Watch HR 1029 : <span id=hr>vérification...</span></p><script>function refresh(){fetch('/api/status').then(r=>r.json()).then(x=>document.getElementById('b').textContent=x.battery_pct===null?'indisponible':x.battery_pct+' %'+(x.charging===null?' (charge inconnue)':x.charging?' (en charge)':''));fetch('/api/sensors').then(r=>r.json()).then(x=>document.getElementById('hr').textContent=!x.available?'Bluetooth indisponible':x.connected?(x.bpm===null?'Connectée, BPM en attente — activer HR Broadcast':'Connectée · '+x.bpm+' bpm'):x.scanning?'Recherche de cette montre…':x.connecting?'Connexion à cette montre…':x.saved?'Montre mémorisée · reconnexion en cours':x.seen?'Montre détectée, connexion en attente ou échouée':'Montre non détectée');}refresh();setInterval(refresh,5000);</script><h2>Activités enregistrées</h2><p>Choisissez un fichier TCX :</p><ul>");
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

static esp_err_t status_handler(httpd_req_t *req)
{
    int pct = -1;
    if (!s_battery_cb || s_battery_cb(&pct) != ESP_OK) pct = -1;
    char body[80];
    if (pct < 0) snprintf(body, sizeof(body), "{\"battery_pct\":null,\"charging\":null}");
    else snprintf(body, sizeof(body), "{\"battery_pct\":%d,\"charging\":null}", pct);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, body);
}

static esp_err_t sensors_handler(httpd_req_t *req)
{
    /* Start NimBLE only after the phone has associated and requested the page.
     * This keeps BLE allocations away from the Wi-Fi association window. */
    if (!s_sensor_start_attempted && s_sensor_start_cb) {
        s_sensor_start_attempted = true;
        esp_err_t start_err = s_sensor_start_cb();
        if (start_err != ESP_OK)
            ESP_LOGW(TAG, "BLE sensor startup deferred/failed: %s", esp_err_to_name(start_err));
    }

    sensor_hub_slot_info_t slot = {0};
    sensor_hub_live_t live = {0};
    sensor_kind_t scanning_kind = SENSOR_KIND_COUNT;
    bool available = sensor_hub_available();
    bool scanning = available && sensor_hub_is_scanning(&scanning_kind) &&
                    scanning_kind == SENSOR_KIND_HR;
    if (available) {
        sensor_hub_get_slot(SENSOR_KIND_HR, &slot);
        sensor_hub_get_live(&live);
    }
    char body[128];
    if (live.hr_bpm) {
        snprintf(body, sizeof(body),
                 "{\"available\":%s,\"connected\":%s,\"connecting\":%s,\"saved\":%s,\"scanning\":%s,\"seen\":%s,\"bpm\":%u}",
                 available ? "true" : "false",
                 live.hr_connected ? "true" : "false",
                 slot.state == SENSOR_HUB_CONNECTING ? "true" : "false",
                 slot.saved ? "true" : "false",
                 scanning ? "true" : "false",
                 slot.name[0] ? "true" : "false", (unsigned)live.hr_bpm);
    } else {
        snprintf(body, sizeof(body),
                 "{\"available\":%s,\"connected\":%s,\"connecting\":%s,\"saved\":%s,\"scanning\":%s,\"seen\":%s,\"bpm\":null}",
                 available ? "true" : "false",
                 live.hr_connected ? "true" : "false",
                 slot.state == SENSOR_HUB_CONNECTING ? "true" : "false",
                 slot.saved ? "true" : "false",
                 scanning ? "true" : "false",
                 slot.name[0] ? "true" : "false");
    }
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, body);
}

static uint32_t query_uint(const char *query, const char *key, uint32_t fallback)
{
    char value[16];
    if (httpd_query_key_value(query, key, value, sizeof(value)) != ESP_OK) return fallback;
    char *end = NULL;
    unsigned long parsed = strtoul(value, &end, 10);
    return (end && *end == '\0' && parsed <= 86400ul) ? (uint32_t)parsed : fallback;
}

static esp_err_t config_handler(httpd_req_t *req)
{
    char query[256], response[128];
    if (!s_config_cb || httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Configuration unavailable");
        return ESP_FAIL;
    }
    wifi_workout_config_t cfg = {
        .mode = (wifi_workout_mode_t)query_uint(query, "mode", 0),
        .duration_s = query_uint(query, "duration", 30) * 60u,
        .distance_m = query_uint(query, "distance", 1000),
        .interval_work_s = query_uint(query, "work", 60),
        .interval_rest_s = query_uint(query, "rest", 60),
        .interval_reps = query_uint(query, "reps", 8),
        .display_preset = (uint8_t)query_uint(query, "display", 0),
    };
    if (cfg.mode > WIFI_WORKOUT_INTERVAL || cfg.display_preset > 2 ||
        (cfg.mode == WIFI_WORKOUT_TIME && cfg.duration_s == 0) ||
        (cfg.mode == WIFI_WORKOUT_DISTANCE && cfg.distance_m == 0) ||
        (cfg.mode == WIFI_WORKOUT_INTERVAL && (!cfg.interval_work_s || !cfg.interval_rest_s || !cfg.interval_reps))) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid configuration");
        return ESP_FAIL;
    }
    esp_err_t err = s_config_cb(&cfg);
    snprintf(response, sizeof(response), "<meta charset=utf-8><p>%s</p><a href='/'>Retour au SpeedCoach</a>",
             err == ESP_OK ? "Séance enregistrée. Vous pouvez la démarrer." : "Impossible d'enregistrer la séance.");
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_sendstr(req, response);
}

static void start_activity_work(void *arg)
{
    (void)arg;
    esp_err_t err = s_activity_cb ? s_activity_cb() : ESP_ERR_INVALID_STATE;
    if (err != ESP_OK)
        ESP_LOGW(TAG, "Web activity start rejected: %s", esp_err_to_name(err));
}

static esp_err_t start_handler(httpd_req_t *req)
{
    if (!s_activity_ready_cb || !s_activity_cb) {
        httpd_resp_set_status(req, "409 Conflict");
        httpd_resp_set_type(req, "text/html; charset=utf-8");
        return httpd_resp_sendstr(req,
            "<meta charset=utf-8><p>Séance indisponible : les services du SpeedCoach ne sont pas initialisés.</p><a href='/'>Retour</a>");
    }
    esp_err_t ready = s_activity_ready_cb();
    if (ready != ESP_OK) {
        httpd_resp_set_status(req, "409 Conflict");
        httpd_resp_set_type(req, "text/html; charset=utf-8");
        return httpd_resp_sendstr(req,
            "<meta charset=utf-8><p>Séance impossible : stockage SPIFFS non monté ou activité déjà en cours. Initialisez le stockage puis réessayez.</p><a href='/'>Retour</a>");
    }
    esp_err_t queued = httpd_queue_work(s_server, start_activity_work, NULL);
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    if (queued != ESP_OK) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        return httpd_resp_sendstr(req, "<meta charset=utf-8><p>Impossible de transmettre le démarrage.</p><a href='/'>Retour</a>");
    }
    return httpd_resp_sendstr(req,
        "<meta charset=utf-8><p>Demande de démarrage transmise. Le Wi-Fi sera coupé uniquement si la séance démarre.</p>");
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
    config.ap.max_connection = 4;
    config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    err = esp_wifi_set_mode(WIFI_MODE_AP);
    if (err == ESP_OK) err = esp_wifi_set_protocol(WIFI_IF_AP,
                                                    WIFI_PROTOCOL_11B |
                                                    WIFI_PROTOCOL_11G |
                                                    WIFI_PROTOCOL_11N);
    if (err == ESP_OK) err = esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW20);
    if (err == ESP_OK) err = esp_wifi_set_config(WIFI_IF_AP, &config);
    if (err == ESP_OK) err = esp_wifi_start();
    if (err != ESP_OK) return err;
    httpd_config_t http_config = HTTPD_DEFAULT_CONFIG();
    err = httpd_start(&s_server, &http_config);
    if (err != ESP_OK) {
        /* Keep the AP running so a bare-board Wi-Fi test can still see it;
         * report that HTTP is unavailable without tearing down the radio. */
        ESP_LOGE(TAG, "HTTP server start failed; AP remains active: %s", esp_err_to_name(err));
        return err;
    }
    httpd_uri_t index = {.uri = "/", .method = HTTP_GET, .handler = index_handler};
    httpd_uri_t download = {.uri = "/download", .method = HTTP_GET, .handler = download_handler};
    httpd_uri_t status = {.uri = "/api/status", .method = HTTP_GET, .handler = status_handler};
    httpd_uri_t sensors = {.uri = "/api/sensors", .method = HTTP_GET, .handler = sensors_handler};
    httpd_uri_t config_uri = {.uri = "/config", .method = HTTP_GET, .handler = config_handler};
    httpd_uri_t start = {.uri = "/start", .method = HTTP_GET, .handler = start_handler};
    httpd_register_uri_handler(s_server, &index);
    httpd_register_uri_handler(s_server, &download);
    httpd_register_uri_handler(s_server, &status);
    httpd_register_uri_handler(s_server, &sensors);
    httpd_register_uri_handler(s_server, &config_uri);
    httpd_register_uri_handler(s_server, &start);
    ESP_LOGI(TAG, "AP started: %s (password: %s), http://192.168.4.1", AP_SSID, AP_PASSWORD);
    return ESP_OK;
}

esp_err_t wifi_export_stop(void)
{
    if (s_server) { httpd_stop(s_server); s_server = NULL; }
    return s_wifi_initialized ? esp_wifi_stop() : ESP_OK;
}
