#pragma once

#include <stdint.h>
#include "esp_err.h"

typedef enum
{
    ACT_CMD_START = 0,
    ACT_CMD_STOP_SAVE,
} act_cmd_t;

void activity_worker_task(void *arg);
void activity_logger_task(void *arg);
esp_err_t app_activity_post_cmd(act_cmd_t cmd);
