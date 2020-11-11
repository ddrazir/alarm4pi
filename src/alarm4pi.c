// Main source code file of the project alarm4pi version 0.2
// Alarm deamon main

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>

#include "log_msgs.h"
#include "port_mapping.h"
#include "gpio_polling.h"
#include "proc_helper.h"

// Uncomment the following define to activate the reverse tunneling connection
// for the web server. This is usefull when your Internet service provider is
// using CG-NAT (carrier grade network address translation) and the Raspnerry
// Pi cannot receive incomming connections from the Internet
//#define REVERSE_TUNNELING 1


// Only needed when the web server is installed
#define WSERVER_ENV_VAR_NAME "LD_LIBRARY_PATH"
#define WSERVER_ENV_VAR_VAL "/usr/local/lib"
// Location of the mjpg_streamer web server directory that contains the executable and plugins.
// We must precede it by "./" to indicate that it is a path relative to the current executable directory.
// Use "" if the server is installed in the operating system directory
#define WEB_SERVER_BIN_PATH "./mjpg-streamer-master/mjpg-streamer-experimental/"


// Directory where the log files will be created (or appended)
#define LOG_FILE_PATH "./log/"

//#define SENSOR_POLLING_PERIOD_SEC 1
// configure_timer(SENSOR_POLLING_PERIOD_SEC); // Activate timer

// List of child processes:
pid_t Child_process_ids[2] = {-1, -1}; // Initialize to -1 in order not to send signals if no child process was created

char * const Web_server_exec_args[]={WEB_SERVER_BIN_PATH"mjpg_streamer", "-i", WEB_SERVER_BIN_PATH"input_raspicam.so", "-o", WEB_SERVER_BIN_PATH"output_http.so -w ./www -p "WEB_SERVER_PORT, NULL}; // WEB_SERVER_PORT is defined in port_mapping.h
char * const Tunneling_exec_args[]={"socketxp", "connect", "http://localhost:"WEB_SERVER_PORT, NULL};

// When Break is pressed (or SIGTERM recevied) this var is set to 1 by the signal handler fn to exit loops
volatile int Exit_daemon_loop=0; // We may use sig_atomic_t in the declaration instead of int, but this is not needed


// This callback function should be called when the main process receives a SIGINT or
// SIGTERM signal.
// Function set_signal_handler() should be called to set this function as the handler of
// these signals
static void exit_deamon_handler(int sig)
  {
   log_printf("Signal %i received: Sending TERM signal to children.\n", sig);
   kill_processes(Child_process_ids, sizeof(Child_process_ids)/sizeof(pid_t));
   Exit_daemon_loop = 1;
  }

// This callback function is the handler of the SIGALRM signal
// Function set_signal_handler() should be called to set this function as the handler of
// these signals
static void timer_handler(int signum)
  {
   static int count = 0;
   log_printf ("timer expired %d times\n", ++count);
  }

// Sets the signal handler functions of SIGALRM, SIGINT and SIGTERM
int set_signal_handler(void)
  {
   int ret;
   struct sigaction act;

   memset (&act, '\0', sizeof(act));

   act.sa_handler = timer_handler;
   sigaction(SIGALRM, &act, NULL);

   act.sa_handler = exit_deamon_handler;
   // If the signal handler is invoked while a system call or library function call is blocked,
   // then the we want the call to be automatically restarted after the signal handler returns
   // instead of making the call fail with the error EINTR.
   act.sa_flags=SA_RESTART;
   sigaction(SIGINT, &act, NULL);
   if(sigaction(SIGTERM, &act, NULL) == 0)
      ret=0;
   else
     {
      ret=errno;
      log_printf("Error setting termination signal handler. errno=%d\n",errno);
     }
   return(ret);
  }


int main(int argc, char *argv[])
  {
   int main_err;

   // main_err=daemonize("/"); // Custom fn, but it causes problems when waiting for child processes
   main_err=daemon(0,0);
   if(main_err == 0)
     {
      pid_t child_proc_id;
      char info_msg_fmt[160]; // Message string that will be sent to the user as a initial notification
      size_t num_child_processes = 0; // Number of child processes currently created by alarm4pi
      char **exec_abs_args;
      int run_prog_ret;

      // info_msg_fmt containt a format string with an explicit conversion specifier %s for the public server IP
      snprintf(info_msg_fmt, sizeof(info_msg_fmt), "Server: http://%%s:"WEB_SERVER_PORT);
      syslog(LOG_NOTICE, "alarm4pi daemon started.");

      open_log_files(LOG_FILE_PATH);

      set_signal_handler();

      config_UPNP(NULL);

      if(setenv(WSERVER_ENV_VAR_NAME, WSERVER_ENV_VAR_VAL, 0) != 0)
         log_printf("Error setting envoronment variable for child process. Errno=%i\n", errno);

      exec_abs_args = replace_relative_path_array(Web_server_exec_args);
      run_prog_ret = run_background_command(&child_proc_id, exec_abs_args[0], exec_abs_args);
      free_substring_array(exec_abs_args );
      if(run_prog_ret == 0)
        {
         Child_process_ids[num_child_processes++]=child_proc_id;
         log_printf("Web server child process %s executed\n", Web_server_exec_args[0]);
#ifdef REVERSE_TUNNELING
           {
            size_t cur_info_msg_fmt_len;

            cur_info_msg_fmt_len=strlen(info_msg_fmt);
            snprintf(info_msg_fmt+cur_info_msg_fmt_len, sizeof(info_msg_fmt)-cur_info_msg_fmt_len, " Tunneling: ");
            cur_info_msg_fmt_len=strlen(info_msg_fmt);
            // Execute the tunneling program and add its text output to the notification message string
            run_prog_ret = run_background_command_out_array(&child_proc_id, info_msg_fmt+cur_info_msg_fmt_len, sizeof(info_msg_fmt)-cur_info_msg_fmt_len, Tunneling_exec_args[0], Tunneling_exec_args);
            if(run_prog_ret == 0)
              {
               Child_process_ids[num_child_processes++]=child_proc_id;
               log_printf("Reverse-tunneling child process %s executed\n", Tunneling_exec_args[0]);
               log_printf("%s\n", info_msg_fmt + cur_info_msg_fmt_len);
              }
           }
#endif
       }

      main_err = init_polling(&Exit_daemon_loop, info_msg_fmt);
      if(main_err == 0) // Success
        {
         wait_polling_end();
        }
      else
         log_printf("Polling thread has not been created.\n");



      sleep(1);

      log_printf("Waiting for child processes to finish\n");

      configure_timer(-1); // Stop timer

      // Wait until created process terminate or time out
      // The system timer (used for polling) is stopped by this function
      // 5
      wait_processes(Child_process_ids, sizeof(Child_process_ids)/sizeof(pid_t), 0);


      close_log_files();
      syslog(LOG_NOTICE, "alarm4pi daemon ended.");
     }
   return(main_err);
  }
