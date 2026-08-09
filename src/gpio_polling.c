#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h> //INET6_ADDRSTRLEN
#include <linux/limits.h> // For PATH_MAX
#include <time.h> // for nanosleep()
// For waitpid():
#include <sys/types.h>
#include <sys/wait.h>
// For mkdir:
#include <sys/stat.h>
#include <sys/types.h>

#include "gpio_control.h"
#include "log_msgs.h"
#include "public_ip.h"
#include "owncloud.h"
#include "pushover2.h"
#include "telegram.h"
#include "capturer.h"
#include "proc_helper.h"

#define PUSHOVER_CONFIG_FILENAME "pushover_conf.txt"
#define OWNCLOUD_CONFIG_FILENAME "owncloud_conf.txt"
#define TELEGRAM_CONFIG_FILENAME "telegram_conf.txt"
#define OWNCLOUD_REMOTE_DIRECTORY "captures"

// The sensor value will be checked each (in milliseconds):
#define PIR_POLLING_PERIOD_SECS 1000

// Switch on the relay 1 (connected to a light switch?) when an alarm sensor
// is activated, that is, any GPIO alarm pin is set to low
// #define RELAY1_ON_ALARM_EVENT

// ID of the thread created by init_polling
pthread_t Polling_thread_id;
// General info string which is added to every msg sent
char Msg_info_str[INET6_ADDRSTRLEN+100];

// Directory where the images captures by the camera will be stored
char Full_capture_path[PATH_MAX+1];

// List of GPIO pins that will the monitored.
// Any change in their values will trigger an alarm event. Example to check
// the 3 inputs:
// const int Alarm_gpios[]={PIR_GPIO, CONTACT1_GPIO, CONTACT2_GPIO};
const int Alarm_gpios[]={CONTACT1_GPIO, CONTACT2_GPIO};

#define MAX_NOTIF_MSG_SIZE ((MAX_PUSHOVER_MSG_SIZE) < (MAX_TELEGRAM_MSG_SIZE) ? (MAX_PUSHOVER_MSG_SIZE) : (MAX_TELEGRAM_MSG_SIZE))

int send_info_notif(char *msg_str, char *msg_priority)
  {
   char tot_msg_str[MAX_NOTIF_MSG_SIZE];
   int ret_err1, ret_err2;

   snprintf(tot_msg_str, MAX_NOTIF_MSG_SIZE, "%s. %s", Msg_info_str, msg_str);

   ret_err1 = send_telegram_message(tot_msg_str);
   ret_err2 = send_pushover_notification(tot_msg_str, msg_priority);
   return((ret_err1)?ret_err1:ret_err2);
  }

int get_current_time(int *hour, int *min, int*sec)
  {
   time_t time_since_epoch;
   struct tm *time_vals;
   int ret_err;

   time(&time_since_epoch);
   time_vals = localtime(&time_since_epoch);
   if(time_vals != NULL)
     {
      if(hour != NULL)
         *hour = time_vals->tm_hour;
      if(min != NULL)
         *min = time_vals->tm_min;
      if(sec != NULL)
         *sec = time_vals->tm_sec;
      ret_err = 0; // Success
     }
   else
      ret_err = errno;

   return(ret_err);
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

#define MAX_TIME_FILENAME_LEN 20

// Obtain a date-and-time string and store it in the specified buffer of specified max length
void get_localtime_filename(char *cur_time_str, size_t cur_time_str_len)
  {
   get_localtime_str(cur_time_str, cur_time_str_len, "%Y-%m-%d_%Hh%Mm%Ss");
  }

#define IMAGE_FILENAME_END ".jpg"
//#define IMAGE_FILENAME_END ".h264"

char *compose_capture_filename(void)
  {
   static char image_filename[MAX_TIME_FILENAME_LEN+sizeof(IMAGE_FILENAME_END)+1];

   get_localtime_filename(image_filename, sizeof(image_filename));
   strcat(image_filename, IMAGE_FILENAME_END);
   return(image_filename);
  }

void capture_images_cmd(char *image_filename)
  {
   char full_image_file_path[PATH_MAX+1];
   // rpicam-vid parameters used:
   // -n: no preview
   // --framerate 5: 5 fps
   // -t 5s: 5 s time recording
   char * const capture_exec_args[]={"rpicam-vid", "-n", "-t", "5s", "--framerate", "5", "-o", full_image_file_path, NULL};
   pid_t capture_proc_id;
   int capture_run_ret;

   if(strlen(Full_capture_path)+strlen(image_filename) < sizeof(full_image_file_path))
     {
      strcpy(full_image_file_path, Full_capture_path);
      strcat(full_image_file_path, image_filename);

      capture_run_ret = run_background_command_out_log(&capture_proc_id, capture_exec_args[0], capture_exec_args);
      if(capture_run_ret == 0)
        {
         int wait_ret;

         wait_ret=waitpid(capture_proc_id, NULL, 0); // wait for the capture process to finish
         if(wait_ret != -1) // Error returned
            log_printf("Presumably, a video file has been stored in %s\n", full_image_file_path);
         else
            log_printf("Error waiting for the capture process to finish\n", capture_exec_args[0]);
        }
      else
         log_printf("Capture child process (%s) could not be executed\n", capture_exec_args[0]);
     }
   else
      log_printf("The complete captured image filename (%s) could not be composed: path too long\n", image_filename);
  }

// This fn is called when an alarm event accurs. Tha is, when an alarm
// GPIO pin chages its value.
// pin: number of GPIO pin that changed
// val: new value that this pin has (0 or 1)
void on_alarm_event(int pin, int val)
  {
   char notif_msg[MAX_NOTIF_MSG_SIZE];
   
#ifdef RELAY1_ON_ALARM_EVENT
   int gpio_read_err;
   int old_gpio_val;
   int at_night = 1; // only switch on the light at night: default value
   int cur_hour;

   if(get_current_time(&cur_hour, NULL, NULL) == 0) // Success reading the time
      if(cur_hour > 8 && cur_hour < 18)
         at_night = 0;
   if(at_night && val != PIN_LOW_VAL)
     {
      // Read the current gpio value before modifying it
      gpio_read_err = gpio_read(RELAY1_GPIO, &old_gpio_val);
      if(gpio_read_err == 0) // Success reading
         if(gpio_write(RELAY1_GPIO, PIN_LOW_VAL) == 0) // switch on the relay. We assume active low
            millisleep(100); // write success: wait for the realy and the bulb to switch on
     }
   else
      gpio_read_err = -1; // The GPIO value has not been read
#endif

   snprintf(notif_msg, sizeof(notif_msg), "Sensor pin (GPIO %i) changed its value to %i\n", pin, val);
   event_printf(notif_msg);
   send_info_notif(notif_msg, "2");

   if(val != PIN_LOW_VAL) // The sensors signal the alarm event with a PIN_HIGH_VAL
     {
      char *cap_filename = compose_capture_filename();
      // Take some photos and store them in the 'captures' directory
      //capture_images_cmd(cap_filename);
      capturer_get_frame(cap_filename);
      // Synchronize (upload) the contant of 'captures' directory with the owncloud server
      upload_capture(cap_filename);
     }

#ifdef RELAY1_ON_ALARM_EVENT
   if(gpio_read_err == 0 && val != PIN_LOW_VAL) // if we succeded reading, restore the old gpio value
      gpio_write(RELAY1_GPIO, old_gpio_val);
#endif
  }

// Number of GPIO pins to monitor
#define NUM_ALARM_GPIOS (sizeof(Alarm_gpios)/sizeof(Alarm_gpios[0]))

void *polling_thread(volatile int *exit_polling)
  {
   int ret_err;
   int read_err;
   int alarm_armed;
   int curr_pin_value;
   int last_pin_value[NUM_ALARM_GPIOS];
   int pin_ind;
   
   event_printf("GPIO monitor initiated\n");

   read_err = 0; // Default thread return value

   for(pin_ind=0;pin_ind<NUM_ALARM_GPIOS;pin_ind++)
      last_pin_value[pin_ind] = 0; // Assume that the sensors are off at the beginning

   while(*exit_polling == 0) // While the exit signal is not triggered:
     {
      // Check if the alarm is armed:
      ret_err = gpio_read(ARMING_GPIO, &alarm_armed);
      if(ret_err == 0) // Success reading
        {
         if(alarm_armed == PIN_LOW_VAL) // If the alarm is armed (GPIO set to low):
           {
            for(pin_ind=0;pin_ind<NUM_ALARM_GPIOS;pin_ind++)
              {     
               // Check if the sensor is activated:
               ret_err = gpio_read(Alarm_gpios[pin_ind], &curr_pin_value);
               if(ret_err == 0) // Success reading
                 {
                  if(curr_pin_value != last_pin_value[pin_ind]) // Sensor output changed
                    {
                     on_alarm_event(Alarm_gpios[pin_ind], curr_pin_value);
                     last_pin_value[pin_ind] = curr_pin_value;
                    }
                 }
               else
                 {
                  if(read_err==0) // No error code has been logged yet
                    {
                     log_printf("Error %i while reading GPIO (%i): %s\n", ret_err, Alarm_gpios[pin_ind], strerror(ret_err));
                     read_err=ret_err;
                    }
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
      millisleep(PIR_POLLING_PERIOD_SECS);
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

   ret_err=open_gpios();
   if(ret_err==0)
     {
      ret_err=configure_gpios();
      if(ret_err==0)
        {
         ret_err=owncloud_init(OWNCLOUD_CONFIG_FILENAME, Full_capture_path, OWNCLOUD_REMOTE_DIRECTORY);
         if(ret_err != 0)
            log_printf("The captured image upload is disabled (error=%d)!\n",ret_err);

         ret_err=pushover_init(PUSHOVER_CONFIG_FILENAME);
         if(ret_err != 0)
            log_printf("The alarm-event notification through Pushover is disabled (error=%d)!\n",ret_err);

         ret_err=telegram_init(TELEGRAM_CONFIG_FILENAME);
         if(ret_err != 0)
            log_printf("The alarm-event notification through Telegram is disabled (erro=%d)!\n",ret_err);

         ret_err=capturer_init(Full_capture_path);
         if(ret_err != 0)
            log_printf("The image downloader (capturer) in case of alarm-event is disabled (erro=%d)!\n",ret_err);

         Msg_info_str[0]='\0'; // Clear message info string so that update_ip_msg can compare it, detect a change and update it with the public IP
         update_ip_msg(msg_info_fmt); // Msg_info_str is updated

         // Create joinable thread
         ret_err = pthread_create(&Polling_thread_id, NULL, (void *(*)(void *))&polling_thread, (void *)exit_polling);
         if(ret_err == 0) // If success
            log_printf("Polling thread initiated\n");
         else
            log_printf("Error %i creating polling thread: %s\n", ret_err, strerror(ret_err));
        }
      else
       log_printf("While configuring direcction of pins error %d: %s\n",ret_err,strerror(ret_err));
     }
   else
      log_printf("While initializing GPIO pin control error %d: %s\n",ret_err,strerror(ret_err));

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
   close_gpios();
   return(ret_err);
  }

void deinit_polling(void)
  {
   owncloud_deinit();
   pushover_deinit();
   telegram_deinit();
   capturer_deinit();
  }
