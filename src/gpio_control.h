#ifndef GPIO_CONTROL_H
#define GPIO_CONTROL_H

#define PIN_IN_DIR 0 // Set pin as input with internal pull-up resistor disabled and pull-down resistor disabled 
#define PIN_IN_DIR_PULLDOWN 1 // Set pin as input with internal pull-down resistor enabled and pull-up resistor disabled 
#define PIN_IN_DIR_PULLUP 2 // Set pin as input with internal pull-up resistor enabled and pull-down resistor disabled 
#define PIN_OUT_DIR 3 // Set pin as output (with internal pull-down resistor disabled and pull-up resistor disabled)

#define PIN_LOW_VAL  0 // pin low value
#define PIN_HIGH_VAL 1 // pin high value

// Defines according to peripheral connections
#define PIR_GPIO 17 // pin 11 in pin header
#define RELAY1_GPIO 8 // pin 24 in pin header
#define RELAY2_GPIO 9 // pin 21 in pin header
#define RELAY3_GPIO 10 // pin 19 in pin header
#define RELAY4_GPIO 11 // pin 23 in pin header
#define CONTACT1_GPIO 5 // pin 29 in pin header
#define CONTACT2_GPIO 6 // pin 31 in pin header

// Pin used as a global variable between the alarm4pi daemon
// and the web server to arm or disarm to alarm notifications:
#define ARMING_GPIO 18 // pin 12 in pin header

// Other defines


// This fn configures a GP I/O: 
// pin: pin offset 
// dir: 0=input, 1=input pull-down, 2=input pull-up, 3=output
// If the function succeeds, it returns 0. Otherwise, it returns an error code. 
int gpio_direction(int pin, int dir);

// This fn obtains the input value of a GP I/O:
// pin: pin offset
// value: pointer to an integer that will receive the obtained value 0=low, 1=high
// If the function succeeds, it returns 0. Otherwise, it returns an error code.
int gpio_read(int pin, int *value);

// This fn sets the value of a GP I/O:
// pin: pin offset
// value: 0=low, 1=high
// If the function succeeds, it returns 0. Otherwise, it returns an error code.
int gpio_write(int pin, int value);

// This fn initializes this libary so that the rest of library functions can be called
// If the function succeeds, it returns 0. Otherwise, it returns an error code.
int open_gpios(void);

// This fn configures the direcciton of the GP I/O pins used by alarm4pi.
// If the function succeeds, it returns 0. Otherwise, it returns an error code.
int configure_gpios(void);

// This fn deinitializes this libary so that the rest of library functions cannot be called,
// but any resource allocated by open_gpios freed.
// If the function succeeds, it returns 0. Otherwise, it returns an error code.
int close_gpios(void);

#endif // GPIO_CONTROL
