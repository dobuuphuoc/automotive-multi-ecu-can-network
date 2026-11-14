/* Log qua USART1 (PA9=TX, PA10=RX), 115200 8N1. Gọi từ nhiều task được (có mutex). */
#ifndef LOG_H
#define LOG_H
void log_init(void);                         /* gọi một lần, trước khi chạy scheduler */
void log_printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
#endif
