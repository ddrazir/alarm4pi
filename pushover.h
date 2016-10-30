#ifndef PUSHOVER_H
#define PUSHOVER_H


int pushover_init(char *conf_filename);

int send_notification(char *msg_str, char *msg_priority);

#endif // PUSHOVER_H
