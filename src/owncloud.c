#include <stdio.h> // printf, sprintf
#include <stdlib.h> // exit, atoi, malloc, free
#include <unistd.h> // read, write, close, sleep
#include <string.h> // memcpy, memset
#include <errno.h> // for errno var and value definitions (EINVAL)
#include <linux/limits.h> // For PATH_MAX
#include <curl/curl.h>

#include "log_msgs.h"
#include "proc_helper.h"
#include "owncloud.h"

// The macro TOSTRING allows us to convert a literal number to a string containg that number (used to set fscanf string limits)
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#define MAX_CONF_STR_LEN 160 // Max. length of variables in configuration file
#define MAX_URL_LEN 2083 // De facto URL max. length
#define URL_START_HTTP "http://" // Potential expected URL start (non secure http)
#define URL_START_HTTPS "https://" // Potential expected URL start (secure http)

// Variable names for the configuration file and POST body: the first character of all of these must be different due to the current conf. file parser implementtion
#define SERVER_URL_TAG "server_url="
#define USER_ID_TAG "user="
#define USER_PASSWORD_TAG "password="

// Directory where the images captured by the camera are stored localy
static char Full_capture_path[PATH_MAX+1];
// Directory where the images captured by the camera will be stored remotely
static char Remote_destin_dir[PATH_MAX+1];

// Global variables obained from config file data by init fn
// We make them static so that they have internal linkage and won't conflict with variables of the same name in other files
static char Server_URL[MAX_URL_LEN+2]; // Owncloud server WebDAV path
static char User_id[MAX_CONF_STR_LEN+1];
static char User_password[MAX_CONF_STR_LEN+1];

// If this value is different from 0, it indicates that the Owncloud library has been correctly initialized
int Owncloud_initialized = 0;

int owncloud_init(const char *conf_filename, const char *full_capture_path, const char *remote_destin_directory)
  {
   int ret_error;
   FILE *conf_fd;
   char full_conf_filename[PATH_MAX+1];

   if(strlen(conf_filename) > PATH_MAX)
     {
      log_printf("Error: Filename of ownCloud configuration is larger than %d\n", PATH_MAX);
      return(EINVAL);
     }

   if(strlen(full_capture_path)+1 > PATH_MAX)
     {
      log_printf("Error in owncloud_init: Image capture path is larger than %d\n", PATH_MAX-1);
      return(EINVAL);
     }

   if(strlen(remote_destin_directory)+1 > PATH_MAX)
     {
      log_printf("Error: ownCloud remote destination directory is larger than %d\n", PATH_MAX-1);
      return(EINVAL);
     }

   strcpy(Full_capture_path, full_capture_path); // Use the path of captured images as a global variable
   if(strlen(Full_capture_path) > 0 && Full_capture_path[strlen(Full_capture_path)-1] != '/')
      strcat(Full_capture_path, "/");

   strcpy(Remote_destin_dir, remote_destin_directory);
   if(strlen(Remote_destin_dir) > 0 && Remote_destin_dir[strlen(Remote_destin_dir)-1] != '/')
      strcat(Remote_destin_dir, "/");

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
         log_printf("Reading owncloud configuration file: Cannot obtain the directory of the current-process executable file: errno=%d\n", ret_error);
         strcpy(full_conf_filename, conf_filename);
        }
     }
   else // Absolute path specified: use it directly with fopen()
      strcpy(full_conf_filename, conf_filename);

   // Init variables to empty strings.
   // So, if they are not empty after loading, we assume that they have been correctly loaded
   Server_URL[0]='\0'; // This default value means that the library has not been initialized yet
   User_id[0]='\0';
   User_password[0]='\0';

   conf_fd=fopen(full_conf_filename, "rt");
   if(conf_fd != NULL)
     {
      ret_error = 0; // Default return value
      while(!feof(conf_fd) && ret_error == 0)
        {
         // Try to read any of the recognized variables
         // It is necessary that all the variables names start with a different letter, so that
         // fscanf does not get chars from file buffer if the corresponding variable is not readed
         if(fscanf(conf_fd, " "SERVER_URL_TAG" %" TOSTRING(MAX_URL_LEN) "s\n", Server_URL) == 0 &&
            fscanf(conf_fd, " "USER_ID_TAG" %" TOSTRING(MAX_CONF_STR_LEN) "s\n", User_id) == 0 &&
            fscanf(conf_fd, " "USER_PASSWORD_TAG" %" TOSTRING(MAX_CONF_STR_LEN) "s\n", User_password) == 0)
           {
            log_printf("Error loading owncloud config file: unknown variable name found in file\n");
            Owncloud_initialized = 0; // Set the library as not (correctly) initialized
            ret_error = EINVAL; // Exit loop
           }
        }
      if(ret_error == 0) // No error so far
        {
         if(strlen(Server_URL) > 0) // If Owncloud server URL could be loaded
           {
            if(Server_URL[strlen(Server_URL)-1] != '/')
               strcat(Server_URL, "/");
            if(strlen(User_id) > 0) // If token ID loaded
              {
               if(strlen(User_password) > 0) // If user ID loaded
                 { // Check if the URL seems to be correct
                  if(strncmp(Server_URL, URL_START_HTTP, strlen(URL_START_HTTP)) == 0 ||
                     strncmp(Server_URL, URL_START_HTTPS, strlen(URL_START_HTTPS)) == 0)
                    {
                     CURLcode curl_res;
                     
                     log_printf("Using owncloud server: %s to store the captured images\n",Server_URL);

                     curl_res = curl_global_init(CURL_GLOBAL_DEFAULT);
                     if(curl_res == CURLE_OK)
                       {
                        Owncloud_initialized = 1; // Set the library flag as correctly initialized
                        ret_error = 0; // Initialization succeeded
                       }
                     else
                       {
                        log_printf("Error initializing Curl library for Telegram API: %s\n", curl_easy_strerror(curl_res));
                        Owncloud_initialized = 0; // Set the library initialization flag as false
                        ret_error = EINVAL;
                       }
                    }
                  else
                    {
                     log_printf("Error parsing owncloud config file: server URL start is not valid. It must be %s or %s\n", URL_START_HTTP, URL_START_HTTPS);
                     Owncloud_initialized = 0;
                     ret_error = EINVAL;
                    }
                 }
               else
                 {
                  log_printf("Error parsing owncloud config file: user password (%s) not found\n", USER_PASSWORD_TAG);
                  Owncloud_initialized = 0;
                  ret_error = EINVAL;
                 }
              }
            else
              {
               log_printf("Error parsing owncloud config file: user id (%s) not found\n", USER_ID_TAG);
               Owncloud_initialized = 0; // Set the library as not (correctly) initialized
               ret_error = EINVAL;
              }
           }
         else
           {
            log_printf("Error parsing owncloud config file: server URL (%s) not found\n", SERVER_URL_TAG);
            Owncloud_initialized = 0;
            ret_error = EINVAL;
           }
        }
      fclose(conf_fd);
     }
   else
     {
      ret_error=errno;
      Owncloud_initialized = 0;
      log_printf("Error opening owncloud config file %s: errno=%d\n", full_conf_filename, errno);
     }

   return(ret_error);
  }



int upload_capture(const char *filename_to_upload)
  {
   int ret_err;
   FILE *file_handle;
   char filename_full_path[PATH_MAX+1];
   char full_server_URL[MAX_URL_LEN+2]; // Full ownCloud server WebDAV path including username, destination directory and filename

   if(Owncloud_initialized == 0) // Check whether the library is not (correctly) initialized
     {
      log_printf("Error the Owncloud upload_capture function is being called but the library in not initizalized\n");
      return(EPERM);
     }

   if(strlen(filename_to_upload) == 0) // Check whether the parameter is not empty
     {
      log_printf("Error the filename to upload to the Owncloud server is empty\n");
      return(EINVAL);
     }

   if(strlen(Full_capture_path)+strlen(filename_to_upload) > PATH_MAX)
     {
      log_printf("Error: Filename of ownCloud configuration plus its path is larger than %d\n", PATH_MAX);
      return(EINVAL);
     }

   if(strlen(Server_URL)+strlen(User_id)+strlen(Remote_destin_dir)+strlen(filename_to_upload)+1 > MAX_URL_LEN+1)
     {
      log_printf("Error: Full URL of ownCloud server is larger than %d\n", MAX_URL_LEN+1);
      return(EINVAL);
     }

   // Compose full local path of filename to upload
   strcpy(filename_full_path, Full_capture_path);
   strcat(filename_full_path, filename_to_upload);

   // Compose full URL of ownCloud server 
   strcpy(full_server_URL, Server_URL);
   strcat(full_server_URL, User_id);
   strcat(full_server_URL, "/");
   strcat(full_server_URL, Remote_destin_dir);
   strcat(full_server_URL, filename_to_upload);

   file_handle = fopen(filename_full_path, "rb");
   if(file_handle != NULL)
     {
      CURL *curl_handle;
      off_t file_size;

      // Determine file size
      fseeko(file_handle, 0, SEEK_END);
      file_size = ftello(file_handle);
      rewind(file_handle);

      curl_handle = curl_easy_init();
      if(curl_handle != NULL)
        {
         CURLcode curl_res;

         curl_easy_setopt(curl_handle, CURLOPT_URL, full_server_URL);

         // Equivalent to -u username:passwd
         curl_easy_setopt(curl_handle, CURLOPT_USERNAME, User_id);
         curl_easy_setopt(curl_handle, CURLOPT_PASSWORD, User_password);

         // Equivalent to -T
         curl_easy_setopt(curl_handle, CURLOPT_UPLOAD, 1L);
         curl_easy_setopt(curl_handle, CURLOPT_READDATA, file_handle);
         curl_easy_setopt(curl_handle, CURLOPT_INFILESIZE_LARGE, file_size);

         // Equivalent to -v
         // curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 1L);

         // Equivalent to -k
         curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0L);
         curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 0L);

         curl_res = curl_easy_perform(curl_handle);
         if(curl_res == CURLE_OK)
           {
            long http_code = 0;

            curl_res = curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &http_code);
            if(curl_res == CURLE_OK)
              {
               log_printf("Uploaded file %s to owncloud server with HTTP status: %ld\n", filename_to_upload, http_code);

               if(http_code >= 200 && http_code < 300)
                  ret_err=0;
               else
                 {
                  log_printf("Error: ownCloud server returned HTTP code: %ld (instead of 200-300 code).\n", http_code);
                  ret_err=EINVAL;
                 }
              }
            else
              {
               log_printf("Error: Curl could not obtain the HTTP response code from the ownCloud server.\n");
               ret_err=EINVAL;
              }
            curl_easy_cleanup(curl_handle);
           }
         else
           {
            log_printf("Error: Failed to send upload request to the ownCloud server: %s\n", curl_easy_strerror(curl_res));
            curl_easy_cleanup(curl_handle);
            ret_err=ECOMM;            
           }
        }
      else
        {
         ret_err=ENOMEM;
         log_printf("Error: curl_easy_init() failed for ownCloud.\n");
        }
      fclose(file_handle);
     }
   else
     {
      log_printf("Error opening file for upload to the ownCloud server: %s\n", filename_to_upload);
      ret_err=errno;
     }
   return(ret_err);
  }

#define MAX_FILENAME_LEN 160
int upload_captures(const char *filenames_to_upload, int num_of_files)
  {
   int ret_err, file_ind;
   char filename_to_upload[MAX_FILENAME_LEN];

   ret_err = 0;
   for(file_ind=0;file_ind<num_of_files && ret_err == 0;file_ind++)
     {
      snprintf(filename_to_upload, sizeof(filename_to_upload), filenames_to_upload, file_ind);
      ret_err = upload_capture(filename_to_upload);
     }

   return(ret_err);
  }

void owncloud_deinit(void)
  {
   if(Owncloud_initialized)
      curl_global_cleanup();
  }