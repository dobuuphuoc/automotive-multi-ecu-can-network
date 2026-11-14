/* ==============================================================================
 * diag_dem.c — AUTOSAR DEM Implementation
 * ============================================================================== */
#include "diag_dem.h"
#include <string.h>

static dem_dtc_slot_t     s_dtc_slots[DEM_MAX_DTC_SLOTS];
static dem_freeze_frame_t s_freeze_frame;

void Dem_Init(void)
{
    memset(s_dtc_slots, 0, sizeof(s_dtc_slots));
    memset(&s_freeze_frame, 0, sizeof(s_freeze_frame));
}

static dem_dtc_slot_t *find_or_allocate_slot(uint32_t dtc_id)
{
    dem_dtc_slot_t *free_slot = NULL;

    for (uint8_t i = 0; i < DEM_MAX_DTC_SLOTS; i++) {
        if (s_dtc_slots[i].dtc_id == dtc_id) {
            return &s_dtc_slots[i];
        }
        if (s_dtc_slots[i].dtc_id == 0 && free_slot == NULL) {
            free_slot = &s_dtc_slots[i];
        }
    }
    if (free_slot != NULL) {
        free_slot->dtc_id = dtc_id;
    }
    return free_slot;
}


void Dem_ReportErrorStatus(uint32_t dtc_id, dem_event_status_t status)
{
    dem_dtc_slot_t *slot = find_or_allocate_slot(dtc_id);
    if (slot == NULL) {
        return; /* Không còn khe trống */
    }

    slot->dtc_id = dtc_id;

    if (status == DEM_EVENT_STATUS_FAILED) {
        slot->fail_count++;
        slot->pass_count = 0;
        /* 3 lỗi liên tiếp -> Active */
        if (slot->fail_count >= 3) {
            slot->status   = DEM_DTC_STATUS_FAILED;
            slot->recorded = true;
        }
    } else {
        slot->pass_count++;
        slot->fail_count = 0;
        /* 3 lần tốt liên tiếp -> Hết lỗi (Healed) */
        if (slot->pass_count >= 3) {
            if (slot->status == DEM_DTC_STATUS_FAILED) {
                slot->status = DEM_DTC_STATUS_PASSED;
            }
        }
    }
}

void Dem_ClearAllDtc(void)
{
    memset(s_dtc_slots, 0, sizeof(s_dtc_slots));
    memset(&s_freeze_frame, 0, sizeof(s_freeze_frame));
}

uint8_t Dem_GetDtcStatus(uint32_t dtc_id)
{
    for (uint8_t i = 0; i < DEM_MAX_DTC_SLOTS; i++) {
        if (s_dtc_slots[i].recorded && s_dtc_slots[i].dtc_id == dtc_id) {
            return s_dtc_slots[i].status;
        }
    }
    return DEM_DTC_STATUS_PASSED;
}

bool Dem_IsDtcActive(uint32_t dtc_id)
{
    return (Dem_GetDtcStatus(dtc_id) == DEM_DTC_STATUS_FAILED);
}

uint8_t Dem_GetDtcCount(uint8_t status_mask)
{
    uint8_t count = 0;
    for (uint8_t i = 0; i < DEM_MAX_DTC_SLOTS; i++) {
        if (s_dtc_slots[i].recorded) {
            if ((s_dtc_slots[i].status & status_mask) != 0 ||
                (status_mask == 0xFFu && s_dtc_slots[i].status == DEM_DTC_STATUS_PASSED) ||
                (status_mask == 0x00u && s_dtc_slots[i].status == DEM_DTC_STATUS_PASSED)) {
                count++;
            }
        }
    }
    return count;
}

bool Dem_GetFirstMatchingDtc(uint8_t status_mask, uint32_t *dtc_id, uint8_t *status)
{
    for (uint8_t i = 0; i < DEM_MAX_DTC_SLOTS; i++) {
        if (s_dtc_slots[i].recorded) {
            bool matches = false;
            if (status_mask == 0xFFu) {
                matches = true;
            } else if ((s_dtc_slots[i].status & status_mask) != 0) {
                matches = true;
            } else if (status_mask == 0x00u && s_dtc_slots[i].status == DEM_DTC_STATUS_PASSED) {
                matches = true;
            }

            if (matches) {
                if (dtc_id != NULL) *dtc_id = s_dtc_slots[i].dtc_id;
                if (status != NULL) *status = s_dtc_slots[i].status;
                return true;
            }
        }
    }
    return false;
}

void Dem_CaptureFreezeFrame(uint16_t wheel_speed_raw, uint8_t slip_pct, uint8_t decel_rpm_100ms)
{
    s_freeze_frame.wheel_speed_raw = wheel_speed_raw;
    s_freeze_frame.slip_percent    = slip_pct;
    s_freeze_frame.decel_rpm_100ms = decel_rpm_100ms;
    s_freeze_frame.is_valid        = true;
}

bool Dem_GetFreezeFrameData(uint8_t buffer[4])
{
    if (buffer == NULL) return false;

    if (!s_freeze_frame.is_valid) {
        memset(buffer, 0, 4);
        return false;
    }

    /* Đóng gói big-endian theo chuẩn UDS và spec */
    buffer[0] = (uint8_t)((s_freeze_frame.wheel_speed_raw >> 8) & 0xFFu);
    buffer[1] = (uint8_t)(s_freeze_frame.wheel_speed_raw & 0xFFu);
    buffer[2] = s_freeze_frame.slip_percent;
    buffer[3] = s_freeze_frame.decel_rpm_100ms;
    return true;
}

bool Dem_IsFreezeFrameValid(void)
{
    return s_freeze_frame.is_valid;
}
