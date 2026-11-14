#ifndef BRINGUP_CONFIG_H
#define BRINGUP_CONFIG_H

/* LED trên Blue Pill: PC13, sáng khi mức thấp */
#define BRINGUP_LED_PORT   GPIOC
#define BRINGUP_LED_PIN    GPIO_PIN_13

/* Frame thử nghiệm (ngoài DBC, chỉ dùng khi bring-up). ID chuẩn 11 bit. */
#define ID_BP_TO_DK1   0x6F0u     /* Blue Pill gửi, DK1 nhận */
#define ID_DK1_TO_BP   0x6F1u     /* DK1 gửi, Blue Pill nhận */
#define DATAID_BP      0xA0u      /* Data ID dùng trong CRC của 0x6F0 */
#define DATAID_DK1     0xB1u      /* Data ID dùng trong CRC của 0x6F1 */

/* Bố cục payload 8 byte:
 *   [0] bit3..0 = Alive_Counter (0..15)    bit7..4 = 0
 *   [1] 0x11  [2] 0x22  [3..4] số thứ tự frame (little-endian)  [5] 0x55  [6] 0x66
 *   [7] CRC-8/J1850 trên [DataID, byte0..6]
 */

#ifndef BRINGUP_CAN_LOOPBACK_DEFAULT
#define BRINGUP_CAN_LOOPBACK_DEFAULT 0
#endif

#endif
