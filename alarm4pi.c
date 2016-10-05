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

#include "bcm_gpio.h"
#include "log_msgs.h"
#include "port_mapping.h"
#include "public_ip.h"

pid_t Child_process_id[2]={-1,-1}; // Initialize to -1 in order no to send signals if no child process created
char * const Capture_exec_args[]={"nc", "-l", "-p", "8080", "-v", "-v", NULL};
char * const Web_server_exec_args[]={"nc", "-l", "-p", "8008", "-v", "-v", NULL};

static volatile sig_atomic_t Exit_daemon_loop=0; // When Break is pressed this var is set to 1 by the Control Handler to exit any time consuming loop

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
         //TODO: Implement a working signal handler */
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

void kill_processes(pid_t *process_ids, size_t n_processes)
  {
   int n_child;
   for(n_child=0;n_child<n_processes;n_child++)
      if(process_ids[n_child] != -1)
         kill(process_ids[n_child], SIGTERM);   
  }

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
      log_printf("waitpid: ret: %i\n",wait_ret);
      if(wait_ret != -1) // Valid PID returned
        {
         int n_child;

         for(n_child=0;n_child<n_processes;n_child++)
            if(process_ids[n_child] != -1) // A process is remaining in the list
              {
               if(process_ids[n_child] == wait_ret) // Is the process that has just died?
                  process_ids[n_child] = -1; // Mark process as dead
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

static void exit_deamon_handler(int sig)
  {
   log_printf("Signal %i received: Sending TERM signal to children.\n", sig);
   kill_processes(Child_process_id, sizeof(Child_process_id)/sizeof(pid_t));
   Exit_daemon_loop = 1;
  }

static void timer_handler(int signum)
  {
   static int count = 0;
   log_printf ("timer expired %d times\n", ++count); 
  }

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


int main(int argc, char *argv[])
  {
   int repeat = 10;
   int main_err;
   int value;
   char wan_address[INET6_ADDRSTRLEN];
   pid_t capture_proc, web_server_proc;
   

   //main_err=daemonize("/");
   main_err=daemon(0,0);
   if(main_err == 0)
     {
      //config_UPNP(NULL);
      //if(get_public_ip(wan_address)==0)
      //   printf("Public IP: %s\n", wan_address);
      //return(0);
      ///syslog(LOG_NOTICE, "iAlarm daemon started.");
      
      if(open_log_files())
         syslog(LOG_WARNING, "Error creating log files.");
      
      set_signal_handler();

      if(run_background_command(&capture_proc, Capture_exec_args[0], Capture_exec_args)==0)
        {
         Child_process_id[0]=capture_proc;
         if(run_background_command(&web_server_proc, Web_server_exec_args[0], Web_server_exec_args)==0)
           {
            Child_process_id[1]=web_server_proc;

            log_printf("Waiting for child processes\n");


            wait_processes(Child_process_id, sizeof(Child_process_id)/sizeof(pid_t), 5);
          }
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

     }
   return(main_err);
  }
