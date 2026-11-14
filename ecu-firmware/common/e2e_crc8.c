#include "e2e_crc8.h"

#define CRC8_POLY   0x1Du
#define CRC8_INIT   0xFFu
#define CRC8_XOROUT 0xFFu

static uint8_t crc8_update(uint8_t crc, uint8_t byte)
{
    crc ^= byte;
    for (int i = 0; i < 8; i++) {
        crc = (crc & 0x80u) ? (uint8_t)((crc << 1) ^ CRC8_POLY) : (uint8_t)(crc << 1);
    }
    return crc;
}

uint8_t e2e_crc8(const uint8_t *data, size_t len)
{
    uint8_t crc = CRC8_INIT;
    for (size_t i = 0; i < len; i++) {
        crc = crc8_update(crc, data[i]);
    }
    return (uint8_t)(crc ^ CRC8_XOROUT);
}

uint8_t e2e_frame_crc(uint8_t data_id, const uint8_t *payload, size_t n)
{
    uint8_t crc = crc8_update(CRC8_INIT, data_id);
    for (size_t i = 0; i < n; i++) {
        crc = crc8_update(crc, payload[i]);
    }
    return (uint8_t)(crc ^ CRC8_XOROUT);
}

void e2e_tx_init(e2e_tx_state_t *state)
{
    if (state != NULL) {
        state->alive_counter = 0;
    }
}

uint8_t e2e_protect(const e2e_config_t *cfg, e2e_tx_state_t *state, uint8_t *payload, uint8_t len)
{
    if (cfg == NULL || state == NULL || payload == NULL || len < cfg->dlc || cfg->dlc < 2) {
        return 0;
    }

    /* Ghi alive_counter vào payload[0] & 0x0F, bảo toàn nibble cao payload[0] & 0xF0 */
    payload[0] = (uint8_t)((payload[0] & 0xF0u) | (state->alive_counter & 0x0Fu));

    /* Tính CRC-8 và ghi vào payload[cfg->dlc - 1] */
    uint8_t crc = e2e_frame_crc(cfg->data_id, payload, (size_t)(cfg->dlc - 1));
    payload[cfg->dlc - 1] = crc;

    /* Tăng counter quay vòng 0..15 */
    state->alive_counter = (uint8_t)((state->alive_counter + 1u) & 0x0Fu);
    return crc;
}

void e2e_rx_init(e2e_rx_state_t *state)
{
    if (state == NULL) {
        return;
    }
    state->last_counter = 0;
    state->initialized = false;
    state->err_count = 0;
    state->ok_count = 0;
    state->state = E2E_STATE_VALID;
    state->last_status = E2E_STATUS_NONEWDATA;
}

e2e_status_t e2e_check_counter(const e2e_config_t *cfg, e2e_rx_state_t *state,
                              const uint8_t *payload, uint8_t len, uint8_t counter)
{
    if (state == NULL || cfg == NULL) {
        return E2E_STATUS_WRONG_LENGTH;
    }

    /* Kiểm tra chiều dài frame */
    if (payload == NULL || len != cfg->dlc || cfg->dlc < 2) {
        state->last_status = E2E_STATUS_WRONG_LENGTH;
        state->err_count++;
        state->ok_count = 0;
        if (state->err_count >= 3) {
            state->state = E2E_STATE_INVALID;
        }
        return E2E_STATUS_WRONG_LENGTH;
    }

    /* Kiểm tra CRC-8 */
    uint8_t expected_crc = payload[cfg->dlc - 1];
    uint8_t calc_crc = e2e_frame_crc(cfg->data_id, payload, (size_t)(cfg->dlc - 1));
    if (calc_crc != expected_crc) {
        state->last_status = E2E_STATUS_WRONG_CRC;
        state->err_count++;
        state->ok_count = 0;
        if (state->err_count >= 3) {
            state->state = E2E_STATE_INVALID;
        }
        return E2E_STATUS_WRONG_CRC;
    }

    counter &= 0x0Fu;

    /* Frame hợp lệ đầu tiên */
    if (!state->initialized) {
        state->initialized = true;
        state->last_counter = counter;
        state->err_count = 0;
        state->ok_count++;
        if (state->ok_count >= 3) {
            state->state = E2E_STATE_VALID;
        }
        state->last_status = E2E_STATUS_INITIAL;
        return E2E_STATUS_INITIAL;
    }

    /* Frame kế tiếp: tính delta = (counter_mới - counter_cũ) mod 16 */
    uint8_t max_delta = (cfg->max_delta_counter > 0) ? cfg->max_delta_counter : 3u;
    uint8_t delta = (uint8_t)((counter - state->last_counter) & 0x0Fu);
    e2e_status_t status;

    if (delta == 1) {
        status = E2E_STATUS_OK;
        state->last_counter = counter;
        state->err_count = 0;
        state->ok_count++;
        if (state->ok_count >= 3) {
            state->state = E2E_STATE_VALID;
        }
    } else if (delta >= 2 && delta <= max_delta) {
        status = E2E_STATUS_OK_SOME_LOST;
        state->last_counter = counter;
        state->err_count = 0;
        state->ok_count++;
        if (state->ok_count >= 3) {
            state->state = E2E_STATE_VALID;
        }
    } else if (delta == 0) {
        status = E2E_STATUS_REPEATED;
        state->err_count++;
        state->ok_count = 0;
        if (state->err_count >= 3) {
            state->state = E2E_STATE_INVALID;
        }
    } else {
        /* delta > max_delta */
        status = E2E_STATUS_WRONG_SEQUENCE;
        state->last_counter = counter;
        state->err_count++;
        state->ok_count = 0;
        if (state->err_count >= 3) {
            state->state = E2E_STATE_INVALID;
        }
    }

    state->last_status = status;
    return status;
}

e2e_status_t e2e_check(const e2e_config_t *cfg, e2e_rx_state_t *state,
                      const uint8_t *payload, uint8_t len)
{
    uint8_t counter = (payload != NULL && len > 0) ? (uint8_t)(payload[0] & 0x0Fu) : 0u;
    return e2e_check_counter(cfg, state, payload, len, counter);
}

void e2e_notify_timeout(e2e_rx_state_t *state)
{
    if (state == NULL) {
        return;
    }
    state->last_status = E2E_STATUS_NONEWDATA;
    state->state = E2E_STATE_INVALID;
    state->ok_count = 0;
    if (state->err_count < 3) {
        state->err_count = 3;
    }
}
