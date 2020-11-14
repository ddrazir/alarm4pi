#ifndef LOG_MSGS_H
#define LOG_MSGS_H
#include <stdio.h>

#define LOG_FILE_NAME "daemon.log" // Debug log file. Debug messages will be written into it (and maybe console as well)
#define EVENT_FILE_NAME "events.log" // Events log file
#define MAX_PREV_MSG_FILE_SIZE (32*1024)

extern FILE *Log_file_handle, *Event_file_handle;

// This fn obatins a string encoding the current local time with the specified
// format and store it in a buffer of specified max length
// cur_time_str buffer where the resultant string will be stored
// cur_time_str_len max length of the string the string than can be stored in
// the output buffer
// str_fmt format specifier as defined for the fn strftime()
void get_localtime_str(char *cur_time_str, size_t cur_time_str_len, const char *str_fmt);

// This function is called once at the beginnig to open the 2 log files,
// that the messgae can be written to them
// log_path is the path (relative to the currently executable or absolute)
// where these files can be created or found.
int open_log_files(const char *log_path);

// At the end of the program this function is executed to close the log files
// so that the changes (mesages) are not lost.
void close_log_files(void);

// Used internally to write a menssage in any log file
int msg_printf(FILE *out_file_handle, const char *format, ...);

// Print event messaages using printf argument format.
// If the event file has been opened (Event_file_handle != NULL), write event messages on it
// If console debug mode is enabled (Debug_messages != 0), write event messages in console as well
#define event_printf(...) msg_printf(Event_file_handle, __VA_ARGS__)

// Print log  messaages using printf argument format.
// If the log file has been opened (Log_file_handle != NULL), write log messages on it
// If console debug mode is enabled (Debug_messages != 0), write log messages in console as well
#define log_printf(...) msg_printf(Log_file_handle, __VA_ARGS__)

#endif
