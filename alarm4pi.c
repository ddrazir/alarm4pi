// Alarm deamon

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <syslog.h>
#include <arpa/inet.h> //INET6_ADDRSTRLEN
#include <signal.h>
#include <sys/time.h>

#include "bcm_gpio.h"
#include "log_msgs.h"
#include "port_mapping.h"
#include "public_ip.h"
#include "gpio_polling.h"

#define WSERVER_ENV_VAR_NAME "LD_LIBRARY_PATH"
#define WSERVER_ENV_VAR_VAL "/usr/local/lib"
/*
Hello,

On this weekend I have been hacked a preliminary CGI support. Now it is  included in the experimental branch's rev 164.
Only the following variables had been passed to the script:
  "SERVER_SOFTWARE=\"mjpg-streamer\" "
  "SERVER_PROTOCOL=\"HTTP/1.1\" "
  "SERVER_PORT=\"%d\" "  // OK
  "GATEWAY_INTERFACE=\"CGI/1.1\" "
  "REQUEST_METHOD=\"GET\" "
  "SCRIPT_NAME=\"%s\" " // OK
  "QUERY_STRING=\"%s\" " //OK

Adding another server/client related informations (such as SERVER_NAME, REMOTE_HOST, REMOTE_PORT) would make the current code much more difficult. If I guess well the current implementation would statisfy the most of the use cases.

Regards,
Miklós
sudo modprobe bcm2835-v4l2
*/
//#define SENSOR_POLLING_PERIOD_SEC 1
// configure_timer(SENSOR_POLLING_PERIOD_SEC); // Activate timer


pid_t Child_process_id[2] = {-1, -1}; // Initialize to -1 in order not to send signals if no child process created
char * const Capture_exec_args[]={"nc", "-l", "-p", "8080", "-v", "-v", NULL};
char * const Web_server_exec_args[]={"nc", "-l", "-p", "8008", "-v", "-v", NULL};

//char * const Web_server_exec_args[]={"mjpg_streamer", "-i", "input_file.so -f /tmp_ram -n webcam_pic.jpg", "-o", "output_http.so -w /usr/local/www -p 8008", NULL};
//char * const Capture_exec_args[]={"raspistill", "-n", "-w", "640", "-h", "480", "-q", "10", "-o", "/tmp_ram/webcam_pic.jpg", "-bm", "-tl", "700", "-t", "0", "-th", "none", NULL};

// When Break is pressed (or SIGTERM recevied) this var is set to 1 by the signal handler fn to exit loops
volatile int Exit_daemon_loop=0; // We mau use sig_atomic_t in the declaration instead of int, but this is not needed

/*
static int daemonize(char *working_dir)
  {
   int ret_error;
   pid_t child_pid;
   int null_fd_rd, null_fd_wr;

   child_pid = fork(); // Fork off the parent process
   if(child_pid != -1) // If no error occurred
     {
      if(child_pid > 0) // Success: terminate parent
         exit(EXIT_SUCCESS);

      // the the child is running here
      if(setsid() != -1) // creates a session and sets the process group ID
        {
         // Catch, ignore and handle signals
         // TODO: Implement a working signal handler
         signal(SIGCHLD, SIG_IGN);
         signal(SIGHUP, SIG_IGN);

         child_pid = fork(); // Fork off the parent process again
         if(child_pid != -1) // If no error occurred
           {
            if(child_pid > 0) // Success: terminate parent
               exit(EXIT_SUCCESS);

            umask(0); // Set new file permissions

            chdir(working_dir); // Change the working directory to an appropriated directory

            null_fd_rd=open ("/dev/null", O_RDONLY);
            if(null_fd_rd != -1)
              {
               dup2(null_fd_rd, STDIN_FILENO);
               close(null_fd_rd);
              }
            else
               perror("iAlarm daemon init error: could not open null device for reading");
            null_fd_wr=open ("/dev/null", O_WRONLY);
            if(null_fd_wr != -1)
              {
               dup2(null_fd_wr, STDERR_FILENO);
               dup2(null_fd_wr, STDOUT_FILENO);
               close(null_fd_wr);
              }
            else
               perror("iAlarm daemon init error: could not open null device for writing");

           }
         else
           {
            ret_error=errno;
            fprintf(stderr,"iAlarm daemon init error: second fork failed. errno=%d\n",errno);
           }


        }
      else
        {
         ret_error=errno;
         fprintf(stderr,"iAlarm daemon init error: child process could become session leader. errno=%d\n",errno);
        }

     }
   else
     {
      ret_error=errno;
      fprintf(stderr,"iAlarm daemon init error: first fork failed. errno=%d\n",errno);
     }

      
   return(ret_error);
  }
*/

// Sends the SIGTERM signal to a list processes
// The list of PIDs is pointed by process_ids and its length in n_processes
void kill_processes(pid_t *process_ids, size_t n_processes)
  {
   int n_child;
   for(n_child=0;n_child<n_processes;n_child++)
      if(process_ids[n_child] != -1)
         kill(process_ids[n_child], SIGTERM);   
  }

// This function blocks until a list of processes terminate or timeout
// process_ids is a pointer to the PID list of length n_processes
// Returns 0 on success (all child processes have finished) or an errno
// code if on error or timeout
// Warning: This function stops the system timer
int wait_processes(pid_t *process_ids, size_t n_processes, int wait_timeout)
  {
   int ret_error;
   int n_remaining_procs;

   ret_error=0;
   do
     {
      int wait_ret;

      n_remaining_procs=0;
      alarm(wait_timeout);
      wait_ret=waitpid(0, NULL, 0); // wait for any child process whose process group ID is equal to that of the calling process.
      if(wait_ret != -1) // Valid PID returned
        {
         int n_child;

         for(n_child=0;n_child<n_processes;n_child++)
            if(process_ids[n_child] != -1) // A process is remaining in the list
              {
               if(process_ids[n_child] == wait_ret) // Is the process that has just died?
                 {
                  log_printf("Child process with PID: %i terminated.\n", wait_ret);
                  process_ids[n_child] = -1; // Mark process as dead
                }
               else
                  n_remaining_procs++; // We still have to wait for his process
              }
        }
      else
        {
         ret_error=errno; // Error: exit loop
         log_printf("Error waiting for child process to finish. errno %i: %s\n", errno, strerror(errno));
        }
     }
   while(n_remaining_procs > 0);
   return(ret_error);
  }

// This callback function should be called when the main process receives a SIGINT or
// SIGTERM signal.
// Function set_signal_handler() should be called to set this function as the handler of
// these signals
static void exit_deamon_handler(int sig)
  {
   log_printf("Signal %i received: Sending TERM signal to children.\n", sig);
   kill_processes(Child_process_id, sizeof(Child_process_id)/sizeof(pid_t));
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

// Executes a program in a child process
// The PID of the created process is returned after successful execution. new_proc_id must point to
// a var when the new PID will be stored.
// exec_filename must point to a \0 terminated string containing the program filename
// exec_argv is an array of pointers. Each poining to a string containing a program argument.
// The first argument is the program filename and the last one must be a NULL pointer
static int run_background_command(pid_t *new_proc_id, const char *exec_filename, char *const exec_argv[])
  {
   int ret;

   *new_proc_id = fork(); // Fork off the parent process
   
   if(*new_proc_id == 0) // Fork off child
     {
      int null_fd_rd;
      if(Log_file_handle != NULL)
        {
         if(dup2(fileno(Log_file_handle), STDOUT_FILENO) == -1)
            log_printf("Creating process %s: failed redirect standard output. errno=%d\n",exec_filename,errno);
         if(dup2(fileno(Log_file_handle), STDERR_FILENO) == -1)
            log_printf("Creating process %s: failed redirect standard error output. errno=%d\n",exec_filename,errno);
         fclose(Log_file_handle);
        }
      if(Event_file_handle != NULL)
         fclose(Event_file_handle);
      null_fd_rd=open ("/dev/null", O_RDONLY);
      if(null_fd_rd != -1)
        {
         if(dup2(null_fd_rd, STDIN_FILENO) == -1)
            log_printf("Creating process %s: failed redirect standard input. errno=%d\n",exec_filename,errno);
         close(null_fd_rd);
        }
      else
         log_printf("Creating process %s: could not open null device for reading. errno=%d\n",exec_filename,errno);

      close(STDIN_FILENO);
      execvp(exec_filename, exec_argv);
      log_printf("Creating process %s: failed to execute capture program. errno=%d\n",exec_filename,errno);
      exit(errno); // exec failed, exit child
     }
   else
     {
      if(*new_proc_id > 0)
         ret=0; // success
      else // < 0: An error occurred
        {
         ret=errno;
         log_printf("Creating process %s: first fork failed. errno=%d\n",exec_filename,errno);
        }
     }
   return(ret);
  }

// Configure the system real-time timer to send a SIGALRM signal to the current process
// SIGALRM must be handled before calling this function
// this signal will be send each interval_sec seconds
// If interval_sec is negative, the timer is stopped
// The function returns 0 on success, or a errno error code on error
int configure_timer(float interval_sec)
  {
   int ret_error;
   struct itimerval timer_conf;

   if(interval_sec < 0) // Disable the timer
     {
      // Set the timer to zero: A timer whose it_value is zero or the timer expires
      // and it_interval is zero stops
      timer_conf.it_value.tv_sec = 0;
      timer_conf.it_value.tv_usec = 0;
      timer_conf.it_interval.tv_sec = 0;
      timer_conf.it_interval.tv_usec = 0;
     }
   else
     {
      // Configure the timer to expire (ring) after 250 ms the first time
      timer_conf.it_value.tv_sec = 0;
      timer_conf.it_value.tv_usec = 250000;
      // and to expire again every interval_sec seconds
      timer_conf.it_interval.tv_sec = (time_t)interval_sec;
      timer_conf.it_interval.tv_usec = (suseconds_t)((interval_sec-timer_conf.it_interval.tv_sec)*1.0e6);
     }

   // Start a real time timer, which deliver a SIGALARM
   if(setitimer (ITIMER_REAL, &timer_conf, NULL) == 0)
     {
      log_printf("Sensor polling (timer) set to %lis and %lius\n", timer_conf.it_interval.tv_sec, timer_conf.it_interval.tv_usec);
      ret_error=0; // Timer set successfully
     }
   else
     {
      ret_error=errno;
      log_printf("Error setting timer: errno %i: %s\n", errno, strerror(errno));
     }
   return(ret_error);
  }

int main(int argc, char *argv[])
  {
   int main_err;
   char wan_address[INET6_ADDRSTRLEN];
   pid_t capture_proc, web_server_proc;
   
   // main_err=daemonize("/"); // Custom fn, but it causes problems when waiting for child processes
   main_err=daemon(0,0);
   if(main_err == 0)
     {
      syslog(LOG_NOTICE, "iAlarm daemon started.");
      
      if(open_log_files())
         syslog(LOG_WARNING, "Error creating log files.");
      
      set_signal_handler();

      //config_UPNP(NULL);


      if(get_public_ip(wan_address)==0)
         log_printf("Public IP address: %s\n", wan_address);

      if(setenv(WSERVER_ENV_VAR_NAME, WSERVER_ENV_VAR_VAL, 0) != 0)
         log_printf("Error setting envoronment variable for child process. Errno=%i\n", errno);

      if(run_background_command(&capture_proc, Capture_exec_args[0], Capture_exec_args)==0)
        {
         Child_process_id[0]=capture_proc;
         log_printf("Child process %s executed\n", Capture_exec_args[0]);
         if(run_background_command(&web_server_proc, Web_server_exec_args[0], Web_server_exec_args)==0)
           {
            Child_process_id[1]=web_server_proc;

            log_printf("Child process %s executed\n", Web_server_exec_args[0]);
          }
        }
      main_err = init_polling(&Exit_daemon_loop);
      if(main_err == 0) // Success
        {
         wait_polling_end();
        }
      else
         log_printf("Polling thread has not been created.\n");



      sleep(10);

      log_printf("Waiting for child processes to finish\n");

      configure_timer(-1); // Stop timer

      // Wait until created process terminate or time out
      // The system timer (used for polling) is stopped by this function
      // 5
      wait_processes(Child_process_id, sizeof(Child_process_id)/sizeof(pid_t), 0);


      close_log_files();
     }
   return(main_err);
  }
