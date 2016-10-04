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
#include <syslog.h>
#include <arpa/inet.h> //INET6_ADDRSTRLEN
#include <signal.h>

#include "bcm_gpio.h"
#include "log_msgs.h"
#include "port_mapping.h"
#include "public_ip.h"

pid_t Child_process_id[2]={-1,-1}; // Initialize to -1 in order no to send signals if no child process created

static volatile sig_atomic_t Exit_daemon_loop=0; // When Break is pressed this var is set to 1 by the Control Handler to exit any time consuming loop

static void daemonize(char *working_dir)
  {
   pid_t child_pid;
   int opened_fd;

   child_pid = fork(); // Fork off the parent process
   if(child_pid < 0) // An error occurred
     {
      fprintf(stderr,"iAlarm daemon init error: first fork failed. errno=%d\n",errno);
      exit(EXIT_FAILURE);
     }

   if(child_pid > 0) // Success: terminate parent
      exit(EXIT_SUCCESS);

   // the the child is running here
   if (setsid() < 0) // creates a session and sets the process group ID 
     {
      fprintf(stderr,"iAlarm daemon init error: child process could become session leader. errno=%d\n",errno);
      exit(EXIT_FAILURE);
     }

    // Catch, ignore and handle signals
    //TODO: Implement a working signal handler */
   signal(SIGCHLD, SIG_IGN);
   signal(SIGHUP, SIG_IGN);

   child_pid = fork(); // Fork off the parent process again
   if(child_pid < 0) // An error occurred
     {
      fprintf(stderr,"iAlarm daemon init error: second fork failed. errno=%d\n",errno);
      exit(EXIT_FAILURE);
     }

   if(child_pid > 0) // Success: terminate parent
      exit(EXIT_SUCCESS);

   umask(0); // Set new file permissions

   chdir(working_dir); // Change the working directory to an appropriated directory

   for(opened_fd = sysconf(_SC_OPEN_MAX); opened_fd>0; opened_fd--) // Close all open file descriptors 
      close (opened_fd);
  }



static void exit_deamon_handler(int sig)
  {
   int n_child;
   log_printf("Signal %i received: Sending TERM signal to children and exiting parent.\n", sig);
   for(n_child=0;n_child<sizeof(Child_process_id)/sizeof(pid_t);n_child++)
      if(Child_process_id[n_child] != -1)
         kill(Child_process_id[n_child], SIGTERM);
   Exit_daemon_loop = 1;
  }

int set_signal_handler(void)
  {
   int ret;
   struct sigaction act;

   memset (&act, '\0', sizeof(act));

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

static int run_webcam_server(pid_t *capture_proc, pid_t *streamer_proc)
  {
   int ret;

   *capture_proc = fork(); // Fork off the parent process
   
   if(*capture_proc == 0) // Fork off child
     {
      if(Log_file_handle != NULL)
        {
         if(dup2(fileno(Log_file_handle), STDOUT_FILENO) == -1)
            log_printf("Creating webcam server: failed redirect standard output. errno=%d\n",errno);
         if(dup2(fileno(Log_file_handle), STDERR_FILENO) == -1)
            log_printf("Creating webcam server: failed redirect standard error output. errno=%d\n",errno);
         fclose(Log_file_handle);
        }

      execlp("nc", "nc", "-l", "-p", "8008", "-v", "-v", (char *)NULL);
      log_printf("Creating webcam server: failed to execute capture program. errno=%d\n",errno);
      exit(errno); // exec failed, exit child
     }
   else
     {
      if(*capture_proc > 0)
         ret=0; // success
      else // < 0: An error occurred
        {
         ret=errno;
         log_printf("Creating webcam server: first fork failed. errno=%d\n",errno);
        }
     }
   return(ret);
  }


int main(int argc, char *argv[])
  {
   int repeat = 10;
   int main_err;
   int value;
   char wan_address[INET6_ADDRSTRLEN];
   pid_t capture_proc;
   

   //daemonize("/");
   //config_UPNP(NULL);
   //if(get_public_ip(wan_address)==0)
   //   printf("Public IP: %s\n", wan_address);
   //return(0);
   ///syslog(LOG_NOTICE, "iAlarm daemon started.");
   
   if(open_log_files())
      syslog(LOG_WARNING, "Error creating log files.");
   
   set_signal_handler();

   if(run_webcam_server(&capture_proc, NULL)==0)
     {
      int stat,retw;
      Child_process_id[0]=capture_proc;
      
      retw=waitpid(capture_proc, &stat, 0);
      perror("Waitpid: ");
      log_printf("CPID: %i STAT: 0x%x RET: %i\n",getpid(), stat,retw);
     }
     
/*
   main_err=export_gpios();
   if(main_err==0)
     {
      main_err=configure_gpios();
      if(main_err==0)
        {
         do
           {
      
       // Write GPIO value
       
              main_err=GPIO_write(RELAY1_GPIO, repeat % 2);
      if (0 != main_err)
                 printf("Error %d: %s\n",main_err,strerror(main_err));

 
      
       // Read GPIO value
       
      GPIO_read(PIR_GPIO,&value);
      log_printf("I'm reading %d in GPIO %d\n", value, PIR_GPIO);
 
      usleep(500 * 1000);
           }
         while (repeat--);
        }
      unexport_gpios();
     }
     */
   close_log_files();
   return(main_err);
  }
