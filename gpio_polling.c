#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include "bcm_gpio.h"
#include "log_msgs.h"

pthread_t Polling_thread_id;
#define PIR_POLLING_PERIOD_SEC 1
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
   int pir_value;

   event_printf("GPIO server initiated\n");

   read_err=0; // Default thread return value
   while(*exit_polling == 0) // While exit signal not triggered
     {
      // Read GPIO value
      ret_err = GPIO_read(PIR_GPIO, &pir_value);
      if(ret_err == 0) // Success reading
        {
         event_printf("GPIO read %i\n", pir_value);

        }
      else
        {
         if(read_err==0) // Error has not been logged yet
           {
            log_printf("Error %i while reading GPIO %i: %s\n", ret_err, PIR_GPIO, strerror(ret_err));
            read_err=ret_err;
           }
        }
 
      sleep(PIR_POLLING_PERIOD_SEC);
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
