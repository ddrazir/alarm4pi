#include <stdio.h> // printf, sprintf
#include <stdlib.h> // exit, atoi, malloc, free
#include <unistd.h> // read, write, close, sleep
#include <string.h> // memcpy, memset
#include <errno.h> // for errno var and value definitions (EINVAL)
#include <linux/limits.h> // For PATH_MAX
#include <curl/curl.h>

#include "log_msgs.h"
#include "proc_helper.h"
#include "telegram.h"

#define TELEGRAM_SEND_MSG_URL "https://api.telegram.org/bot%s/sendMessage"

// The macro TOSTRING allows us to convert a literal number to a string containg that number (used to set fscanf string limits)
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#define MAX_CONF_STR_LEN 160 // Max. length of variables in configuration file
#define MAX_URL_LEN 2083 // De facto URL max. length
#define MAX_POST_FIELDS_LEN 4096 // Large enough size for Telegram send-message POST fields

// Variable names for the configuration file and POST body: the first character of all of these must be different due to the current conf. file parser implementtion
#define CHAT_ID "chat_id="
#define API_TOKEN "api_token="

// Global variables obatined or derived from config file data by init fn
// We make them static so that they have internal linkage and won't conflict with variables of the same name in other files
static char Chat_id[MAX_CONF_STR_LEN+1];
static char Api_token[MAX_CONF_STR_LEN+1];

// If this value is different from 0, it indicates that the Telegram library has been correctly initialized
static int Telegram_initialized = 0;

int telegram_init(char *conf_filename)
  {
   int ret_error;
   FILE *conf_fd;
   char full_conf_filename[PATH_MAX+1];

   if(strlen(conf_filename)>PATH_MAX)
      return(EINVAL);

   if(conf_filename[0] != '/') // Relative path specified: obtain executable directory
     {
      ret_error = get_current_exec_path(full_conf_filename, PATH_MAX);
      if(ret_error == 0) // Directory of current executable successfully obtained
        {
         if(strlen(full_conf_filename)+strlen(conf_filename) <= PATH_MAX) // total path of conf file name is not too long
            strcat(full_conf_filename, conf_filename); // Success on getting the complete conf file path 
         else // Error path too long: try to open file with relative path
            strcpy(full_conf_filename, conf_filename); // We will try to use conf_filename anyway
        }
      else // Error getting executable dir: try to open file with relative path
        {
         log_printf("Error obtaining the directory of the current-process executable file: errno=%d\n", ret_error);
         strcpy(full_conf_filename, conf_filename); // We will try to use conf_filename anyway
        }
     }
   else // Absolute path specified: use it directly with fopen()
      strcpy(full_conf_filename, conf_filename);

   conf_fd=fopen(full_conf_filename, "rt");
   if(conf_fd != NULL)
     {
      // Delete global string variables
      Chat_id[0]='\0';
      Api_token[0]='\0';

      ret_error = 0;
      while(!feof(conf_fd) && ret_error == 0)
        {
         // Try to read any of the recognized variables
         // It is necessary that all the variables names start with a different letter, so that
         // fscanf does not get chars from file buffer if the corresponding variable is not readed
         if(fscanf(conf_fd, " "CHAT_ID" %" TOSTRING(MAX_CONF_STR_LEN) "s\n", Chat_id) == 0 &&
            fscanf(conf_fd, " "API_TOKEN" %" TOSTRING(MAX_CONF_STR_LEN) "s\n", Api_token) == 0)
           {
            ret_error = EINVAL; // Exit loop
           }
        }
      if(ret_error == 0) // No error so far
        {
         if(strlen(Chat_id) > 0) // If Telegram chat ID could be loaded
           {
            if(strlen(Api_token) > 0) // If API token could be loaded
              {
               if(strchr(Api_token,':') != NULL) // Api_Token seems to be correct
                 {
                  CURLcode curl_res = curl_global_init(CURL_GLOBAL_DEFAULT);
                  if(curl_res == CURLE_OK)
                    {
                     Telegram_initialized = 1; // Set the library initialization flag as true
                     log_printf("Using Telegram bot: %s to send notifications\n",Api_token);
                     ret_error = 0; // Initialization succeeded
                    }
                  else
                    {
                     log_printf("Error initializing Curl library for Telegram API: %s\n", curl_easy_strerror(curl_res));
                     Telegram_initialized = 0; // Set the library initialization flag as false
                     ret_error = EINVAL;
                    }
                 }
               else
                 {
                  log_printf("Error loading Telegram config file: the API token does not contain the ':' sepatator character\n");
                  Telegram_initialized = 0;
                  ret_error = EINVAL;
                 }
              }
            else
              {
               log_printf("Error loading Telegram config file: API token not found\n");
               Telegram_initialized = 0;
               ret_error = EINVAL;
              }
           }
         else
           {
            log_printf("Error loading Telegram config file: Chat ID not found\n");
            Telegram_initialized = 0;
            ret_error = EINVAL;
           }
        }
      else
        {
         Telegram_initialized = 0;
         log_printf("Error loading Telegram config file: unknown variable name found in file\n");
        }
      fclose(conf_fd);
     }
   else
     {
      ret_error = errno;
      Telegram_initialized = 0;
      log_printf("Error opening Telegram config file %s: errno=%d\n", full_conf_filename, errno);
     }

   return(ret_error);
  }

struct curl_response_buffer {
    char *data;
    size_t size;
};

static size_t curl_write_callback_fn(void *contents, size_t size, size_t nmemb, void *userp)
  {
   size_t realsize = size * nmemb;
   struct curl_response_buffer *buf = userp;

   char *new_data = realloc(buf->data, buf->size + realsize + 1);
   if (new_data != NULL)
     {
      buf->data = new_data;
      memcpy(buf->data + buf->size, contents, realsize);
      buf->size += realsize;
      buf->data[buf->size] = '\0';
     }
   else
     {
      log_printf("Error: Out of memory while receiving response wirh Curl.\n");
      realsize = 0;   // Causes libcurl to abort the transfer.
     }

    return realsize;
  }

int send_telegram_message(char *msg_str)
  {
   int ret_error = 0;
   CURL *curl_handle;

   if(Telegram_initialized == 0) // check if the library not is initializated
     {
      //log_printf("Error the Telegram send message function is being called but the library in not initizalized\n");
      return(EPERM);
     }

   struct curl_response_buffer post_response =
     {
      .data = NULL,
      .size = 0
     }; 

   curl_handle = curl_easy_init();
   if(curl_handle != NULL)
     {
      char *escaped_msg = curl_easy_escape(curl_handle, msg_str, 0);
      if(escaped_msg != NULL)
        {
         CURLcode curl_res;
         char telegram_url[MAX_URL_LEN];
         char post_fields[MAX_POST_FIELDS_LEN];
         snprintf(telegram_url, sizeof(telegram_url), TELEGRAM_SEND_MSG_URL, Api_token);
         snprintf(post_fields, sizeof(post_fields),"chat_id=%s&text=%s", Chat_id, escaped_msg);

         curl_easy_setopt(curl_handle, CURLOPT_URL, telegram_url);
         curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, post_fields);
         curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "telegram-c-notifier/1.0");

         curl_easy_setopt(curl_handle, CURLOPT_CONNECTTIMEOUT, 10L);
         curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, 30L);

         curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, curl_write_callback_fn);
         curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &post_response);

         curl_res = curl_easy_perform(curl_handle);
         if(curl_res == CURLE_OK)
           {
            long http_code = 0;

            curl_res = curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &http_code);
            if(curl_res == CURLE_OK)
              {
               /*
               // Debug info:
               log_printf("Telegram send-message POST HTTP status: %ld\n", http_code);
               if(post_response.data != NULL)
                  log_printf("Telegram send-message POST response:\n%s\n", post_response.data);
               */
               if(http_code == 200)
                  ret_error=0;
               else
                 {
                  log_printf("Error: Telegram server returned HTTP code: %ld (instead of 200).\n", http_code);
                  ret_error=EINVAL;
                 }
              }
            else
              {
               log_printf("Error: Curl could not obtain the HTTP response code of the Telegram message post.\n");
               ret_error=EINVAL;
              }

            curl_free(escaped_msg);
            curl_easy_cleanup(curl_handle);
            free(post_response.data);
           }
         else
           {
            log_printf("Error: Failed to send Telegram post message: %s\n", curl_easy_strerror(curl_res));
            curl_free(escaped_msg);
            curl_easy_cleanup(curl_handle);
            free(post_response.data);
            ret_error=ECOMM;            
           }
        }
      else
        {
         curl_easy_cleanup(curl_handle);
         ret_error=ENOMEM;
         log_printf("Error: Curl failed to URL-encode message to send through Telegram.\n");
        }

     }
   else
     {
      ret_error=ENOMEM;
      log_printf("Error: curl_easy_init() failed for Telegram.\n");
     }
   return(ret_error);
  }

void telegram_deinit(void)
  {
   if(Telegram_initialized)
     {
      curl_global_cleanup();
      Telegram_initialized--;
     }
  }
