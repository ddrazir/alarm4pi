#ifndef GPIO_POLLING_H
#define GPIO_POLLING_H

int init_polling(volatile int *exit_polling);

int wait_polling_end(void);

#endif // GPIO_POLLING
