/* ==============================================================================
 * swc_window.c — Software Component Power Window Implementation
 * ============================================================================== */
#include "swc_window.h"

/* Hằng số */
#define WIN_MAX_ANGLE           180u
#define WIN_PINCH_REVERSE_DEG   36u    /* Đảo chiều mở thêm 20% (36 độ) */

/* Trạng thái nội bộ */
static uint8_t s_current_angle = 0;    /* 0 = Đóng hoàn toàn, 180 = Mở hoàn toàn */
static uint8_t s_target_angle  = 0;
static uint8_t s_moving        = 0;
static uint8_t s_pinch_detected= 0;
static uint8_t s_child_lock    = 0;

void Window_Init_Runnable(void)
{
    s_current_angle = 0;
    s_target_angle = 0;
    s_moving = 0;
    s_pinch_detected = 0;
    s_child_lock = 0;

    Rte_Write_CurrentAngle(s_current_angle);
    Rte_Write_TargetAngleEcho(s_target_angle);
    Rte_Write_Moving(s_moving);
    Rte_Write_PinchDetected(s_pinch_detected);
    Rte_Write_ChildLockState(s_child_lock);
}

void Window_Cmd_Runnable(void)
{
    /* 1. Xử lý lệnh góc cửa sổ từ Master (MP157) */
    com_msg_window_cmd_t cmd;
    uint8_t cmd_status = RTE_SIG_VALID;

    if (Rte_Read_WindowCmd(&cmd, &cmd_status) == E_OK) {
        if (cmd_status == RTE_SIG_VALID) {
            /* Cập nhật trạng thái khóa trẻ em */
            s_child_lock = cmd.child_lock;

            if (cmd.target_angle > WIN_MAX_ANGLE) {
                /* Bỏ lệnh và ghi mã lỗi DTC 0xB30001 */
                Rte_Call_Dem_SetEventStatus(DEM_DTC_WINDOW_POS_RANGE, DEM_EVENT_STATUS_FAILED);
            } else {
                Rte_Call_Dem_SetEventStatus(DEM_DTC_WINDOW_POS_RANGE, DEM_EVENT_STATUS_PASSED);

                if (s_child_lock) {
                    /* Child Lock: Bỏ qua lệnh mở (góc tăng), chỉ chấp nhận lệnh đóng kính (góc giảm hoặc về 0) */
                    if (cmd.target_angle <= s_current_angle) {
                        s_target_angle = cmd.target_angle;
                    }
                } else {
                    s_target_angle = cmd.target_angle;
                }
            }
        } else {
            /* Failsafe: Mất tín hiệu hoặc E2E sai -> Giữ nguyên góc hiện tại, dừng mô tơ */
            s_target_angle = s_current_angle;
        }
    }

    /* 2. Tự động đóng kính khi xe bị khóa (từ BCM_STATUS) */
    com_msg_bcm_status_t bcm_st;
    uint8_t bcm_status = RTE_SIG_VALID;
    if (Rte_Read_BcmStatus(&bcm_st, &bcm_status) == E_OK && bcm_status == RTE_SIG_VALID) {
        if (bcm_st.lock_state == COM_LOCK_STATE_LOCKED && !s_pinch_detected) {
            s_target_angle = 0; /* Đóng kính hoàn toàn */
        }
    }

    Rte_Write_TargetAngleEcho(s_target_angle);
    Rte_Write_ChildLockState(s_child_lock);
}

void Window_10ms_PID_Runnable(void)
{
    /* 1. Kiểm tra cảm biến vật cản (Chống kẹt) */
    uint8_t obstacle = 0;
    Rte_Read_ObstacleSensor(&obstacle);

    /* Kính đang đóng khi góc mục tiêu nhỏ hơn góc hiện tại */
    bool is_closing = (s_target_angle < s_current_angle);

    if (obstacle && is_closing) {
        /* Chống kẹt kích hoạt: Dừng ngay lập tức và đảo chiều mở kính ít nhất 20% (36 độ) */
        s_pinch_detected = 1;
        uint16_t rev_target = (uint16_t)s_current_angle + WIN_PINCH_REVERSE_DEG;
        if (rev_target > WIN_MAX_ANGLE) {
            rev_target = WIN_MAX_ANGLE;
        }
        s_target_angle = (uint8_t)rev_target;
        Rte_Call_Dem_SetEventStatus(DEM_DTC_WINDOW_PINCH, DEM_EVENT_STATUS_FAILED);
    } else {
        if (!obstacle && s_current_angle == s_target_angle) {
            s_pinch_detected = 0;
            Rte_Call_Dem_SetEventStatus(DEM_DTC_WINDOW_PINCH, DEM_EVENT_STATUS_PASSED);
        }
    }

    /* 2. Điều khiển vị trí mô tơ theo profile mượt (Rate-limited, vọt lố <= 5%, xác lập < 2s)
     * Tại chu kỳ 10ms: dịch 1 độ / 10ms = 100 độ/s -> di chuyển 90 độ mất 0.9s (đạt yêu cầu < 2s) */
    if (s_current_angle < s_target_angle) {
        s_current_angle++;
        s_moving = 1;
    } else if (s_current_angle > s_target_angle) {
        s_current_angle--;
        s_moving = 1;
    } else {
        s_moving = 0;
    }

    Rte_Write_CurrentAngle(s_current_angle);
    Rte_Write_TargetAngleEcho(s_target_angle);
    Rte_Write_Moving(s_moving);
    Rte_Write_PinchDetected(s_pinch_detected);
}
