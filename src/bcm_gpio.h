#ifndef BCM_GPIO_H
#define BCM_GPIO_H

#define PIN_IN_DIR  0
#define PIN_OUT_DIR 1
 
#define PIN_LOW_VAL  0
#define PIN_HIGH_VAL 1

// Defines according to peripheral connections
#define PIR_GPIO 17 // pin 11 in pin header
#define RELAY1_GPIO 8 // pin 24 in pin header
#define RELAY2_GPIO 9 // pin 21 in pin header
#define RELAY3_GPIO 10 // pin 19 in pin header
#define RELAY4_GPIO 11 // pin 23 in pin header
// Pin used as a global variable between the alarm4pi daemon
// and the web server to arm or disarm to alarm notifications:
#define ARMING_GPIO 18 // pin 12 in pin header

// Internal defines
#define PIN_NAME_MAX_BUFF_LEN 4
#define PIN_DIRECTION_PATH_MAX_LEN 34
#define GPIO_EXPORT_MAX_WAIT_CYCLES 20
#define PIN_VALUE_PATH_MAX_LEN 30
#define PIN_VALUE_STR_LEN 4

// General fn for BCM
int GPIO_export(int pin);
int GPIO_unexport(int pin);
int GPIO_direction(int pin, int dir);
int GPIO_read(int pin, int *value);
int GPIO_write(int pin, int value);

// Fn specific for alarm sys connections
int export_gpios(void);
int configure_gpios(void);
int unexport_gpios(void);

#endif
