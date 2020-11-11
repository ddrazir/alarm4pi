#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/file.h>
#include <stdarg.h>
#include <linux/limits.h> // For PATH_MAX
#include <string.h>
#include <errno.h>
#include <syslog.h>
// For mkdir:
#include <sys/stat.h>
#include <sys/types.h>

#include "log_msgs.h"
#include "proc_helper.h" // for get_current_exec_path()

int Console_messages=1; // Indicate whether info messages should be printed in console

FILE *Log_file_handle=NULL, *Event_file_handle=NULL; // File handle to write debug messages

// Get date and time string and store it in the specified buffer of specified max lentgh
void get_localtime_str(char *cur_time_str, size_t cur_time_str_len)
  {
   time_t cur_time;
   struct tm *cur_time_struct;

   cur_time = time(NULL);  // Get current time
   if(cur_time != (time_t)-1)
     {
      cur_time_struct = localtime(&cur_time); // Use local time (not UTC)
      if(strftime(cur_time_str, cur_time_str_len, "%Y-%m-%d %H:%M:%S", cur_time_struct)==0) // No output 
         if(cur_time_str_len>0) // if strftime() returns 0, the contents of the array may be undefined
            cur_time_str[0]='\0'; // Terminate string
     }
   else
     {
      if(cur_time_str_len>0) // Error getting time, terminate string
         cur_time_str[0]='\0';
     }
  }

// Print debug messaages (event or log) using printf argument format.
// If the specified file has been opened (!= NULL), write messages on it
// If console debug mode is enabled (Debug_messages != 0), write messages in console as well
int msg_printf(FILE *out_file_handle, const char *format, ...)
  {
   int ret;
   if(Console_messages || out_file_handle != NULL)
     {
      int printf_ret=0,fprintf_ret=0;
      va_list arglist;
      char cur_time_str[20];

	   get_localtime_str(cur_time_str, sizeof(cur_time_str));
      if(Console_messages)
        {
         va_start(arglist, format);
         printf_ret=vprintf(format, arglist);
         va_end(arglist);
        }

      if(out_file_handle != NULL)
        {
         va_start(arglist, format);
         fprintf(out_file_handle, "[%s] ",cur_time_str);
         fprintf_ret=vfprintf(out_file_handle, format, arglist);
         va_end(arglist);
        }
      ret=(printf_ret!=0)?printf_ret:fprintf_ret;
     }
   else
      ret=0;
   return(ret);
  }

// Open (or creates) a out-message text file for writing info messages
// If max_file_len<>0, truncates the file to prevent if from becoming huge
FILE *open_msg_file(const char *file_name, long max_file_len)
  {
   FILE *file_handle;
   file_handle=fopen(file_name,"a+t"); // create a new file if it does not exist
   if(file_handle)
     {
      long log_size;
      size_t log_size_loaded;
      char cur_time_str[20];

      flock(fileno(file_handle), LOCK_UN); // Remove existing file lock held by this process
      setbuf(file_handle, NULL); // Disable file buffer: Otherwise several processes may write simultaneously to this same file using different buffers

      fseek(file_handle, 0, SEEK_END); // Move read file pointer to the end for ftell
      log_size = ftell(file_handle); // Log file size
      // Truncate file
      if (log_size > max_file_len)
        {
         char *log_file_buf;

         log_file_buf=(char *)malloc(max_file_len*sizeof(char));
         if(log_file_buf)
           {
            fseek(file_handle, -max_file_len, SEEK_END);
            log_size_loaded = fread(log_file_buf, sizeof(char), max_file_len, file_handle);
            fclose(file_handle);
            free(log_file_buf);
            file_handle = fopen(file_name,"wt"); // Delete previous log file content
            if (file_handle)
              {
               get_localtime_str(cur_time_str, sizeof(cur_time_str));

               fprintf(file_handle,"\n[%s] <Old messages deleted>\n\n", cur_time_str);
               fwrite(log_file_buf, sizeof(char), log_size_loaded, file_handle);
              }
           }
        }

      if(file_handle)
        {
         get_localtime_str(cur_time_str, sizeof(cur_time_str));
         fprintf(file_handle, "\n[%s] --------------------- Log initiated ---------------------\n", cur_time_str);     
         fprintf(file_handle, "[%s] alarm4pi daemon running\n", cur_time_str);
        }
     }
   return(file_handle);
  }

// Closes log file if file_handle is not NULL
void close_log_file(FILE *file_handle)
  {
   if(file_handle)
     {
      char cur_time_str[20];

      get_localtime_str(cur_time_str, sizeof(cur_time_str));
      fprintf(file_handle,"[%s] alarm4pi daemon terminated\n\n", cur_time_str);
      fclose(file_handle);
     }
  }

int open_log_files(const char *log_file_path)
  {
   int ret_error;
   char full_log_filename[PATH_MAX+1];
   char *filename_start;
   int mkdir_ret;

   if(log_file_path == NULL || strlen(log_file_path) >= PATH_MAX)
      return(EINVAL);

   if(log_file_path[0] != '/') // Relative path specified: we fist obtain the current executable directory
     {
      ret_error = get_current_exec_path(full_log_filename, PATH_MAX);
      if(ret_error == 0) // Directory of current executable successfully obtained
        {
         if(strlen(full_log_filename)+strlen(log_file_path) <= PATH_MAX) // we check that the total path of the log files is not too long
            strcat(full_log_filename, log_file_path);
         else // Error path too long: we will try to open log files with relative path anyway
            strcpy(full_log_filename, log_file_path);
        }
      else // Error getting executable dir: try to open file with relative path
        {
         syslog(LOG_WARNING,"When opening log files: Error obtaining the directory of the current-process executable file: errno=%d\n", ret_error);
         strcpy(full_log_filename, log_file_path);
        }
     }
   else // Absolute path specified: use it directly with fopen()
      strcpy(full_log_filename, log_file_path);

   // Create the directory for contaning the log files
   mkdir_ret = mkdir(full_log_filename, 0777);
   if(mkdir_ret == -1 && errno != EEXIST) // If an error occurred and it is different from 'File exists' (that is, the directory already exists), warn it:
      syslog(LOG_WARNING,"The log file directory (%s) cannot be created: errno=%d\n", full_log_filename, errno);

   // Pointer to the start of the log filenames in the full_log_filename array
   filename_start=full_log_filename+strlen(full_log_filename);

   if(strlen(full_log_filename)+strlen(LOG_FILE_NAME) <= PATH_MAX) // we check that the total filename of the log file is not too long
     {
      strcpy(filename_start, LOG_FILE_NAME);
      Log_file_handle=open_msg_file(full_log_filename, MAX_PREV_MSG_FILE_SIZE);
      if(Log_file_handle == NULL)
         syslog(LOG_WARNING,"The daemon log file (%s) cannot be created or opened: errno=%d\n", full_log_filename, errno);
     }

   if(strlen(full_log_filename)+strlen(EVENT_FILE_NAME) <= PATH_MAX) // we check that the total filename of the event file is not too long
     {
      strcpy(filename_start, EVENT_FILE_NAME);
      Event_file_handle=open_msg_file(full_log_filename, MAX_PREV_MSG_FILE_SIZE);
      if(Event_file_handle == NULL)
         syslog(LOG_WARNING,"The event log file (%s) cannot be created or opened: errno=%d\n", full_log_filename, errno);
     }

   return(Log_file_handle == NULL || Event_file_handle == NULL);
  }

void close_log_files(void)
  {
   close_log_file(Event_file_handle);
   close_log_file(Log_file_handle);
  }
