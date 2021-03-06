#ifndef GPIO_POLLING_H
#define GPIO_POLLING_H

// Configure GPIOs, get public IP, init Pushover library and creates a
// thread for continuosly polling the GPIO state and sending a notification
// message when a change is detected
// exit_polling is a pointer to a volatile variable which must be always
// 0 except when the calling process wants to terminate the thread.
// capture_path is the directory where the images captured by the camera
// will be stored.
// msg_info_fmt must point to a \0-terminated string which contains general
// information and that will be added to every message sent. This string can
// contain a %s substring which will be replaced by the public IP address.
// This fn returns 0 on success or an errno error code.
int init_polling(volatile int *exit_polling, const char *capture_path, char *msg_info_fmt);

int wait_polling_end(void);

#endif // GPIO_POLLING
