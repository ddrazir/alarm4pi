#include <stdio.h> // fscanf,...
#include <string.h> // strcpy,...
#include <errno.h> // for errno var and value definitions
#include <linux/limits.h> // For PATH_MAX
// For waitpid():
#include <sys/types.h>
#include <sys/wait.h>

#include "log_msgs.h"
#include "proc_helper.h"
#include "owncloud.h"

// The macro TOSTRING allows us to convert a literal number to a string containg that number (used to set fscanf string limits)
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#define MAX_CONF_STR_LEN 80 // Max. length of variables in configuration file
#define MAX_URL_LEN 2083 // De facto URL max. length
#define URL_START_HTTP "http://" // Potential expected URL start (non secure http)
#define URL_START_HTTPS "https://" // Potential expected URL start (secure http)
#define URL_START_OWNCLOUD "owncloud://" // Potential expected URL start (owncloud)
#define URL_START_OWNCLOUDS "ownclouds://" // Potential expected URL start (ownclouds)

// Variable names for the configuration file and POST body: the first character of all of these must be different due to the current conf. file parser implementtion
#define SERVER_URL_TAG "server_url="
#define USER_ID_TAG "user="
#define USER_PASSWORD_TAG "password="

// Directory where the images captures by the camera are stored
char Full_capture_path[PATH_MAX+1];

// Global variables obained from config file data by init fn
char Server_URL[MAX_CONF_STR_LEN+1] = ""; // Initialized to empty string to indicate that the library has not been initialized (yet)
char User_id[MAX_CONF_STR_LEN+1];
char User_password[MAX_URL_LEN+1];

int owncloud_init(const char *conf_filename, const char *full_capture_path)
  {
   int ret_error;
   FILE *conf_fd;
   char full_conf_filename[PATH_MAX+1];

   if(strlen(conf_filename)>PATH_MAX)
      return(EINVAL);

   if(strlen(full_capture_path)>PATH_MAX)
      return(EINVAL);

   strcpy(Full_capture_path, full_capture_path); // Use the path of captured images as a global variable
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
            Server_URL[0]='\0'; // Set the library as not (correctly) initialized
            ret_error = EINVAL; // Exit loop
           }
        }
      if(ret_error == 0) // No error so far
        {
         if(strlen(Server_URL) > 0) // If Pushover server URL could be loaded
           {
            if(strlen(User_id) > 0) // If token ID loaded
              {
               if(strlen(User_password) > 0) // If user ID loaded
                 { // Check if the URL seems to be correct
                  if(strncmp(Server_URL, URL_START_HTTP, strlen(URL_START_HTTP)) == 0 ||
                     strncmp(Server_URL, URL_START_HTTPS, strlen(URL_START_HTTPS)) == 0 ||
                     strncmp(Server_URL, URL_START_OWNCLOUD, strlen(URL_START_OWNCLOUD)) == 0 ||
                     strncmp(Server_URL, URL_START_OWNCLOUDS, strlen(URL_START_OWNCLOUDS)) == 0)
                    {
                     log_printf("Using owncloud server %s to store the captured images\n",Server_URL);
                    }
                  else
                    {
                     log_printf("Error parsing owncloud config file: server URL start is not valid (%s, %s, %s or %s) \n", URL_START_HTTP, URL_START_HTTPS, URL_START_OWNCLOUD, URL_START_OWNCLOUDS);
                     Server_URL[0]='\0';
                     ret_error = EINVAL;
                    }
                 }
               else
                 {
                  log_printf("Error parsing owncloud config file: user password (%s) not found\n", USER_PASSWORD_TAG);
                  Server_URL[0]='\0';
                  ret_error = EINVAL;
                 }
              }
            else
              {
               log_printf("Error parsing owncloud config file: user id (%s) not found\n", USER_ID_TAG);
               Server_URL[0]='\0'; // Set the library as not (correctly) initialized
               ret_error = EINVAL;
              }
           }
         else
           {
            log_printf("Error parsing owncloud config file: server URL (%s) not found\n", SERVER_URL_TAG);
            ret_error = EINVAL;
           }
        }
      fclose(conf_fd);
     }
   else
     {
      ret_error=errno;
      log_printf("Error opening owncloud config file %s: errno=%d\n", full_conf_filename, errno);
     }

   return(ret_error);
  }

int upload_captures(void)
  {
   // owncloudcmd parameters used are:
   // -n: get the authentication credentials from the .netrc file in user home directory
   // -s: produce a succinct text output
   // --non-interactive: do not prompt asking questions
   char * const owncloudcmd_exec_args[]={"owncloudcmd", "-s", "-n", Full_capture_path, Server_URL, NULL};
   pid_t owncloudcmd_proc_id;
   int ret_err;

   if(Server_URL[0] == '\0') // Check whether the library is not (correctly) initialized
      return(EPERM);

   ret_err = run_background_command(&owncloudcmd_proc_id, owncloudcmd_exec_args[0], owncloudcmd_exec_args);
   if(ret_err == 0)
     {
      int wait_ret;

      wait_ret=waitpid(owncloudcmd_proc_id, NULL, 0); // wait for the owncloud-client process to finish
      if(wait_ret != -1) // Error returned
         event_printf("Photographs have been uploaded to owncloud\n");
      else
        {
         ret_err=errno;
         log_printf("Error waiting for the owncloud-client process to finish\n", owncloudcmd_exec_args[0]);
        }
     }
   else
      log_printf("Owncloud-client child process (%s) could not be executed\n", owncloudcmd_exec_args[0]);
   return(ret_err);
  }


