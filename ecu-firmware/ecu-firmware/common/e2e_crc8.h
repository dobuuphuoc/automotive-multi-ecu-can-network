/* CRC-8/SAE-J1850: poly 0x1D, init 0xFF, xorout 0xFF, không phản xạ bit.
 * Tuân thủ AUTOSAR E2E Profile 1 (Alive Counter 4-bit, DataID, CRC-8, 3-strike debounce).
 * Khớp với e2e_reference.py: frame_crc = crc8([data_id] + payload[0..n-2]). */
#ifndef E2E_CRC8_H
#define E2E_CRC8_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Tương thích ngược với mã bring-up và test_crc8 */
uint8_t e2e_crc8(const uint8_t *data, size_t len);
/* CRC của một frame: tính trên [data_id] + payload (không gồm byte CRC cuối). */
uint8_t e2e_frame_crc(uint8_t data_id, const uint8_t *payload, size_t payload_len_without_crc);

/* Trạng thái kiểm tra E2E Profile 1 tại bên nhận */
typedef enum {
    E2E_STATUS_INITIAL = 0,     /* Frame hợp lệ đầu tiên */
    E2E_STATUS_OK,              /* CRC đúng, delta == 1 */
    E2E_STATUS_OK_SOME_LOST,    /* CRC đúng, delta == 2..max_delta */
    E2E_STATUS_REPEATED,        /* CRC đúng, delta == 0 (lặp hoặc replay) */
    E2E_STATUS_WRONG_SEQUENCE,  /* CRC đúng, delta > max_delta */
    E2E_STATUS_WRONG_CRC,       /* CRC sai */
    E2E_STATUS_WRONG_LENGTH,    /* DLC không khớp cấu hình */
    E2E_STATUS_NONEWDATA        /* Quá hạn deadline (timeout) không có dữ liệu */
} e2e_status_t;

/* Trạng thái kênh truyền E2E (Bộ lọc debounce 3-strike) */
typedef enum {
    E2E_STATE_VALID = 0,
    E2E_STATE_INVALID = 1
} e2e_state_t;

/* Cấu hình E2E cho từng loại Message ID */
typedef struct {
    uint8_t data_id;            /* Hằng số 1 byte DataID duy nhất */
    uint8_t dlc;                /* Chiều dài frame cố định */
    uint8_t max_delta_counter;  /* Bước nhảy counter tối đa cho phép (mặc định 3) */
} e2e_config_t;

/* Trạng thái phát E2E */
typedef struct {
    uint8_t alive_counter;      /* Bộ đếm sống 4-bit (0..15) */
} e2e_tx_state_t;

/* Trạng thái thu E2E (lưu trữ lịch sử & debounce) */
typedef struct {
    uint8_t last_counter;       /* Counter hợp lệ gần nhất */
    bool initialized;           /* Đã nhận được frame hợp lệ đầu tiên */
    uint8_t err_count;          /* Đếm số lần lỗi liên tiếp (>=3 -> INVALID) */
    uint8_t ok_count;           /* Đếm số lần hợp lệ liên tiếp (>=3 -> VALID) */
    e2e_state_t state;          /* Trạng thái kênh hiện tại */
    e2e_status_t last_status;   /* Trạng thái kiểm tra của frame gần nhất */
} e2e_rx_state_t;

/* Khởi tạo trạng thái phát */
void e2e_tx_init(e2e_tx_state_t *state);

/* Đóng gói bảo vệ E2E chuẩn: ghi counter vào byte 0 [3:0], tính CRC ghi vào byte cuối, tăng counter */
uint8_t e2e_protect(const e2e_config_t *cfg, e2e_tx_state_t *state, uint8_t *payload, uint8_t len);

/* Khởi tạo trạng thái thu */
void e2e_rx_init(e2e_rx_state_t *state);

/* Kiểm tra E2E với counter nằm ở byte 0 [3:0] */
e2e_status_t e2e_check(const e2e_config_t *cfg, e2e_rx_state_t *state, const uint8_t *payload, uint8_t len);

/* Kiểm tra E2E với counter được trích xuất tường minh theo DBC */
e2e_status_t e2e_check_counter(const e2e_config_t *cfg, e2e_rx_state_t *state,
                              const uint8_t *payload, uint8_t len, uint8_t counter);

/* Thông báo mất frame quá thời hạn (timeout) */
void e2e_notify_timeout(e2e_rx_state_t *state);

#ifdef __cplusplus
}
#endif

#endif /* E2E_CRC8_H */
