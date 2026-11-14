/* ==============================================================================
 * wdgm.c — AUTOSAR Watchdog Manager Implementation
 * ============================================================================== */
#include "wdgm.h"
#include <string.h>

static wdgm_entity_cfg_t   s_entity_cfg[WDGM_MAX_ENTITIES];
static uint8_t             s_alive_counts[WDGM_MAX_ENTITIES];
static wdgm_status_t       s_global_status = WDGM_STATUS_OK;
static uint32_t            s_reload_success_count = 0;
static wdgm_feed_hook_fn_t s_feed_hook = NULL;

#if defined(__GNUC__) || defined(__clang__)
__attribute__((weak))
#endif
void wdgm_hal_feed_iwdg(void)
{
    /* Mặc định rỗng khi chạy host test hoặc board chưa bật IWDG */
}

void WdgM_Init(void)
{
    memset(s_entity_cfg, 0, sizeof(s_entity_cfg));
    memset(s_alive_counts, 0, sizeof(s_alive_counts));
    s_global_status = WDGM_STATUS_OK;
    s_reload_success_count = 0;
    s_feed_hook = NULL;
}

bool WdgM_ConfigureEntity(uint8_t se_id, uint8_t min_alive, uint8_t max_alive)
{
    if (se_id >= WDGM_MAX_ENTITIES) {
        return false;
    }
    s_entity_cfg[se_id].min_alive  = min_alive;
    s_entity_cfg[se_id].max_alive  = max_alive;
    s_entity_cfg[se_id].configured = true;
    s_alive_counts[se_id] = 0;
    return true;
}

void WdgM_CheckpointReached(uint8_t se_id)
{
    if (se_id < WDGM_MAX_ENTITIES && s_entity_cfg[se_id].configured) {
        if (s_alive_counts[se_id] < 255u) {
            s_alive_counts[se_id]++;
        }
    }
}

void WdgM_MainFunction_100ms(void)
{
    if (s_global_status == WDGM_STATUS_EXPIRED) {
        /* Đã rơi vào trạng thái EXPIRED: ngắt nạp vĩnh viễn chờ reset phần cứng */
        return;
    }

    bool all_entities_healthy = true;

    for (uint8_t i = 0; i < WDGM_MAX_ENTITIES; i++) {
        if (s_entity_cfg[i].configured) {
            uint8_t count = s_alive_counts[i];
            if (count < s_entity_cfg[i].min_alive || count > s_entity_cfg[i].max_alive) {
                all_entities_healthy = false;
            }
        }
    }

    if (all_entities_healthy) {
        s_global_status = WDGM_STATUS_OK;
        s_reload_success_count++;

        if (s_feed_hook != NULL) {
            s_feed_hook();
        } else {
            wdgm_hal_feed_iwdg();
        }
    } else {
        /* Một hoặc nhiều tác vụ không đạt checkpoint (deadlock/treo) */
        s_global_status = WDGM_STATUS_EXPIRED;
        /* KHÔNG gọi feed -> để phần cứng Watchdog tự timeout reset MCU */
    }

    /* Đặt lại bộ đếm cho chu kỳ 100ms tiếp theo */
    for (uint8_t i = 0; i < WDGM_MAX_ENTITIES; i++) {
        s_alive_counts[i] = 0;
    }
}

wdgm_status_t WdgM_GetGlobalStatus(void)
{
    return s_global_status;
}

uint32_t WdgM_GetSuccessReloadCount(void)
{
    return s_reload_success_count;
}

void WdgM_SetFeedHook(wdgm_feed_hook_fn_t hook)
{
    s_feed_hook = hook;
}
