#include "unity.h"
#include "e2e_crc8.h"

void setUp(void) {}
void tearDown(void) {}

static void test_e2e_protect_basic(void)
{
    const e2e_config_t cfg = { .data_id = 0x3C, .dlc = 4, .max_delta_counter = 3 };
    e2e_tx_state_t tx;
    e2e_tx_init(&tx);
    TEST_ASSERT_EQUAL_UINT8(0, tx.alive_counter);

    uint8_t payload[4] = { 0xF0, 0x11, 0x22, 0x00 };
    uint8_t crc = e2e_protect(&cfg, &tx, payload, 4);

    /* Byte 0: preserve 0xF0, counter = 0 -> 0xF0 */
    TEST_ASSERT_EQUAL_HEX8(0xF0, payload[0]);
    TEST_ASSERT_EQUAL_HEX8(crc, payload[3]);
    TEST_ASSERT_EQUAL_UINT8(1, tx.alive_counter);

    /* Next frame: counter should be 1 */
    payload[0] = 0x50;
    crc = e2e_protect(&cfg, &tx, payload, 4);
    TEST_ASSERT_EQUAL_HEX8(0x51, payload[0]);
    TEST_ASSERT_EQUAL_HEX8(crc, payload[3]);
    TEST_ASSERT_EQUAL_UINT8(2, tx.alive_counter);
}

static void test_e2e_rx_init_state(void)
{
    e2e_rx_state_t rx;
    e2e_rx_init(&rx);

    TEST_ASSERT_FALSE(rx.initialized);
    TEST_ASSERT_EQUAL_UINT8(0, rx.last_counter);
    TEST_ASSERT_EQUAL_UINT8(0, rx.err_count);
    TEST_ASSERT_EQUAL_UINT8(0, rx.ok_count);
    TEST_ASSERT_EQUAL_INT(E2E_STATE_VALID, rx.state);
    TEST_ASSERT_EQUAL_INT(E2E_STATUS_NONEWDATA, rx.last_status);
}

static void test_e2e_check_initial_and_ok_sequence(void)
{
    const e2e_config_t cfg = { .data_id = 0x5A, .dlc = 7, .max_delta_counter = 3 };
    e2e_tx_state_t tx = { .alive_counter = 5 };
    e2e_rx_state_t rx;
    e2e_rx_init(&rx);

    uint8_t p[7] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x00, 0x00 };
    e2e_protect(&cfg, &tx, p, 7);

    /* Frame 1: Initial */
    e2e_status_t status = e2e_check(&cfg, &rx, p, 7);
    TEST_ASSERT_EQUAL_INT(E2E_STATUS_INITIAL, status);
    TEST_ASSERT_TRUE(rx.initialized);
    TEST_ASSERT_EQUAL_UINT8(5, rx.last_counter);
    TEST_ASSERT_EQUAL_INT(E2E_STATE_VALID, rx.state);

    /* Frame 2: Next sequential frame (delta = 1) */
    p[0] = 0x00;
    e2e_protect(&cfg, &tx, p, 7);
    status = e2e_check(&cfg, &rx, p, 7);
    TEST_ASSERT_EQUAL_INT(E2E_STATUS_OK, status);
    TEST_ASSERT_EQUAL_UINT8(6, rx.last_counter);
    TEST_ASSERT_EQUAL_INT(E2E_STATE_VALID, rx.state);
}

static void test_e2e_check_counter_rollover(void)
{
    const e2e_config_t cfg = { .data_id = 0x96, .dlc = 8, .max_delta_counter = 3 };
    e2e_tx_state_t tx = { .alive_counter = 15 };
    e2e_rx_state_t rx;
    e2e_rx_init(&rx);

    uint8_t p[8] = {0};
    e2e_protect(&cfg, &tx, p, 8);
    TEST_ASSERT_EQUAL_INT(E2E_STATUS_INITIAL, e2e_check(&cfg, &rx, p, 8));
    TEST_ASSERT_EQUAL_UINT8(15, rx.last_counter);

    /* Rollover 15 -> 0: delta = (0 - 15) & 0x0F = 1 */
    e2e_protect(&cfg, &tx, p, 8);
    TEST_ASSERT_EQUAL_INT(E2E_STATUS_OK, e2e_check(&cfg, &rx, p, 8));
    TEST_ASSERT_EQUAL_UINT8(0, rx.last_counter);
}

static void test_e2e_check_some_lost(void)
{
    const e2e_config_t cfg = { .data_id = 0xC3, .dlc = 4, .max_delta_counter = 3 };
    e2e_tx_state_t tx = { .alive_counter = 1 };
    e2e_rx_state_t rx;
    e2e_rx_init(&rx);

    uint8_t p[4] = {0};
    e2e_protect(&cfg, &tx, p, 4);
    e2e_check(&cfg, &rx, p, 4); /* Initial: counter = 1 */

    /* Skip 1 frame: counter becomes 3 (delta = 2) */
    tx.alive_counter = 3;
    e2e_protect(&cfg, &tx, p, 4);
    TEST_ASSERT_EQUAL_INT(E2E_STATUS_OK_SOME_LOST, e2e_check(&cfg, &rx, p, 4));
    TEST_ASSERT_EQUAL_UINT8(3, rx.last_counter);

    /* Skip 2 frames: counter becomes 6 (delta = 3) */
    tx.alive_counter = 6;
    e2e_protect(&cfg, &tx, p, 4);
    TEST_ASSERT_EQUAL_INT(E2E_STATUS_OK_SOME_LOST, e2e_check(&cfg, &rx, p, 4));
    TEST_ASSERT_EQUAL_UINT8(6, rx.last_counter);
}

static void test_e2e_check_repeated_and_wrong_sequence(void)
{
    const e2e_config_t cfg = { .data_id = 0x69, .dlc = 3, .max_delta_counter = 3 };
    e2e_tx_state_t tx = { .alive_counter = 4 };
    e2e_rx_state_t rx;
    e2e_rx_init(&rx);

    uint8_t p[3] = {0};
    e2e_protect(&cfg, &tx, p, 3);
    e2e_check(&cfg, &rx, p, 3); /* Initial: counter = 4 */

    /* Repeated frame (same counter = 4) */
    tx.alive_counter = 4;
    e2e_protect(&cfg, &tx, p, 3);
    TEST_ASSERT_EQUAL_INT(E2E_STATUS_REPEATED, e2e_check(&cfg, &rx, p, 3));
    TEST_ASSERT_EQUAL_UINT8(4, rx.last_counter);

    /* Wrong sequence: jump from 4 to 8 (delta = 4 > max_delta 3) */
    tx.alive_counter = 8;
    e2e_protect(&cfg, &tx, p, 3);
    TEST_ASSERT_EQUAL_INT(E2E_STATUS_WRONG_SEQUENCE, e2e_check(&cfg, &rx, p, 3));
    TEST_ASSERT_EQUAL_UINT8(8, rx.last_counter);
}

static void test_e2e_check_crc_and_length_errors(void)
{
    const e2e_config_t cfg = { .data_id = 0xA5, .dlc = 5, .max_delta_counter = 3 };
    e2e_tx_state_t tx = { .alive_counter = 0 };
    e2e_rx_state_t rx;
    e2e_rx_init(&rx);

    uint8_t p[5] = {0};
    e2e_protect(&cfg, &tx, p, 5);

    /* Wrong length */
    TEST_ASSERT_EQUAL_INT(E2E_STATUS_WRONG_LENGTH, e2e_check(&cfg, &rx, p, 4));
    TEST_ASSERT_EQUAL_INT(E2E_STATUS_WRONG_LENGTH, e2e_check(&cfg, &rx, p, 6));

    /* Wrong CRC */
    p[4] ^= 0x55;
    TEST_ASSERT_EQUAL_INT(E2E_STATUS_WRONG_CRC, e2e_check(&cfg, &rx, p, 5));
}

static void test_e2e_debounce_3_strikes_and_recovery(void)
{
    const e2e_config_t cfg = { .data_id = 0x0F, .dlc = 3, .max_delta_counter = 3 };
    e2e_tx_state_t tx = { .alive_counter = 0 };
    e2e_rx_state_t rx;
    e2e_rx_init(&rx);

    uint8_t p[3] = {0};
    e2e_protect(&cfg, &tx, p, 3);
    TEST_ASSERT_EQUAL_INT(E2E_STATUS_INITIAL, e2e_check(&cfg, &rx, p, 3));
    TEST_ASSERT_EQUAL_INT(E2E_STATE_VALID, rx.state);

    /* 1st strike (CRC error) */
    p[2] ^= 0xFF;
    TEST_ASSERT_EQUAL_INT(E2E_STATUS_WRONG_CRC, e2e_check(&cfg, &rx, p, 3));
    TEST_ASSERT_EQUAL_UINT8(1, rx.err_count);
    TEST_ASSERT_EQUAL_INT(E2E_STATE_VALID, rx.state);

    /* 2nd strike (CRC error) */
    TEST_ASSERT_EQUAL_INT(E2E_STATUS_WRONG_CRC, e2e_check(&cfg, &rx, p, 3));
    TEST_ASSERT_EQUAL_UINT8(2, rx.err_count);
    TEST_ASSERT_EQUAL_INT(E2E_STATE_VALID, rx.state);

    /* 3rd strike (CRC error) -> Channel becomes INVALID */
    TEST_ASSERT_EQUAL_INT(E2E_STATUS_WRONG_CRC, e2e_check(&cfg, &rx, p, 3));
    TEST_ASSERT_EQUAL_UINT8(3, rx.err_count);
    TEST_ASSERT_EQUAL_INT(E2E_STATE_INVALID, rx.state);

    /* Recovery process: requires 3 consecutive valid frames */
    /* Valid frame 1 */
    tx.alive_counter = 1;
    e2e_protect(&cfg, &tx, p, 3);
    TEST_ASSERT_EQUAL_INT(E2E_STATUS_OK, e2e_check(&cfg, &rx, p, 3));
    TEST_ASSERT_EQUAL_UINT8(0, rx.err_count);
    TEST_ASSERT_EQUAL_UINT8(1, rx.ok_count);
    TEST_ASSERT_EQUAL_INT(E2E_STATE_INVALID, rx.state); /* Still invalid */

    /* Valid frame 2 */
    e2e_protect(&cfg, &tx, p, 3);
    TEST_ASSERT_EQUAL_INT(E2E_STATUS_OK, e2e_check(&cfg, &rx, p, 3));
    TEST_ASSERT_EQUAL_UINT8(2, rx.ok_count);
    TEST_ASSERT_EQUAL_INT(E2E_STATE_INVALID, rx.state); /* Still invalid */

    /* Valid frame 3 -> Channel recovers to VALID */
    e2e_protect(&cfg, &tx, p, 3);
    TEST_ASSERT_EQUAL_INT(E2E_STATUS_OK, e2e_check(&cfg, &rx, p, 3));
    TEST_ASSERT_EQUAL_UINT8(3, rx.ok_count);
    TEST_ASSERT_EQUAL_INT(E2E_STATE_VALID, rx.state);   /* Recovered! */
}

static void test_e2e_timeout_notification_and_recovery(void)
{
    const e2e_config_t cfg = { .data_id = 0xF0, .dlc = 3, .max_delta_counter = 3 };
    e2e_tx_state_t tx = { .alive_counter = 0 };
    e2e_rx_state_t rx;
    e2e_rx_init(&rx);

    uint8_t p[3] = {0};
    e2e_protect(&cfg, &tx, p, 3);
    e2e_check(&cfg, &rx, p, 3);
    TEST_ASSERT_EQUAL_INT(E2E_STATE_VALID, rx.state);

    /* Timeout notification */
    e2e_notify_timeout(&rx);
    TEST_ASSERT_EQUAL_INT(E2E_STATE_INVALID, rx.state);
    TEST_ASSERT_EQUAL_INT(E2E_STATUS_NONEWDATA, rx.last_status);

    /* Requires 3 valid frames to recover */
    for (int i = 0; i < 2; i++) {
        e2e_protect(&cfg, &tx, p, 3);
        e2e_check(&cfg, &rx, p, 3);
        TEST_ASSERT_EQUAL_INT(E2E_STATE_INVALID, rx.state);
    }
    e2e_protect(&cfg, &tx, p, 3);
    e2e_check(&cfg, &rx, p, 3);
    TEST_ASSERT_EQUAL_INT(E2E_STATE_VALID, rx.state);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_e2e_protect_basic);
    RUN_TEST(test_e2e_rx_init_state);
    RUN_TEST(test_e2e_check_initial_and_ok_sequence);
    RUN_TEST(test_e2e_check_counter_rollover);
    RUN_TEST(test_e2e_check_some_lost);
    RUN_TEST(test_e2e_check_repeated_and_wrong_sequence);
    RUN_TEST(test_e2e_check_crc_and_length_errors);
    RUN_TEST(test_e2e_debounce_3_strikes_and_recovery);
    RUN_TEST(test_e2e_timeout_notification_and_recovery);
    return UNITY_END();
}
