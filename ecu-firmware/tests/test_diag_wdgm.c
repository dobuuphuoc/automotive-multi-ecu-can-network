#include "unity.h"
#include "diag_dem.h"
#include "diag_dcm.h"
#include "wdgm.h"
#include <string.h>

void setUp(void)
{
    Dem_Init();
    Dcm_Init(ECU_NODE_WSW);
    WdgM_Init();
}

void tearDown(void) {}

/* ------------------------------------------------------------------------------
 * DEM Tests
 * ------------------------------------------------------------------------------ */
static void test_dem_3_strike_debounce_and_healing(void)
{
    /* Bắt đầu: không có lỗi nào active */
    TEST_ASSERT_EQUAL_UINT8(DEM_DTC_STATUS_PASSED, Dem_GetDtcStatus(DEM_DTC_E2E_ERROR));
    TEST_ASSERT_FALSE(Dem_IsDtcActive(DEM_DTC_E2E_ERROR));

    /* Báo lỗi lần 1 -> chưa active */
    Dem_ReportErrorStatus(DEM_DTC_E2E_ERROR, DEM_EVENT_STATUS_FAILED);
    TEST_ASSERT_FALSE(Dem_IsDtcActive(DEM_DTC_E2E_ERROR));

    /* Báo lỗi lần 2 -> chưa active */
    Dem_ReportErrorStatus(DEM_DTC_E2E_ERROR, DEM_EVENT_STATUS_FAILED);
    TEST_ASSERT_FALSE(Dem_IsDtcActive(DEM_DTC_E2E_ERROR));

    /* Báo lỗi lần 3 -> Chạm ngưỡng 3-strike: Active! */
    Dem_ReportErrorStatus(DEM_DTC_E2E_ERROR, DEM_EVENT_STATUS_FAILED);
    TEST_ASSERT_TRUE(Dem_IsDtcActive(DEM_DTC_E2E_ERROR));
    TEST_ASSERT_EQUAL_UINT8(DEM_DTC_STATUS_FAILED, Dem_GetDtcStatus(DEM_DTC_E2E_ERROR));

    /* Quá trình phục hồi (healing): cần 3 lần Passed liên tiếp */
    Dem_ReportErrorStatus(DEM_DTC_E2E_ERROR, DEM_EVENT_STATUS_PASSED);
    TEST_ASSERT_TRUE(Dem_IsDtcActive(DEM_DTC_E2E_ERROR)); /* Vẫn active */

    Dem_ReportErrorStatus(DEM_DTC_E2E_ERROR, DEM_EVENT_STATUS_PASSED);
    TEST_ASSERT_TRUE(Dem_IsDtcActive(DEM_DTC_E2E_ERROR)); /* Vẫn active */

    Dem_ReportErrorStatus(DEM_DTC_E2E_ERROR, DEM_EVENT_STATUS_PASSED);
    TEST_ASSERT_FALSE(Dem_IsDtcActive(DEM_DTC_E2E_ERROR)); /* Đã hết lỗi! status = 0x00 */
    TEST_ASSERT_EQUAL_UINT8(DEM_DTC_STATUS_PASSED, Dem_GetDtcStatus(DEM_DTC_E2E_ERROR));

    /* Nhưng DTC vẫn được ghi nhận trong lịch sử cho tới khi xóa bằng 0x14 */
    uint32_t dtc_id = 0;
    uint8_t status = 0xFF;
    TEST_ASSERT_TRUE(Dem_GetFirstMatchingDtc(0xFF, &dtc_id, &status));
    TEST_ASSERT_EQUAL_HEX32(DEM_DTC_E2E_ERROR, dtc_id);
    TEST_ASSERT_EQUAL_HEX8(DEM_DTC_STATUS_PASSED, status);

    /* Xóa lỗi */
    Dem_ClearAllDtc();
    TEST_ASSERT_FALSE(Dem_GetFirstMatchingDtc(0xFF, &dtc_id, &status));
}

static void test_dem_freeze_frame_capture(void)
{
    TEST_ASSERT_FALSE(Dem_IsFreezeFrameValid());

    /* Bắt freeze frame khi có sự kiện trượt: tốc độ 45.0 rpm (raw 450), trượt 60%, giảm tốc 20 rpm/100ms */
    Dem_CaptureFreezeFrame(450, 60, 20);
    TEST_ASSERT_TRUE(Dem_IsFreezeFrameValid());

    uint8_t buf[4] = {0};
    TEST_ASSERT_TRUE(Dem_GetFreezeFrameData(buf));
    /* Byte 0-1: 450 = 0x01C2 (big-endian) */
    TEST_ASSERT_EQUAL_HEX8(0x01, buf[0]);
    TEST_ASSERT_EQUAL_HEX8(0xC2, buf[1]);
    /* Byte 2: slip = 60 */
    TEST_ASSERT_EQUAL_UINT8(60, buf[2]);
    /* Byte 3: decel = 20 */
    TEST_ASSERT_EQUAL_UINT8(20, buf[3]);

    /* Dem_ClearAllDtc phải xóa luôn Freeze Frame */
    Dem_ClearAllDtc();
    TEST_ASSERT_FALSE(Dem_IsFreezeFrameValid());
}

/* ------------------------------------------------------------------------------
 * DCM UDS Tests
 * ------------------------------------------------------------------------------ */
static uint16_t dummy_primary_signal(void)
{
    return 0x1234;
}

static uint32_t dummy_uptime(void)
{
    return 1000; /* 1000 giây */
}

static void test_dcm_session_control_and_nrc(void)
{
    uint32_t tx_id = 0;
    uint8_t tx_data[8] = {0};
    uint8_t tx_dlc = 0;

    /* 0x10 Sub 0x01 (Default session) */
    const uint8_t req_default[8] = { 0x02, 0x10, 0x01, 0, 0, 0, 0, 0 };
    TEST_ASSERT_TRUE(Dcm_ProcessCanFrame(0x7DF, req_default, 8, &tx_id, tx_data, &tx_dlc));
    TEST_ASSERT_EQUAL_HEX32(0x7EB, tx_id); /* WSW Resp ID */
    TEST_ASSERT_EQUAL_HEX8(0x02, tx_data[0]); /* PCI len */
    TEST_ASSERT_EQUAL_HEX8(0x50, tx_data[1]); /* Positive resp */
    TEST_ASSERT_EQUAL_HEX8(0x01, tx_data[2]);
    TEST_ASSERT_EQUAL_INT(DCM_SESSION_DEFAULT, Dcm_GetCurrentSession());

    /* 0x10 Sub 0x03 (Extended diagnostic session) */
    const uint8_t req_extended[8] = { 0x02, 0x10, 0x03, 0, 0, 0, 0, 0 };
    TEST_ASSERT_TRUE(Dcm_ProcessCanFrame(0x7E3, req_extended, 8, &tx_id, tx_data, &tx_dlc));
    TEST_ASSERT_EQUAL_HEX8(0x50, tx_data[1]);
    TEST_ASSERT_EQUAL_HEX8(0x03, tx_data[2]);
    TEST_ASSERT_EQUAL_INT(DCM_SESSION_EXTENDED, Dcm_GetCurrentSession());

    /* 0x10 Sub 0x05 (Không hỗ trợ -> NRC 0x12) */
    const uint8_t req_invalid_sub[8] = { 0x02, 0x10, 0x05, 0, 0, 0, 0, 0 };
    TEST_ASSERT_TRUE(Dcm_ProcessCanFrame(0x7E3, req_invalid_sub, 8, &tx_id, tx_data, &tx_dlc));
    TEST_ASSERT_EQUAL_HEX8(0x03, tx_data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x7F, tx_data[1]);
    TEST_ASSERT_EQUAL_HEX8(0x10, tx_data[2]);
    TEST_ASSERT_EQUAL_HEX8(DCM_NRC_SUBFUNCTION_NOT_SUPPORTED, tx_data[3]);
}

static void test_dcm_read_data_by_identifier(void)
{
    Dcm_SetFirmwareVersion(1, 2);
    Dcm_RegisterPrimarySignalGetter(dummy_primary_signal);
    Dcm_RegisterUptimeGetter(dummy_uptime);

    uint32_t tx_id = 0;
    uint8_t tx_data[8] = {0};
    uint8_t tx_dlc = 0;

    /* DID 0x0100: Firmware Version -> 7E8#056201000102 (khớp ví dụ doc:539) */
    const uint8_t req_ver[8] = { 0x03, 0x22, 0x01, 0x00, 0, 0, 0, 0 };
    TEST_ASSERT_TRUE(Dcm_ProcessCanFrame(0x7E3, req_ver, 8, &tx_id, tx_data, &tx_dlc));
    TEST_ASSERT_EQUAL_HEX8(0x05, tx_data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x62, tx_data[1]);
    TEST_ASSERT_EQUAL_HEX8(0x01, tx_data[2]);
    TEST_ASSERT_EQUAL_HEX8(0x00, tx_data[3]);
    TEST_ASSERT_EQUAL_HEX8(0x01, tx_data[4]); /* Major = 1 */
    TEST_ASSERT_EQUAL_HEX8(0x02, tx_data[5]); /* Minor = 2 */

    /* DID 0x0110: Primary signal -> 0x1234 */
    const uint8_t req_sig[8] = { 0x03, 0x22, 0x01, 0x10, 0, 0, 0, 0 };
    TEST_ASSERT_TRUE(Dcm_ProcessCanFrame(0x7E3, req_sig, 8, &tx_id, tx_data, &tx_dlc));
    TEST_ASSERT_EQUAL_HEX8(0x05, tx_data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x62, tx_data[1]);
    TEST_ASSERT_EQUAL_HEX8(0x12, tx_data[4]);
    TEST_ASSERT_EQUAL_HEX8(0x34, tx_data[5]);

    /* DID 0x0120: Freeze Frame */
    Dem_CaptureFreezeFrame(500, 80, 30);
    const uint8_t req_ff[8] = { 0x03, 0x22, 0x01, 0x20, 0, 0, 0, 0 };
    TEST_ASSERT_TRUE(Dcm_ProcessCanFrame(0x7E3, req_ff, 8, &tx_id, tx_data, &tx_dlc));
    TEST_ASSERT_EQUAL_HEX8(0x07, tx_data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x62, tx_data[1]);
    TEST_ASSERT_EQUAL_HEX8(0x01, tx_data[4]); /* 500 = 0x01F4 */
    TEST_ASSERT_EQUAL_HEX8(0xF4, tx_data[5]);
    TEST_ASSERT_EQUAL_UINT8(80, tx_data[6]);
    TEST_ASSERT_EQUAL_UINT8(30, tx_data[7]);

    /* DID không tồn tại (0x9999) -> NRC 0x31 (khớp ví dụ doc:542) */
    const uint8_t req_unknown[8] = { 0x03, 0x22, 0x99, 0x99, 0, 0, 0, 0 };
    TEST_ASSERT_TRUE(Dcm_ProcessCanFrame(0x7E3, req_unknown, 8, &tx_id, tx_data, &tx_dlc));
    TEST_ASSERT_EQUAL_HEX8(0x03, tx_data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x7F, tx_data[1]);
    TEST_ASSERT_EQUAL_HEX8(0x22, tx_data[2]);
    TEST_ASSERT_EQUAL_HEX8(DCM_NRC_REQUEST_OUT_OF_RANGE, tx_data[3]);
}

static void test_dcm_read_dtc_and_clear_dtc(void)
{
    uint32_t tx_id = 0;
    uint8_t tx_data[8] = {0};
    uint8_t tx_dlc = 0;

    /* Khi chưa có DTC: 0x19 0x02 0xFF -> 7EB#035902FF */
    const uint8_t req_dtc[8] = { 0x03, 0x19, 0x02, 0xFF, 0, 0, 0, 0 };
    TEST_ASSERT_TRUE(Dcm_ProcessCanFrame(0x7E3, req_dtc, 8, &tx_id, tx_data, &tx_dlc));
    TEST_ASSERT_EQUAL_HEX8(0x03, tx_data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x59, tx_data[1]);
    TEST_ASSERT_EQUAL_HEX8(0x02, tx_data[2]);
    TEST_ASSERT_EQUAL_HEX8(0xFF, tx_data[3]);

    /* Kích hoạt lỗi 0xB10001 (LDR Short/Open) 3 lần để active */
    for (int i = 0; i < 3; i++) {
        Dem_ReportErrorStatus(DEM_DTC_LDR_SHORT_OPEN, DEM_EVENT_STATUS_FAILED);
    }

    /* Đọc lại: khớp chính xác ví dụ doc:541: 7E8#075902FFB1000101 */
    TEST_ASSERT_TRUE(Dcm_ProcessCanFrame(0x7E3, req_dtc, 8, &tx_id, tx_data, &tx_dlc));
    TEST_ASSERT_EQUAL_HEX8(0x07, tx_data[0]); /* PCI len = 7 */
    TEST_ASSERT_EQUAL_HEX8(0x59, tx_data[1]);
    TEST_ASSERT_EQUAL_HEX8(0x02, tx_data[2]);
    TEST_ASSERT_EQUAL_HEX8(0xFF, tx_data[3]);
    TEST_ASSERT_EQUAL_HEX8(0xB1, tx_data[4]); /* DTC High */
    TEST_ASSERT_EQUAL_HEX8(0x00, tx_data[5]); /* DTC Mid */
    TEST_ASSERT_EQUAL_HEX8(0x01, tx_data[6]); /* DTC Low */
    TEST_ASSERT_EQUAL_HEX8(0x01, tx_data[7]); /* Status = 0x01 */

    /* Xóa DTC bằng UDS 0x14: 7E3#0414FFFFFF */
    const uint8_t req_clear[8] = { 0x04, 0x14, 0xFF, 0xFF, 0xFF, 0, 0, 0 };
    TEST_ASSERT_TRUE(Dcm_ProcessCanFrame(0x7E3, req_clear, 8, &tx_id, tx_data, &tx_dlc));
    TEST_ASSERT_EQUAL_HEX8(0x01, tx_data[0]); /* PCI len = 1 */
    TEST_ASSERT_EQUAL_HEX8(0x54, tx_data[1]); /* Positive resp 0x54 */

    /* Đọc lại DTC sau khi xóa -> không còn DTC nào */
    TEST_ASSERT_TRUE(Dcm_ProcessCanFrame(0x7E3, req_dtc, 8, &tx_id, tx_data, &tx_dlc));
    TEST_ASSERT_EQUAL_HEX8(0x03, tx_data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x59, tx_data[1]);
}

static void test_dcm_service_not_supported(void)
{
    uint32_t tx_id = 0;
    uint8_t tx_data[8] = {0};
    uint8_t tx_dlc = 0;

    /* Gửi SID không hỗ trợ (ví dụ 0x27 SecurityAccess) -> NRC 0x11 */
    const uint8_t req_sec[8] = { 0x02, 0x27, 0x01, 0, 0, 0, 0, 0 };
    TEST_ASSERT_TRUE(Dcm_ProcessCanFrame(0x7E3, req_sec, 8, &tx_id, tx_data, &tx_dlc));
    TEST_ASSERT_EQUAL_HEX8(0x03, tx_data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x7F, tx_data[1]);
    TEST_ASSERT_EQUAL_HEX8(0x27, tx_data[2]);
    TEST_ASSERT_EQUAL_HEX8(DCM_NRC_SERVICE_NOT_SUPPORTED, tx_data[3]);

    /* Frame gửi tới ID khác không thuộc ECU này (0x7E0 BCM) -> bỏ qua */
    TEST_ASSERT_FALSE(Dcm_ProcessCanFrame(0x7E0, req_sec, 8, &tx_id, tx_data, &tx_dlc));
}

/* ------------------------------------------------------------------------------
 * WdgM Tests (Alive Supervision & Thực nghiệm E3 Hang Simulation)
 * ------------------------------------------------------------------------------ */
static uint32_t s_hw_feed_count = 0;
static void test_feed_hook(void)
{
    s_hw_feed_count++;
}

static void test_wdgm_alive_supervision_normal_and_hang(void)
{
    s_hw_feed_count = 0;
    WdgM_SetFeedHook(test_feed_hook);

    /* Cấu hình 3 Supervised Entities theo kiến trúc:
     * - Task CAN RX (100ms): min 1, max 2
     * - Task Control (20ms): 5 lần trong 100ms -> min 4, max 6
     * - Task CAN TX / Diag (100ms): min 1, max 2 */
    TEST_ASSERT_TRUE(WdgM_ConfigureEntity(WDGM_SE_TASK_COM_RX, 1, 2));
    TEST_ASSERT_TRUE(WdgM_ConfigureEntity(WDGM_SE_TASK_CONTROL, 4, 6));
    TEST_ASSERT_TRUE(WdgM_ConfigureEntity(WDGM_SE_TASK_COM_TX_DIAG, 1, 2));

    /* Chu kỳ 1: Tất cả các task chạy bình thường */
    WdgM_CheckpointReached(WDGM_SE_TASK_COM_RX);
    for (int i = 0; i < 5; i++) {
        WdgM_CheckpointReached(WDGM_SE_TASK_CONTROL);
    }
    WdgM_CheckpointReached(WDGM_SE_TASK_COM_TX_DIAG);

    WdgM_MainFunction_100ms();
    TEST_ASSERT_EQUAL_INT(WDGM_STATUS_OK, WdgM_GetGlobalStatus());
    TEST_ASSERT_EQUAL_UINT32(1, WdgM_GetSuccessReloadCount());
    TEST_ASSERT_EQUAL_UINT32(1, s_hw_feed_count);

    /* Chu kỳ 2: Thực nghiệm E3 — Giả lập Task Control bị treo/deadlock (0 checkpoint) */
    WdgM_CheckpointReached(WDGM_SE_TASK_COM_RX);
    /* Task Control bị treo nên KHÔNG chạm checkpoint! */
    WdgM_CheckpointReached(WDGM_SE_TASK_COM_TX_DIAG);

    WdgM_MainFunction_100ms();
    /* WdgM phát hiện vi phạm -> Chuyển sang EXPIRED và NGỪNG nạp watchdog */
    TEST_ASSERT_EQUAL_INT(WDGM_STATUS_EXPIRED, WdgM_GetGlobalStatus());
    TEST_ASSERT_EQUAL_UINT32(1, WdgM_GetSuccessReloadCount()); /* Không tăng */
    TEST_ASSERT_EQUAL_UINT32(1, s_hw_feed_count);            /* Watchdog phần cứng không được nạp */

    /* Chu kỳ 3: Kể cả sau đó các task có gửi lại, trạng thái EXPIRED vẫn được giữ để MCU reset */
    WdgM_CheckpointReached(WDGM_SE_TASK_COM_RX);
    for (int i = 0; i < 5; i++) {
        WdgM_CheckpointReached(WDGM_SE_TASK_CONTROL);
    }
    WdgM_CheckpointReached(WDGM_SE_TASK_COM_TX_DIAG);

    WdgM_MainFunction_100ms();
    TEST_ASSERT_EQUAL_INT(WDGM_STATUS_EXPIRED, WdgM_GetGlobalStatus());
    TEST_ASSERT_EQUAL_UINT32(1, s_hw_feed_count);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_dem_3_strike_debounce_and_healing);
    RUN_TEST(test_dem_freeze_frame_capture);
    RUN_TEST(test_dcm_session_control_and_nrc);
    RUN_TEST(test_dcm_read_data_by_identifier);
    RUN_TEST(test_dcm_read_dtc_and_clear_dtc);
    RUN_TEST(test_dcm_service_not_supported);
    RUN_TEST(test_wdgm_alive_supervision_normal_and_hang);
    return UNITY_END();
}
