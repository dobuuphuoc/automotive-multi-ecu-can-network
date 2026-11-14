/* ==============================================================================
 * diag_dcm.h — AUTOSAR DCM (Diagnostic Communication Manager)
 * Chuẩn UDS (ISO 14229) trên ISO-TP Single-Frame CAN.
 * Khớp chuẩn can_matrix.dbc và doc/CAN-byte-layout.md Mục 7.
 * ============================================================================== */
#ifndef DIAG_DCM_H
#define DIAG_DCM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "diag_dem.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------------------
 * Node ID và Định tuyến CAN ID chẩn đoán (Physical & Functional)
 * ------------------------------------------------------------------------------ */
typedef enum {
    ECU_NODE_BCM     = 0,  /* Req 0x7E0, Resp 0x7E8 */
    ECU_NODE_WINDOW  = 1,  /* Req 0x7E1, Resp 0x7E9 */
    ECU_NODE_CLIMATE = 2,  /* Req 0x7E2, Resp 0x7EA */
    ECU_NODE_WSW     = 3,  /* Req 0x7E3, Resp 0x7EB */
    ECU_NODE_DMS     = 4   /* Req 0x7E4, Resp 0x7EC */
} ecu_node_id_t;

#define DCM_FUNCTIONAL_REQ_ID   0x7DFu
#define DCM_PHYSICAL_REQ_BASE   0x7E0u
#define DCM_PHYSICAL_RESP_BASE  0x7E8u

/* ------------------------------------------------------------------------------
 * Diagnostic Sessions & DIDs
 * ------------------------------------------------------------------------------ */
typedef enum {
    DCM_SESSION_DEFAULT  = 0x01,
    DCM_SESSION_EXTENDED = 0x03
} dcm_session_t;

#define DCM_DID_FIRMWARE_VERSION  0x0100u  /* 2 byte: major, minor */
#define DCM_DID_UPTIME            0x0101u  /* 4 byte: giây, big-endian */
#define DCM_DID_CAN_TX_COUNT      0x0102u  /* 4 byte: số frame đã gửi, big-endian */
#define DCM_DID_CAN_ERROR_COUNT   0x0103u  /* 2 byte: TEC, REC */
#define DCM_DID_PRIMARY_SIGNAL    0x0110u  /* 2 byte: Tín hiệu chính của từng ECU */
#define DCM_DID_FREEZE_FRAME      0x0120u  /* 4 byte: Freeze Frame WSW */

/* ------------------------------------------------------------------------------
 * Negative Response Codes (NRC)
 * ------------------------------------------------------------------------------ */
#define DCM_NRC_SERVICE_NOT_SUPPORTED       0x11u
#define DCM_NRC_SUBFUNCTION_NOT_SUPPORTED   0x12u
#define DCM_NRC_INCORRECT_MESSAGE_LENGTH   0x13u
#define DCM_NRC_REQUEST_OUT_OF_RANGE        0x31u

/* Callbacks lấy dữ liệu vi điều khiển */
typedef uint16_t (*dcm_primary_signal_fn_t)(void);
typedef uint32_t (*dcm_uptime_fn_t)(void);
typedef uint32_t (*dcm_tx_count_fn_t)(void);
typedef void (*dcm_can_errors_fn_t)(uint8_t *tec, uint8_t *rec);

/* ------------------------------------------------------------------------------
 * API DCM
 * ------------------------------------------------------------------------------ */
void Dcm_Init(ecu_node_id_t node_id);
void Dcm_SetFirmwareVersion(uint8_t major, uint8_t minor);
dcm_session_t Dcm_GetCurrentSession(void);

void Dcm_RegisterPrimarySignalGetter(dcm_primary_signal_fn_t fn);
void Dcm_RegisterUptimeGetter(dcm_uptime_fn_t fn);
void Dcm_RegisterTxCountGetter(dcm_tx_count_fn_t fn);
void Dcm_RegisterCanErrorsGetter(dcm_can_errors_fn_t fn);

/* Xử lý 1 frame CAN đến. Trả về true nếu frame thuộc về chẩn đoán và đã sinh frame phản hồi */
bool Dcm_ProcessCanFrame(uint32_t rx_id, const uint8_t *rx_data, uint8_t rx_dlc,
                         uint32_t *tx_id, uint8_t *tx_data, uint8_t *tx_dlc);

#ifdef __cplusplus
}
#endif

#endif /* DIAG_DCM_H */
