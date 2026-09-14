/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 Cpt_Kirk
 */
#include "panel_mqtt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "cJSON.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_random.h"
#include "mqtt_client.h"

#include "app_config.h"
#include "drivers/display_init.h"
#include "ui/ui_screen_saver.h"
#include "util/log_tags.h"

#define MQTT_URI_MAX_LEN (APP_MQTT_HOST_MAX_LEN + 16)
#define MQTT_TOPIC_MAX_LEN 96
#define MQTT_DISCOVERY_TOPIC_MAX_LEN 224

typedef struct {
    const char *object_id;
    const char *name;
    const char *component;   /* "switch" | "number" | "button" */
    const char *command_key; /* e.g. "screensaver_enabled", NULL for none */
    int number_min;
    int number_max;
    int number_step;
} mqtt_entity_t;

/* All strings below must remain valid for the lifetime of the esp-mqtt
 * client: the config struct only stores pointers, it does not copy them. */
static char s_broker_uri[MQTT_URI_MAX_LEN];
static char s_client_id[64];
static char s_username[APP_MQTT_USERNAME_MAX_LEN];
static char s_password[APP_MQTT_PASSWORD_MAX_LEN];
static char s_discovery_prefix[APP_MQTT_DISCOVERY_PREFIX_MAX_LEN];
static char s_node_suffix[16];
static char s_base_topic[64];
static char s_state_topic[MQTT_TOPIC_MAX_LEN];
static char s_availability_topic[MQTT_TOPIC_MAX_LEN];
static char s_command_prefix[MQTT_TOPIC_MAX_LEN];
static char s_subscribe_topic[MQTT_TOPIC_MAX_LEN];
static size_t s_command_prefix_len;

static esp_mqtt_client_config_t s_cfg;
static esp_mqtt_client_handle_t s_client = NULL;
static bool s_initialized = false;
static bool s_started = false;
static volatile bool s_connected = false;

/* Snapshot of the last applied MQTT settings, used to detect config changes. */
static bool s_have_applied = false;
static char s_applied_host[APP_MQTT_HOST_MAX_LEN];
static uint16_t s_applied_port = 0;
static char s_applied_username[APP_MQTT_USERNAME_MAX_LEN];
static char s_applied_password[APP_MQTT_PASSWORD_MAX_LEN];
static char s_applied_prefix[APP_MQTT_DISCOVERY_PREFIX_MAX_LEN];

static void panel_mqtt_publish_discovery(esp_mqtt_client_handle_t client);
static void panel_mqtt_publish_state(esp_mqtt_client_handle_t client);
static void panel_mqtt_publish_state_from(const runtime_settings_t *settings, esp_mqtt_client_handle_t client);

static void panel_mqtt_compute_node_id(void)
{
    uint8_t mac[6] = {0};
    if (esp_read_mac(mac, ESP_MAC_WIFI_STA) != ESP_OK) {
        uint32_t rnd = esp_random();
        snprintf(s_node_suffix, sizeof(s_node_suffix), "%06lX", (unsigned long)(rnd & 0xFFFFFFUL));
    } else {
        snprintf(s_node_suffix, sizeof(s_node_suffix), "%02X%02X%02X", mac[3], mac[4], mac[5]);
    }

    snprintf(s_client_id, sizeof(s_client_id), "betta_panel_%s", s_node_suffix);
    snprintf(s_base_topic, sizeof(s_base_topic), "betta_panel/%s", s_node_suffix);
    snprintf(s_state_topic, sizeof(s_state_topic), "%s/state", s_base_topic);
    snprintf(s_availability_topic, sizeof(s_availability_topic), "%s/status", s_base_topic);
    snprintf(s_command_prefix, sizeof(s_command_prefix), "%s/set/", s_base_topic);
    snprintf(s_subscribe_topic, sizeof(s_subscribe_topic), "%s/set/+", s_base_topic);
    s_command_prefix_len = strlen(s_command_prefix);
}

void panel_mqtt_init(void)
{
    if (s_initialized) {
        return;
    }
    panel_mqtt_compute_node_id();
    s_initialized = true;
}

static void panel_mqtt_derive_host(const runtime_settings_t *settings, char *out, size_t out_len)
{
    out[0] = '\0';

    if (settings->mqtt_host[0] != '\0') {
        snprintf(out, out_len, "%s", settings->mqtt_host);
        return;
    }

    /* No explicit broker: derive the host from the HA WebSocket URL
     * (e.g. ws://192.168.1.10:8123/api/websocket -> 192.168.1.10). */
    const char *ws = settings->ha_ws_url;
    const char *scheme = strstr(ws, "://");
    if (scheme == NULL) {
        return;
    }
    const char *start = scheme + 3;
    const char *end = strchr(start, ':');
    const char *slash = strchr(start, '/');
    if (end == NULL || (slash != NULL && slash < end)) {
        end = slash;
    }
    if (end == NULL) {
        end = start + strlen(start);
    }
    if (end <= start) {
        return;
    }
    size_t len = (size_t)(end - start);
    if (len >= out_len) {
        len = out_len - 1;
    }
    memcpy(out, start, len);
    out[len] = '\0';
}

static bool panel_mqtt_build_config(const runtime_settings_t *settings)
{
    char host[APP_MQTT_HOST_MAX_LEN];
    panel_mqtt_derive_host(settings, host, sizeof(host));
    if (host[0] == '\0') {
        ESP_LOGW(TAG_MQTT, "cannot derive MQTT broker host");
        return false;
    }

    snprintf(s_broker_uri, sizeof(s_broker_uri), "mqtt://%s:%u", host, (unsigned)settings->mqtt_port);
    snprintf(s_username, sizeof(s_username), "%s", settings->mqtt_username);
    snprintf(s_password, sizeof(s_password), "%s", settings->mqtt_password);
    snprintf(s_discovery_prefix, sizeof(s_discovery_prefix), "%s",
             settings->mqtt_discovery_prefix[0] != '\0' ? settings->mqtt_discovery_prefix
                                                        : APP_MQTT_DISCOVERY_PREFIX_DEFAULT);

    s_cfg.broker.address.uri = s_broker_uri;
    s_cfg.credentials.client_id = s_client_id;
    s_cfg.credentials.username = s_username[0] != '\0' ? s_username : NULL;
    s_cfg.credentials.authentication.password = s_password[0] != '\0' ? s_password : NULL;
    s_cfg.session.last_will.topic = s_availability_topic;
    s_cfg.session.last_will.msg = "offline";
    s_cfg.session.last_will.msg_len = strlen("offline");
    s_cfg.session.last_will.qos = 1;
    s_cfg.session.last_will.retain = 1;
    s_cfg.session.keepalive = 60;
    s_cfg.network.disable_auto_reconnect = false;

    /* The MQTT task handles incoming commands synchronously inside the event
     * loop. The command path stacks two runtime_settings_t (~1.5 KB each) plus
     * several nested NVS/cJSON locals on top of the 6 KB default task stack,
     * which overflowed and corrupted the event loop's handler list. Give the
     * task enough headroom. */
    s_cfg.task.stack_size = 16384;

    return true;
}

static void panel_mqtt_record_applied(const runtime_settings_t *settings)
{
    char host[APP_MQTT_HOST_MAX_LEN];
    panel_mqtt_derive_host(settings, host, sizeof(host));
    snprintf(s_applied_host, sizeof(s_applied_host), "%s", host);
    s_applied_port = settings->mqtt_port;
    snprintf(s_applied_username, sizeof(s_applied_username), "%s", settings->mqtt_username);
    snprintf(s_applied_password, sizeof(s_applied_password), "%s", settings->mqtt_password);
    snprintf(s_applied_prefix, sizeof(s_applied_prefix), "%s", s_discovery_prefix);
    s_have_applied = true;
}

static bool panel_mqtt_config_changed(const runtime_settings_t *settings)
{
    if (!s_have_applied) {
        return true;
    }
    char host[APP_MQTT_HOST_MAX_LEN];
    panel_mqtt_derive_host(settings, host, sizeof(host));
    const char *prefix = settings->mqtt_discovery_prefix[0] != '\0'
                             ? settings->mqtt_discovery_prefix
                             : APP_MQTT_DISCOVERY_PREFIX_DEFAULT;
    return strcmp(host, s_applied_host) != 0 ||
           settings->mqtt_port != s_applied_port ||
           strcmp(settings->mqtt_username, s_applied_username) != 0 ||
           strcmp(settings->mqtt_password, s_applied_password) != 0 ||
           strcmp(prefix, s_applied_prefix) != 0;
}

static bool panel_mqtt_parse_bool(const char *payload, bool *out)
{
    if (payload == NULL || out == NULL) {
        return false;
    }
    if (strcasecmp(payload, "ON") == 0 || strcasecmp(payload, "true") == 0 ||
        strcmp(payload, "1") == 0) {
        *out = true;
        return true;
    }
    if (strcasecmp(payload, "OFF") == 0 || strcasecmp(payload, "false") == 0 ||
        strcmp(payload, "0") == 0) {
        *out = false;
        return true;
    }
    return false;
}

static bool panel_mqtt_parse_int(const char *payload, int *out)
{
    if (payload == NULL || out == NULL || payload[0] == '\0') {
        return false;
    }
    char *end = NULL;
    long v = strtol(payload, &end, 10);
    if (end == payload || *end != '\0') {
        return false;
    }
    *out = (int)v;
    return true;
}

static int panel_mqtt_clamp(int v, int lo, int hi)
{
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

static void panel_mqtt_apply_command(esp_mqtt_client_handle_t client, const char *key, const char *payload)
{
    if (strcmp(key, "wake") == 0) {
        /* Wake to the screensaver clock (motion-sensor style); a touch
         * dismisses the clock and opens the full UI. */
        ui_screen_saver_wake();
        return;
    }

    runtime_settings_t s;
    if (runtime_settings_load(&s) != ESP_OK) {
        runtime_settings_set_defaults(&s);
    }

    bool changed = false;
    bool b = false;
    int n = 0;

    if (strcmp(key, "screensaver_enabled") == 0 && panel_mqtt_parse_bool(payload, &b)) {
        if (b != s.display_screensaver_enabled) {
            s.display_screensaver_enabled = b;
            changed = true;
        }
    } else if (strcmp(key, "screen_off_enabled") == 0 && panel_mqtt_parse_bool(payload, &b)) {
        if (b != s.display_screen_off_enabled) {
            s.display_screen_off_enabled = b;
            changed = true;
        }
    } else if (strcmp(key, "clock_24h") == 0 && panel_mqtt_parse_bool(payload, &b)) {
        if (b != s.display_clock_24h) {
            s.display_clock_24h = b;
            changed = true;
        }
    } else if (strcmp(key, "saver_show_seconds") == 0 && panel_mqtt_parse_bool(payload, &b)) {
        if (b != s.display_saver_show_seconds) {
            s.display_saver_show_seconds = b;
            changed = true;
        }
    } else if (strcmp(key, "saver_show_date") == 0 && panel_mqtt_parse_bool(payload, &b)) {
        if (b != s.display_saver_show_date) {
            s.display_saver_show_date = b;
            changed = true;
        }
    } else if (strcmp(key, "brightness") == 0 && panel_mqtt_parse_int(payload, &n)) {
        n = panel_mqtt_clamp(n, 1, 100);
        if ((uint8_t)n != s.display_brightness) {
            s.display_brightness = (uint8_t)n;
            changed = true;
        }
    } else if (strcmp(key, "screensaver_timeout_sec") == 0 && panel_mqtt_parse_int(payload, &n)) {
        n = panel_mqtt_clamp(n, 5, 3600);
        if ((uint32_t)n != s.display_screensaver_timeout_sec) {
            s.display_screensaver_timeout_sec = (uint32_t)n;
            changed = true;
        }
    } else if (strcmp(key, "screen_off_timeout_sec") == 0 && panel_mqtt_parse_int(payload, &n)) {
        n = panel_mqtt_clamp(n, 5, 7200);
        if ((uint32_t)n != s.display_screen_off_timeout_sec) {
            s.display_screen_off_timeout_sec = (uint32_t)n;
            changed = true;
        }
    } else {
        ESP_LOGW(TAG_MQTT, "unknown command key '%s'", key);
        return;
    }

    if (changed) {
        runtime_settings_save(&s);
        ui_screen_saver_apply_settings(&s);
        panel_mqtt_publish_state_from(&s, client);
    }
}

static void panel_mqtt_handle_data(esp_mqtt_event_handle_t event)
{
    if (event->topic == NULL) {
        return;
    }

    /* esp-mqtt does NOT guarantee null-terminated topic/data; use the length
     * fields and copy into null-terminated buffers. */
    int topic_len = event->topic_len > 0 ? event->topic_len : (int)strlen(event->topic);
    if (topic_len <= (int)s_command_prefix_len ||
        strncmp(event->topic, s_command_prefix, s_command_prefix_len) != 0) {
        return;
    }

    char key[MQTT_TOPIC_MAX_LEN];
    int key_len = topic_len - (int)s_command_prefix_len;
    if (key_len <= 0 || key_len >= (int)sizeof(key)) {
        return;
    }
    memcpy(key, event->topic + s_command_prefix_len, (size_t)key_len);
    key[key_len] = '\0';
    if (strchr(key, '/') != NULL) {
        return;
    }

    char payload[64] = {0};
    size_t len = (event->data != NULL) ? (size_t)event->data_len : 0;
    if (len >= sizeof(payload)) {
        len = sizeof(payload) - 1;
    }
    if (len > 0) {
        memcpy(payload, event->data, len);
        payload[len] = '\0';
    }

    /* Empty payloads are only our own retained-clear messages; ignore them to
     * avoid a clear -> redeliver -> clear loop. */
    if (len == 0) {
        return;
    }

    ESP_LOGI(TAG_MQTT, "command: %s=%s", key, payload);
    panel_mqtt_apply_command(event->client, key, payload);

    /* Clear any retained value on this command topic so a stale command cannot
     * be re-applied after the next reconnect. */
    char command_topic[MQTT_TOPIC_MAX_LEN * 2];
    snprintf(command_topic, sizeof(command_topic), "%s%s", s_command_prefix, key);
    esp_mqtt_client_enqueue(event->client, command_topic, "", 0, 1, 1, true);
}

static void panel_mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    (void)handler_args;
    (void)base;

    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        s_connected = true;
        ESP_LOGI(TAG_MQTT, "connected to broker");
        esp_mqtt_client_subscribe(event->client, s_subscribe_topic, 0);
        esp_mqtt_client_enqueue(event->client, s_availability_topic, "online", strlen("online"), 1, 1, true);
        panel_mqtt_publish_discovery(event->client);
        panel_mqtt_publish_state(event->client);
        break;
    case MQTT_EVENT_DISCONNECTED:
        s_connected = false;
        ESP_LOGI(TAG_MQTT, "disconnected from broker");
        break;
    case MQTT_EVENT_DATA:
        panel_mqtt_handle_data(event);
        break;
    default:
        break;
    }
}

static void panel_mqtt_add_discovery_entity(esp_mqtt_client_handle_t client,
                                            const char *object_id,
                                            const char *name,
                                            const char *component,
                                            const char *command_key,
                                            int number_min,
                                            int number_max,
                                            int number_step)
{
    char topic[MQTT_DISCOVERY_TOPIC_MAX_LEN];
    snprintf(topic, sizeof(topic), "%s/%s/%s/config", s_discovery_prefix, component, object_id);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "name", name);
    cJSON_AddStringToObject(root, "unique_id", object_id);
    cJSON_AddStringToObject(root, "state_topic", s_state_topic);
    cJSON_AddStringToObject(root, "availability_topic", s_availability_topic);
    cJSON_AddBoolToObject(root, "retain", true);

    cJSON *device = cJSON_CreateObject();
    cJSON *identifiers = cJSON_AddArrayToObject(device, "identifiers");
    cJSON_AddItemToArray(identifiers, cJSON_CreateString(s_client_id));
    cJSON_AddStringToObject(device, "name", "BETTA Panel");
    cJSON_AddStringToObject(device, "manufacturer", "Guition");
    cJSON_AddStringToObject(device, "model", "ESP32-S3 4848S040");
    cJSON_AddItemToObject(root, "device", device);

    if (command_key != NULL) {
        char command_topic[MQTT_TOPIC_MAX_LEN];
        snprintf(command_topic, sizeof(command_topic), "%s%s", s_command_prefix, command_key);
        cJSON_AddStringToObject(root, "command_topic", command_topic);

        if (strcmp(component, "switch") == 0) {
            char value_template[160];
            snprintf(value_template, sizeof(value_template),
                     "{{ 'ON' if value_json.%s else 'OFF' }}", command_key);
            cJSON_AddStringToObject(root, "value_template", value_template);
            cJSON_AddStringToObject(root, "payload_on", "ON");
            cJSON_AddStringToObject(root, "payload_off", "OFF");
            cJSON_AddStringToObject(root, "state_on", "ON");
            cJSON_AddStringToObject(root, "state_off", "OFF");
        } else if (strcmp(component, "number") == 0) {
            char value_template[128];
            snprintf(value_template, sizeof(value_template), "{{ value_json.%s }}", command_key);
            cJSON_AddStringToObject(root, "value_template", value_template);
            cJSON_AddNumberToObject(root, "min", number_min);
            cJSON_AddNumberToObject(root, "max", number_max);
            cJSON_AddNumberToObject(root, "step", number_step);
            cJSON_AddStringToObject(root, "mode", "box");
        } else if (strcmp(component, "button") == 0) {
            cJSON_AddStringToObject(root, "payload_press", "PRESS");
        }
    }

    char *payload = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (payload != NULL) {
        esp_mqtt_client_enqueue(client, topic, payload, 0, 1, 1, true);
        cJSON_free(payload);
    }
}

static void panel_mqtt_publish_discovery(esp_mqtt_client_handle_t client)
{
    char object_id[MQTT_TOPIC_MAX_LEN];

    snprintf(object_id, sizeof(object_id), "%s_screensaver", s_client_id);
    panel_mqtt_add_discovery_entity(client, object_id, "Screensaver",
                                    "switch", "screensaver_enabled", 0, 0, 0);
    snprintf(object_id, sizeof(object_id), "%s_screen_off", s_client_id);
    panel_mqtt_add_discovery_entity(client, object_id, "Screen Off",
                                    "switch", "screen_off_enabled", 0, 0, 0);
    snprintf(object_id, sizeof(object_id), "%s_clock_24h", s_client_id);
    panel_mqtt_add_discovery_entity(client, object_id, "Clock 24h",
                                    "switch", "clock_24h", 0, 0, 0);
    snprintf(object_id, sizeof(object_id), "%s_show_seconds", s_client_id);
    panel_mqtt_add_discovery_entity(client, object_id, "Show Seconds",
                                    "switch", "saver_show_seconds", 0, 0, 0);
    snprintf(object_id, sizeof(object_id), "%s_show_date", s_client_id);
    panel_mqtt_add_discovery_entity(client, object_id, "Show Date",
                                    "switch", "saver_show_date", 0, 0, 0);

    snprintf(object_id, sizeof(object_id), "%s_brightness", s_client_id);
    panel_mqtt_add_discovery_entity(client, object_id, "Brightness",
                                    "number", "brightness", 1, 100, 1);
    snprintf(object_id, sizeof(object_id), "%s_saver_timeout", s_client_id);
    panel_mqtt_add_discovery_entity(client, object_id, "Screensaver Timeout",
                                    "number", "screensaver_timeout_sec", 5, 3600, 5);
    snprintf(object_id, sizeof(object_id), "%s_off_timeout", s_client_id);
    panel_mqtt_add_discovery_entity(client, object_id, "Screen Off Timeout",
                                    "number", "screen_off_timeout_sec", 5, 7200, 5);

    snprintf(object_id, sizeof(object_id), "%s_wake", s_client_id);
    panel_mqtt_add_discovery_entity(client, object_id, "Wake Display",
                                    "button", "wake", 0, 0, 0);
}

static void panel_mqtt_publish_state_from(const runtime_settings_t *s, esp_mqtt_client_handle_t client)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "screensaver_enabled", s->display_screensaver_enabled);
    cJSON_AddBoolToObject(root, "screen_off_enabled", s->display_screen_off_enabled);
    cJSON_AddBoolToObject(root, "clock_24h", s->display_clock_24h);
    cJSON_AddBoolToObject(root, "saver_show_seconds", s->display_saver_show_seconds);
    cJSON_AddBoolToObject(root, "saver_show_date", s->display_saver_show_date);
    cJSON_AddNumberToObject(root, "brightness", s->display_brightness);
    cJSON_AddNumberToObject(root, "screensaver_timeout_sec", (double)s->display_screensaver_timeout_sec);
    cJSON_AddNumberToObject(root, "screen_off_timeout_sec", (double)s->display_screen_off_timeout_sec);

    char *payload = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (payload != NULL) {
        esp_mqtt_client_enqueue(client, s_state_topic, payload, 0, 1, 1, true);
        cJSON_free(payload);
    }
}

static void panel_mqtt_publish_state(esp_mqtt_client_handle_t client)
{
    runtime_settings_t s;
    if (runtime_settings_load(&s) != ESP_OK) {
        runtime_settings_set_defaults(&s);
    }
    panel_mqtt_publish_state_from(&s, client);
}

void panel_mqtt_apply_settings(const runtime_settings_t *settings)
{
    if (!s_initialized) {
        panel_mqtt_init();
    }
    if (settings == NULL) {
        return;
    }

    if (!settings->mqtt_enabled) {
        if (s_client != NULL && s_started) {
            esp_mqtt_client_stop(s_client);
        }
        s_started = false;
        s_connected = false;
        s_have_applied = false;
        ESP_LOGI(TAG_MQTT, "disabled");
        return;
    }

    if (!panel_mqtt_build_config(settings)) {
        if (s_client != NULL && s_started) {
            esp_mqtt_client_stop(s_client);
        }
        s_started = false;
        s_connected = false;
        s_have_applied = false;
        return;
    }

    bool created = false;
    if (s_client == NULL) {
        s_client = esp_mqtt_client_init(&s_cfg);
        if (s_client == NULL) {
            ESP_LOGE(TAG_MQTT, "failed to init client");
            return;
        }
        esp_mqtt_client_register_event(s_client, MQTT_EVENT_ANY, panel_mqtt_event_handler, NULL);
        created = true;
    }

    bool config_changed = created || panel_mqtt_config_changed(settings);
    if (config_changed) {
        if (s_started) {
            esp_mqtt_client_stop(s_client);
            s_started = false;
            s_connected = false;
        }
        if (!created) {
            esp_mqtt_set_config(s_client, &s_cfg);
        }
    }

    if (!s_started) {
        esp_err_t err = esp_mqtt_client_start(s_client);
        if (err == ESP_OK) {
            s_started = true;
        } else {
            ESP_LOGW(TAG_MQTT, "start failed: %s", esp_err_to_name(err));
        }
    }

    panel_mqtt_record_applied(settings);
}

void panel_mqtt_notify_settings_changed(void)
{
    runtime_settings_t s;
    if (runtime_settings_load(&s) != ESP_OK) {
        runtime_settings_set_defaults(&s);
    }
    panel_mqtt_apply_settings(&s);
    if (s.mqtt_enabled && s_client != NULL && s_connected) {
        panel_mqtt_publish_state(s_client);
    }
}

bool panel_mqtt_is_connected(void)
{
    return s_connected;
}
