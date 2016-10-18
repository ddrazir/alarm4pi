#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include "bcm_gpio.h"
#include "log_msgs.h"

pthread_t Polling_thread_id;
// The sensor value will be checked each (in seconds):
#define PIR_POLLING_PERIOD_SECS 1
// The sensor value will be remembered during (in periods):
#define PIR_PERMAN_PERS 60
/*
      // Write GPIO value
      ret_err=GPIO_write(RELAY1_GPIO, repeat % 2);
      if (0 != ret_err)
         printf("Error %d: %s\n",ret_err,strerror(ret_err));
*/
void* polling_thread(volatile int *exit_polling)
  {
   int ret_err;
   int read_err;
   int curr_pir_value;
   int last_pir_value;
   int pir_perman_counter;

   event_printf("GPIO server initiated\n");

   read_err = 0; // Default thread return value
   pir_perman_counter = 0; // Sensor not activated
   last_pir_value = 0; // Assume that the sensor if off at the beginning
   while(*exit_polling == 0) // While exit signal not triggered
     {
      // Read GPIO value
      ret_err = GPIO_read(PIR_GPIO, &curr_pir_value);
      if(ret_err == 0) // Success reading
        {
         if(curr_pir_value != last_pir_value) // Sensor output changed
           {
            if(curr_pir_value != 0)
               event_printf("GPIO PIR (%i) value: %i\n", PIR_GPIO, curr_pir_value);
            last_pir_value = curr_pir_value;
           }

         if(curr_pir_value != 0) // Sensor output activated, remember its value
            pir_perman_counter = PIR_PERMAN_PERS;

        }
      else
        {
         if(read_err==0) // Error has not been logged yet
           {
            log_printf("Error %i while reading GPIO %i: %s\n", ret_err, PIR_GPIO, strerror(ret_err));
            read_err=ret_err;
           }
        }
      sleep(PIR_POLLING_PERIOD_SECS);
      if(pir_perman_counter > 0)
         pir_perman_counter--;
     }

   event_printf("GPIO server terminated with error code: %i\n", read_err);
   return((void *)(intptr_t)read_err); // we do not know the sizeof(void *) in principle, so cast to intptr_t which has the same sizer to avoid warning
  }

int init_polling(volatile int *exit_polling)
  {
   int ret_err;

   ret_err=export_gpios(); // This function and configure_gpios() will log error messages 
   if(ret_err==0)
     {
      ret_err=configure_gpios();
      if(ret_err==0)
        {
         // Create joinable thread
         ret_err = pthread_create(&Polling_thread_id, NULL, (void *(*)(void *))&polling_thread, (void *)exit_polling);
         if(ret_err == 0) // If success
            log_printf("Polling thread initiated\n");
         else
            log_printf("Error %i creating polling thread: %s\n", ret_err, strerror(ret_err));
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
