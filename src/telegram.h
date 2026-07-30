#ifndef TELEGRAM_H
#define TELEGRAM_H

// We limit Telegram messages to 1024 4-byte UTF-8 characters.
#define MAX_TELEGRAM_MSG_SIZE (1024*4)

// This function initializes the Telegram client library
// conf_filename is a pointer to a \0-terminated string containing the filename (and path) of a text
// configuration text file containing the chat ID of the Telegram bot to which the notifications must
// be sent.
// The file must has the format of the following example:
//chat_id=987654321
// The fn returns 0 on success or a errno error code
int telegram_init(char *conf_filename);

// This function connect to pushover server and send a notification
// msg_str is a pointer to a \0-terminated string containing the notification message
// The fn returns 0 on success or a errno error code
int send_telegram_message(char *msg_str);

// This function deinitializes the Telegram client library
void telegram_deinit(void);

#endif // TELEGRAM_H
