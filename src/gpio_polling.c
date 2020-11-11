#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h> //INET6_ADDRSTRLEN
#include <linux/limits.h> // For PATH_MAX
// For waitpid();
#include <sys/types.h>
#include <sys/wait.h>
// For mkdir:
#include <sys/stat.h>
#include <sys/types.h>

#include "bcm_gpio.h"
#include "log_msgs.h"
#include "public_ip.h"
#include "pushover.h"
#include "proc_helper.h"

#define PUSHOVER_CONFIG_FILENAME "pushover_conf.txt"

// The sensor value will be checked each (in seconds):
#define PIR_POLLING_PERIOD_SECS 1
// The sensor value will be remembered during (in periods):
#define PIR_PERMAN_PERS 60

// ID of the thread created by init_polling
pthread_t Polling_thread_id;
// General info string which is added to every msg sent
char Msg_info_str[INET6_ADDRSTRLEN+100];

// Directory where the images captures by the camera will be stored
char Full_capture_path[PATH_MAX+1];

/*
      // Write GPIO value
      ret_err=GPIO_write(RELAY1_GPIO, repeat % 2);
      if (0 != ret_err)
         printf("Error %d: %s\n",ret_err,strerror(ret_err));
*/

int send_info_notif(char *msg_str, char *msg_priority)
  {
   char tot_msg_str[MAX_PUSHOVER_MSG_SIZE];

   snprintf(tot_msg_str, MAX_PUSHOVER_MSG_SIZE, "%s. %s", Msg_info_str, msg_str);

   return(send_notification(tot_msg_str, msg_priority));
  }

// Precondition: Msg_info_str must point to a \0 terminated string
int update_ip_msg(char *msg_info_fmt)
  {
   int ret_err;

   char wan_address[INET6_ADDRSTRLEN];
   char curr_msg_info_str[sizeof(Msg_info_str)];

   ret_err=get_public_ip(wan_address);
   if(ret_err==0)
     {
      // Compose a general info string that will be added to each message sent
      snprintf(curr_msg_info_str, sizeof(curr_msg_info_str), msg_info_fmt, wan_address);
      if(strcmp(curr_msg_info_str, Msg_info_str) != 0) // Public IP address has chanded: Update msg info string and notification
        {
         strcpy(Msg_info_str, curr_msg_info_str);

         log_printf("Public IP address: %s\n", wan_address);

         send_info_notif("Alarm4pi running. Public IP obtained","-2");
        }
     }
   return(ret_err);
  }

#define MAX_TIME_FILENAME_LEN 19

// Obtain a date-and-time string and store it in the specified buffer of specified max length
void get_localtime_filename(char *cur_time_str, size_t cur_time_str_len)
  {
   get_localtime_str(cur_time_str, cur_time_str_len, "%Y-%m-%d_%H_%M_%S");
  }

#define IMAGE_FILENAME_END "_%02d.jpg"

void capture_images(void)
  {
   char full_image_file_path[PATH_MAX+1];
   // raspistill parameters used:
   // -n: no preview
   // -q 10: image quality 10 % (quality 100 means almost completely uncompressed)
   // -bm: burst capture mode: The camera does not return to preview mode between captures, allowing captures to be taken closer together
   // -tl 1000: 1000 ms time between shots. %d must be specified in the filename for the counter
   // -t 2000: 2000 ms time before the camera (takes picture and) shuts down. 2000/1000=2 images will be take
   // -th none: no thumbnail will be inserted into the JPEG file
   char * const capture_exec_args[]={"raspistill", "-n", "-w", "640", "-h", "480", "-q", "10", "-o", full_image_file_path, "-bm", "-tl", "1000", "-t", "2000", "-th", "none", NULL};
//   char * const capture_exec_args[]={"touch", full_image_file_path, NULL};
   pid_t capture_proc_id;
   int capture_run_ret;
   char image_filename[MAX_TIME_FILENAME_LEN+sizeof(IMAGE_FILENAME_END)+1];

   get_localtime_filename(image_filename, sizeof(image_filename));
   strcat(image_filename, IMAGE_FILENAME_END);
   if(strlen(Full_capture_path)+strlen(image_filename) < sizeof(full_image_file_path))
     {
      strcpy(full_image_file_path, Full_capture_path);
      strcat(full_image_file_path, image_filename);

      capture_run_ret = run_background_command(&capture_proc_id, capture_exec_args[0], capture_exec_args);
      if(capture_run_ret == 0)
        {
         int wait_ret;

         wait_ret=waitpid(capture_proc_id, NULL, 0); // wait for the capture process to finish
         if(wait_ret != -1) // Error returned
            event_printf("Photographs have been taken: %s\n", full_image_file_path);
         else
            log_printf("Error waiting for the capture process to finish\n", capture_exec_args[0]);
        }
      else
         log_printf("Capture child process (%s) could not be executed\n", capture_exec_args[0]);
     }
   else
      log_printf("The complete captured image filename (%s) could not be composed: path too long\n", image_filename);
  }

void on_alarm_event(void)
  {
   event_printf("GPIO PIR (%i) value != 0\n", PIR_GPIO);
   send_info_notif("PIR sensor activated", "2");
   capture_images();
  }

void* polling_thread(volatile int *exit_polling)
  {
   int ret_err;
   int read_err;
   int alarm_armed;
   int curr_pir_value;
   int last_pir_value;
   int pir_perman_counter;

   event_printf("GPIO server initiated\n");

   read_err = 0; // Default thread return value
   pir_perman_counter = 0; // Sensor not activated
   last_pir_value = 0; // Assume that the sensor is off at the beginning
   while(*exit_polling == 0) // While the exit signal is not triggered:
     {
      // Check if the alarm is armed:
      ret_err = GPIO_read(ARMING_GPIO, &alarm_armed);
      if(ret_err == 0) // Success reading
        {
         if(alarm_armed == 0) // If the alarm is armed (GPIO set to 0):
           {
            // Check if the PIR sensor is activated:
            ret_err = GPIO_read(PIR_GPIO, &curr_pir_value);
            if(ret_err == 0) // Success reading
              {
               if(curr_pir_value != last_pir_value) // Sensor output changed
                 {
                  if(curr_pir_value != 0)
                    {
                     on_alarm_event();
                    }
                  last_pir_value = curr_pir_value;
                 }

               if(curr_pir_value != 0) // Sensor output activated, remember its value
                  pir_perman_counter = PIR_PERMAN_PERS;
              }
            else
              {
               if(read_err==0) // No error code has been logged yet
                 {
                  log_printf("Error %i while reading PIR GPIO (%i): %s\n", ret_err, PIR_GPIO, strerror(ret_err));
                  read_err=ret_err;
                 }
              }
           }
        }
      else
        {
         if(read_err==0) // No error code has been logged yet
           {
            log_printf("Error %i while reading alarm arming GPIO (%i): %s\n", ret_err, ARMING_GPIO, strerror(ret_err));
            read_err=ret_err;
           }
        }
      sleep(PIR_POLLING_PERIOD_SECS);
      if(pir_perman_counter > 0)
         pir_perman_counter--;
     }

   event_printf("GPIO server terminated with error code: %i\n", read_err);
   return((void *)(intptr_t)read_err); // initially we do not know the sizeof(void *), so cast to intptr_t which has the same size to avoid warning
  }

int init_polling(volatile int *exit_polling, const char *capture_path, char *msg_info_fmt)
  {
   int ret_err;
   int mkdir_ret;

   ret_err=get_absolute_path(Full_capture_path, capture_path);
   if(ret_err != 0)
     {
      log_printf("Error obtaining the absolute capture path from %s (by using the current-process executable file path): errno=%d\n", capture_path, ret_err);
      return(ret_err);
     }

   // Create the directory for storing captured images
   mkdir_ret = mkdir(Full_capture_path, 0777);
   if(mkdir_ret == -1 && errno != EEXIST) // If an error occurred and it is different from 'File exists': exit
     {
      ret_err=errno;
      log_printf("The capture file directory (%s) cannot be created: errno=%d\n", Full_capture_path, ret_err);
      return(ret_err);
     }

//   capture_images();

   ret_err=export_gpios(); // This function and configure_gpios() will log error messages
   if(ret_err==0)
     {
      ret_err=configure_gpios();
      if(ret_err==0)
        {
         ret_err=pushover_init(PUSHOVER_CONFIG_FILENAME);
         if(ret_err == 0)
           {
            Msg_info_str[0]='\0'; // Clear message info string so that update_ip_msg can compare it, detect a change and update it with the public IP
            update_ip_msg(msg_info_fmt);



            // Create joinable thread
            ret_err = pthread_create(&Polling_thread_id, NULL, (void *(*)(void *))&polling_thread, (void *)exit_polling);
            if(ret_err == 0) // If success
               log_printf("Polling thread initiated\n");
            else
               log_printf("Error %i creating polling thread: %s\n", ret_err, strerror(ret_err));
           }
        }
     }

   return(ret_err);
  }

int wait_polling_end(void)
  {
   int ret_err;
   ret_err = pthread_join(Polling_thread_id, NULL);
   if(ret_err == 0) // If success
      log_printf("Polling thread terminated correctly\n");
   else
      log_printf("Error waiting for the polling thread to finish\n");
   unexport_gpios();
   return(ret_err);
  }
