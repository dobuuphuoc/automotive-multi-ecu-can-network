#include "unity.h"
#include "e2e_crc8.h"

void setUp(void) {}
void tearDown(void) {}

/* Giá trị kiểm chuẩn của CRC-8/SAE-J1850 với chuỗi "123456789" là 0x4B */
static void test_crc8_check_value(void)
{
    TEST_ASSERT_EQUAL_HEX8(0x4B, e2e_crc8((const uint8_t *)"123456789", 9));
}

static void test_crc8_empty_is_zero(void)
{
    /* init 0xFF xor out 0xFF = 0x00 */
    TEST_ASSERT_EQUAL_HEX8(0x00, e2e_crc8((const uint8_t *)"", 0));
}

/* e2e_frame_crc(id, p, n) phải bằng e2e_crc8 trên [id, p...] */
static void test_frame_crc_matches_concatenation(void)
{
    const uint8_t id = 0xA0;
    const uint8_t p[7] = {0x03, 0x11, 0x22, 0x34, 0x12, 0x55, 0x66};
    uint8_t buf[8] = {id, 0x03, 0x11, 0x22, 0x34, 0x12, 0x55, 0x66};
    TEST_ASSERT_EQUAL_HEX8(e2e_crc8(buf, 8), e2e_frame_crc(id, p, 7));
}

static void test_single_bit_flip_detected(void)
{
    uint8_t p[7] = {0x03, 0x11, 0x22, 0x34, 0x12, 0x55, 0x66};
    uint8_t good = e2e_frame_crc(0xA0, p, 7);
    for (int byte = 0; byte < 7; byte++) {
        for (int bit = 0; bit < 8; bit++) {
            p[byte] ^= (uint8_t)(1u << bit);
            TEST_ASSERT_NOT_EQUAL_HEX8(good, e2e_frame_crc(0xA0, p, 7));
            p[byte] ^= (uint8_t)(1u << bit);
        }
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_crc8_check_value);
    RUN_TEST(test_crc8_empty_is_zero);
    RUN_TEST(test_frame_crc_matches_concatenation);
    RUN_TEST(test_single_bit_flip_detected);
    return UNITY_END();
}
