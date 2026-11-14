/* ==============================================================================
 * diag_dcm.c — AUTOSAR DCM Implementation
 * ============================================================================== */
#include "diag_dcm.h"
#include <string.h>

static ecu_node_id_t s_node_id = ECU_NODE_BCM;
static dcm_session_t s_session = DCM_SESSION_DEFAULT;
static uint8_t s_ver_major = 1;
static uint8_t s_ver_minor = 0;

static dcm_primary_signal_fn_t s_fn_primary_signal = NULL;
static dcm_uptime_fn_t         s_fn_uptime         = NULL;
static dcm_tx_count_fn_t       s_fn_tx_count       = NULL;
static dcm_can_errors_fn_t     s_fn_can_errors     = NULL;

void Dcm_Init(ecu_node_id_t node_id)
{
    s_node_id = node_id;
    s_session = DCM_SESSION_DEFAULT;
    s_ver_major = 1;
    s_ver_minor = 0;
    s_fn_primary_signal = NULL;
    s_fn_uptime = NULL;
    s_fn_tx_count = NULL;
    s_fn_can_errors = NULL;
}

void Dcm_SetFirmwareVersion(uint8_t major, uint8_t minor)
{
    s_ver_major = major;
    s_ver_minor = minor;
}

dcm_session_t Dcm_GetCurrentSession(void)
{
    return s_session;
}

void Dcm_RegisterPrimarySignalGetter(dcm_primary_signal_fn_t fn)
{
    s_fn_primary_signal = fn;
}

void Dcm_RegisterUptimeGetter(dcm_uptime_fn_t fn)
{
    s_fn_uptime = fn;
}

void Dcm_RegisterTxCountGetter(dcm_tx_count_fn_t fn)
{
    s_fn_tx_count = fn;
}

void Dcm_RegisterCanErrorsGetter(dcm_can_errors_fn_t fn)
{
    s_fn_can_errors = fn;
}

static void make_negative_response(uint8_t sid, uint8_t nrc, uint8_t *tx_data)
{
    memset(tx_data, 0, 8);
    tx_data[0] = 0x03; /* PCI len = 3 */
    tx_data[1] = 0x7F;
    tx_data[2] = sid;
    tx_data[3] = nrc;
}

bool Dcm_ProcessCanFrame(uint32_t rx_id, const uint8_t *rx_data, uint8_t rx_dlc,
                         uint32_t *tx_id, uint8_t *tx_data, uint8_t *tx_dlc)
{
    uint32_t my_phys_req = DCM_PHYSICAL_REQ_BASE + (uint32_t)s_node_id;
    uint32_t my_phys_resp = DCM_PHYSICAL_RESP_BASE + (uint32_t)s_node_id;

    /* Chỉ tiếp nhận Frame tới Functional ID (0x7DF) hoặc Physical ID của ECU này */
    if (rx_id != DCM_FUNCTIONAL_REQ_ID && rx_id != my_phys_req) {
        return false;
    }

    if (rx_data == NULL || rx_dlc < 2 || tx_id == NULL || tx_data == NULL || tx_dlc == NULL) {
        return false;
    }

    uint8_t pci_len = rx_data[0] & 0x0Fu;
    if (pci_len == 0 || pci_len > 7 || (uint8_t)(pci_len + 1) > rx_dlc) {
        return false;
    }

    uint8_t sid = rx_data[1];
    memset(tx_data, 0, 8);
    *tx_id = my_phys_resp;
    *tx_dlc = 8;

    switch (sid) {
    /* ---------------- 0x10 DiagnosticSessionControl ---------------- */
    case 0x10: {
        if (pci_len < 2) {
            make_negative_response(sid, DCM_NRC_INCORRECT_MESSAGE_LENGTH, tx_data);
            return true;
        }
        uint8_t sub = rx_data[2];
        if (sub == (uint8_t)DCM_SESSION_DEFAULT || sub == (uint8_t)DCM_SESSION_EXTENDED) {
            s_session = (dcm_session_t)sub;
            tx_data[0] = 0x02; /* PCI length */
            tx_data[1] = 0x50; /* Positive resp SID */
            tx_data[2] = sub;
        } else {
            make_negative_response(sid, DCM_NRC_SUBFUNCTION_NOT_SUPPORTED, tx_data);
        }
        return true;
    }

    /* ---------------- 0x14 ClearDiagnosticInformation ---------------- */
    case 0x14: {
        /* Xóa toàn bộ DTC trong DEM và Freeze Frame */
        Dem_ClearAllDtc();
        tx_data[0] = 0x01; /* PCI length */
        tx_data[1] = 0x54; /* Positive resp SID */
        return true;
    }

    /* ---------------- 0x19 ReadDTCInformation ---------------- */
    case 0x19: {
        if (pci_len < 3) {
            make_negative_response(sid, DCM_NRC_INCORRECT_MESSAGE_LENGTH, tx_data);
            return true;
        }
        uint8_t sub = rx_data[2];
        if (sub != 0x02) {
            /* Hệ thống chỉ hỗ trợ sub-function 0x02 reportDTCByStatusMask */
            make_negative_response(sid, DCM_NRC_SUBFUNCTION_NOT_SUPPORTED, tx_data);
            return true;
        }
        uint8_t mask = rx_data[3];
        uint32_t dtc_id = 0;
        uint8_t status = 0;
        bool found = Dem_GetFirstMatchingDtc(mask, &dtc_id, &status);

        if (found) {
            /* 7E8#075902<mask=FF><DTC_H><DTC_M><DTC_L><status> */
            tx_data[0] = 0x07;
            tx_data[1] = 0x59;
            tx_data[2] = 0x02;
            tx_data[3] = mask;
            tx_data[4] = (uint8_t)((dtc_id >> 16) & 0xFFu);
            tx_data[5] = (uint8_t)((dtc_id >> 8) & 0xFFu);
            tx_data[6] = (uint8_t)(dtc_id & 0xFFu);
            tx_data[7] = status;
        } else {
            /* Không có lỗi khớp mask: PCI = 3: 59 02 mask */
            tx_data[0] = 0x03;
            tx_data[1] = 0x59;
            tx_data[2] = 0x02;
            tx_data[3] = mask;
        }
        return true;
    }

    /* ---------------- 0x22 ReadDataByIdentifier ---------------- */
    case 0x22: {
        if (pci_len < 3) {
            make_negative_response(sid, DCM_NRC_INCORRECT_MESSAGE_LENGTH, tx_data);
            return true;
        }
        uint16_t did = (uint16_t)(((uint16_t)rx_data[2] << 8) | (uint16_t)rx_data[3]);

        switch (did) {
        case DCM_DID_FIRMWARE_VERSION: {
            tx_data[0] = 0x05;
            tx_data[1] = 0x62;
            tx_data[2] = 0x01;
            tx_data[3] = 0x00;
            tx_data[4] = s_ver_major;
            tx_data[5] = s_ver_minor;
            return true;
        }
        case DCM_DID_UPTIME: {
            uint32_t uptime = (s_fn_uptime != NULL) ? s_fn_uptime() : 0u;
            tx_data[0] = 0x07;
            tx_data[1] = 0x62;
            tx_data[2] = 0x01;
            tx_data[3] = 0x01;
            tx_data[4] = (uint8_t)((uptime >> 24) & 0xFFu);
            tx_data[5] = (uint8_t)((uptime >> 16) & 0xFFu);
            tx_data[6] = (uint8_t)((uptime >> 8) & 0xFFu);
            tx_data[7] = (uint8_t)(uptime & 0xFFu);
            return true;
        }
        case DCM_DID_CAN_TX_COUNT: {
            uint32_t tx_cnt = (s_fn_tx_count != NULL) ? s_fn_tx_count() : 0u;
            tx_data[0] = 0x07;
            tx_data[1] = 0x62;
            tx_data[2] = 0x01;
            tx_data[3] = 0x02;
            tx_data[4] = (uint8_t)((tx_cnt >> 24) & 0xFFu);
            tx_data[5] = (uint8_t)((tx_cnt >> 16) & 0xFFu);
            tx_data[6] = (uint8_t)((tx_cnt >> 8) & 0xFFu);
            tx_data[7] = (uint8_t)(tx_cnt & 0xFFu);
            return true;
        }
        case DCM_DID_CAN_ERROR_COUNT: {
            uint8_t tec = 0, rec = 0;
            if (s_fn_can_errors != NULL) {
                s_fn_can_errors(&tec, &rec);
            }
            tx_data[0] = 0x05;
            tx_data[1] = 0x62;
            tx_data[2] = 0x01;
            tx_data[3] = 0x03;
            tx_data[4] = tec;
            tx_data[5] = rec;
            return true;
        }
        case DCM_DID_PRIMARY_SIGNAL: {
            uint16_t val = (s_fn_primary_signal != NULL) ? s_fn_primary_signal() : 0u;
            tx_data[0] = 0x05;
            tx_data[1] = 0x62;
            tx_data[2] = 0x01;
            tx_data[3] = 0x10;
            tx_data[4] = (uint8_t)((val >> 8) & 0xFFu);
            tx_data[5] = (uint8_t)(val & 0xFFu);
            return true;
        }
        case DCM_DID_FREEZE_FRAME: {
            uint8_t ff_buf[4];
            (void)Dem_GetFreezeFrameData(ff_buf);
            tx_data[0] = 0x07;
            tx_data[1] = 0x62;
            tx_data[2] = 0x01;
            tx_data[3] = 0x20;
            tx_data[4] = ff_buf[0];
            tx_data[5] = ff_buf[1];
            tx_data[6] = ff_buf[2];
            tx_data[7] = ff_buf[3];
            return true;
        }
        default:
            make_negative_response(sid, DCM_NRC_REQUEST_OUT_OF_RANGE, tx_data);
            return true;
        }
    }

    /* ---------------- Dịch vụ không hỗ trợ ---------------- */
    default:
        make_negative_response(sid, DCM_NRC_SERVICE_NOT_SUPPORTED, tx_data);
        return true;
    }
}
