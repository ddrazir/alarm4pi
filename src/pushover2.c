#include <stdio.h> // printf, sprintf
#include <stdlib.h> // exit, atoi, malloc, free
#include <unistd.h> // read, write, close
#include <string.h> // memcpy, memset
#include <errno.h> // for errno var and value definitions
#include <limits.h> // max hostname length
#include <linux/limits.h> // For PATH_MAX
#include <curl/curl.h>

#include "log_msgs.h"
#include "proc_helper.h"
#include "pushover2.h"

// The macro TOSTRING allows us to convert a literal number to a string containg that number (used to set fscanf string limits)
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#define MAX_CONF_STR_LEN 160 // Max. length of variables in configuration file
#define MAX_URL_LEN 2083 // De facto URL max. length
#define MAX_POST_FIELDS_LEN 4096 // Large enough size for Pushover send-message POST fields
#define PUSHOVER_SEND_MSG_URL "https://api.pushover.net/1/messages.json"

// Variable names for the configuration file and POST body: the first character of all of these must be different due to the current conf. file parser implementtion
#define SERVER_URL "server_url="
#define TOKEN_ID "token="
#define USER_ID "user="
// Variables only for POST
#define MESSAGE_ID "message="
#define PRIORITY_ID "priority="
#define RETRY_ID "retry="
#define EXPIRE_ID "expire="
// If message priority is set to 2, these parameters set the time period at which the message will be repeated
// and the time at which the message expires (in seconds)
#define RETRY_TIME_SEC 31
#define EXPIRE_TIME_SEC 120

// Invalid value used to indicate that the library is not initializated
#define INVALID_ADDR_PORT -1

// Global variables obatined or derived from config file data by init fn
// We make them static so that they have internal linkage and won't conflict with variables of the same name in other files
static char Token_id[MAX_CONF_STR_LEN+1];
static char User_id[MAX_CONF_STR_LEN+1];
static char Server_url[MAX_URL_LEN+1];
// If this value is different from 0, it indicates that the Pushover library has been correctly initialized
static int Pushover_initialized = 0;

struct curl_response_buffer {
   char *data;
   size_t size;
};

static size_t curl_write_callback_fn(void *contents, size_t size, size_t nmemb, void *userp)
  {
   size_t realsize;
   struct curl_response_buffer *buf;
   char *new_data;

   realsize = size * nmemb;
   buf = userp;

   new_data = realloc(buf->data, buf->size + realsize + 1);
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
      realsize = 0; // Causes libcurl to abort the transfer.
     }

   return(realsize);
  }

int pushover_init(char *conf_filename)
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
            strcpy(full_conf_filename, conf_filename);
        }
      else // Error getting executable dir: try to open file with relative path
        {
         log_printf("Error obtaining the directory of the current-process executable file: errno=%d\n", ret_error);
         strcpy(full_conf_filename, conf_filename);
        }
     }
   else // Absolute path specified: use it directly with fopen()
      strcpy(full_conf_filename, conf_filename);

   conf_fd=fopen(full_conf_filename, "rt");
   if(conf_fd != NULL)
     {
      char server_url[MAX_URL_LEN+1];

      // Init variables to empty strings.
      // If they are not empty after loading, we assume that they have been correctly loaded
      server_url[0]='\0';
      // Delete global string variables
      Token_id[0]='\0';
      User_id[0]='\0';
      Server_url[0]='\0';

      ret_error = 0;
      while(!feof(conf_fd) && ret_error == 0)
        {
         // Try to read any of the recognized variables
         // It is necessary that all the variables names start with a different letter, so that
         // fscanf does not get chars from file buffer if the corresponding variable is not readed
         if(fscanf(conf_fd, " "SERVER_URL" %" TOSTRING(MAX_URL_LEN) "s\n", server_url) == 0 &&
            fscanf(conf_fd, " "TOKEN_ID" %" TOSTRING(MAX_CONF_STR_LEN) "s\n", Token_id) == 0 &&
            fscanf(conf_fd, " "USER_ID" %" TOSTRING(MAX_CONF_STR_LEN) "s\n", User_id) == 0)
           {
            log_printf("Error loading Pushover config file: unknown variable name found in file\n");
            ret_error = EINVAL; // Exit loop
           }
        }
      if(ret_error == 0) // No error so far
        {
         if(strlen(Token_id) > 0) // If token ID loaded
           {
            if(strlen(User_id) > 0) // If user ID loaded
              {
               if(strlen(server_url) > 0)
                 {
                  if(strncmp(server_url, "https://", 8) == 0) // URL seems to be correct
                    {
                     if(strlen(server_url) <= MAX_URL_LEN)
                        strcpy(Server_url, server_url);
                     else
                       {
                        log_printf("Error loading Pushover config file: server URL is too long (more than " TOSTRING(MAX_URL_LEN) " characters)\n");
                        Pushover_initialized = 0;
                        ret_error = EINVAL;
                       }
                    }
                  else
                    {
                     log_printf("Error loading Pushover config file: server URL start is not https://\n");
                     Pushover_initialized = 0;
                     ret_error = EINVAL;
                    }
                 }
               else
                 {
                  strcpy(Server_url, PUSHOVER_SEND_MSG_URL);
                 }

               if(ret_error == 0)
                 {
                  CURLcode curl_res = curl_global_init(CURL_GLOBAL_DEFAULT);
                  if(curl_res == CURLE_OK)
                    {
                     Pushover_initialized++; // Set the library initialization flag as true
                     ret_error = 0; // Initialization succeeded
                    }
                  else
                    {
                     log_printf("Error initializing Curl library for Pushover API: %s\n", curl_easy_strerror(curl_res));
                     Pushover_initialized = 0; // Set the library initialization flag as false
                     ret_error = EINVAL;
                    }
                 }
              }
            else
              {
               log_printf("Error loading Pushover config file: user id not found\n");
               Pushover_initialized = 0;
               ret_error = EINVAL;
              }
           }
         else
           {
            log_printf("Error loading Pushover config file: token id not found\n");
            Pushover_initialized = 0;
            ret_error = EINVAL;
           }
        }
      fclose(conf_fd);
     }
   else
     {
      ret_error=errno;
      Pushover_initialized = 0;
      log_printf("Error opening Pushover config file %s: errno=%d\n", full_conf_filename, errno);
     }

   return(ret_error);
  }

int send_pushover_notification(char *msg_str, char *msg_priority)
  {
   int ret_error = 0;
   CURL *curl_handle;

   if(Pushover_initialized == 0) // check if the library is not initializated
      return(EPERM);

   if(strlen(msg_str) >= MAX_PUSHOVER_MSG_SIZE)
      return(EINVAL);

   curl_handle = curl_easy_init();
   if(curl_handle != NULL)
     {
      struct curl_response_buffer post_response =
        {
         .data = NULL,
         .size = 0
        };
      char post_fields[MAX_POST_FIELDS_LEN];
      char *escaped_msg;
      char *escaped_token;
      char *escaped_user;
      char *escaped_priority;

      escaped_msg = curl_easy_escape(curl_handle, msg_str, 0);
      escaped_token = curl_easy_escape(curl_handle, Token_id, 0);
      escaped_user = curl_easy_escape(curl_handle, User_id, 0);
      escaped_priority = curl_easy_escape(curl_handle, msg_priority, 0);

      if(escaped_msg != NULL && escaped_token != NULL && escaped_user != NULL && escaped_priority != NULL)
        {
         CURLcode curl_res;
         long http_code = 0;
         char *status_ptr;
         int notif_state;
         int variables_obtined;

         if(strcmp(msg_priority,"2") == 0) // Max. priority selected, include parameters: retry and expire in the body
           {
            snprintf(post_fields, sizeof(post_fields), TOKEN_ID"%s&"USER_ID"%s&"MESSAGE_ID"%s&"PRIORITY_ID"%s&"RETRY_ID"%d&"EXPIRE_ID"%d", escaped_token, escaped_user, escaped_msg, escaped_priority, RETRY_TIME_SEC, EXPIRE_TIME_SEC);
           }
         else
           {
            snprintf(post_fields, sizeof(post_fields), TOKEN_ID"%s&"USER_ID"%s&"MESSAGE_ID"%s&"PRIORITY_ID"%s", escaped_token, escaped_user, escaped_msg, escaped_priority);
           }

         curl_easy_setopt(curl_handle, CURLOPT_URL, Server_url);
         curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, post_fields);
         curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "pushover-c-notifier/1.0");
         curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, NULL);

         curl_easy_setopt(curl_handle, CURLOPT_CONNECTTIMEOUT, 10L);
         curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, 30L);

         curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, curl_write_callback_fn);
         curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &post_response);

         curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 1L);
         curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 2L);

         curl_res = curl_easy_perform(curl_handle);
         if(curl_res == CURLE_OK)
           {
            curl_res = curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &http_code);
            if(curl_res == CURLE_OK)
              {
               if(http_code == 200)
                 {
                  if(post_response.data != NULL)
                    {
                     status_ptr = strstr(post_response.data, "\"status\"");
                     if(status_ptr != NULL)
                       {
                        variables_obtined = 0;
                        notif_state = 0;
                        if(sscanf(status_ptr, "\"status\"%*[^0-9-]%d", &notif_state) == 1)
                           variables_obtined = 1;

                        if(variables_obtined != 0)
                          {
                           if(notif_state == 1) // Server code=success: notification correctly pushed
                              ret_error = 0;
                           else
                             {
                              ret_error = EBADRQC;
                              log_printf("Error status code %i received from Pushover server\n", notif_state);
                             }
                          }
                        else
                          {
                           ret_error = EPROTO;
                           log_printf("Invalid format of response body from Pushover server. Status code could not be obtained\n");
                          }
                       }
                     else
                       {
                        ret_error = EPROTO;
                        log_printf("Invalid format of response body from Pushover server. Status code field not found\n");
                       }
                    }
                  else
                    {
                     ret_error = EPROTO;
                     log_printf("Empty response body from Pushover server\n");
                    }
                 }
               else
                 {
                  ret_error = EBADRQC;
                  log_printf("HTTP error code %u received from Pushover server\n", (unsigned int)http_code);
                 }
              }
            else
              {
               ret_error = EINVAL;
               log_printf("Error: Curl could not obtain the HTTP response code of the Pushover message post.\n");
              }
           }
         else
           {
            ret_error = ECOMM;
            log_printf("Error: Failed to send Pushover post message: %s\n", curl_easy_strerror(curl_res));
           }

         curl_free(escaped_msg);
         curl_free(escaped_token);
         curl_free(escaped_user);
         curl_free(escaped_priority);
         curl_easy_cleanup(curl_handle);
         free(post_response.data);
        }
      else
        {
         if(escaped_msg != NULL)
            curl_free(escaped_msg);
         if(escaped_token != NULL)
            curl_free(escaped_token);
         if(escaped_user != NULL)
            curl_free(escaped_user);
         if(escaped_priority != NULL)
            curl_free(escaped_priority);

         curl_easy_cleanup(curl_handle);
         free(post_response.data);
         ret_error = ENOMEM;
         log_printf("Error: Curl failed to URL-encode data to send through Pushover.\n");
        }
     }
   else
     {
      ret_error=ENOMEM;
      log_printf("Error: curl_easy_init() failed for Pushover.\n");
     }
   return(ret_error);
  }

void pushover_deinit(void)
  {
   if(Pushover_initialized)
     {
      curl_global_cleanup();
      Pushover_initialized--;
     }
  }
