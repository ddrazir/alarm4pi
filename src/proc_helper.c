#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h> // for readlink
#include <errno.h> // for errno var and value definitions
#include <libgen.h> // For basename
#include <linux/limits.h> // For MAXPATH
#include <sys/time.h>
#include <fcntl.h>
#include <time.h> // for time()

// for daemonize()
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <sys/select.h> // for select()

#include "proc_helper.h"
#include "log_msgs.h"

// Defines for run_background_command_out_array()
#define PROC_OUTPUT_READ_TIMEOUT 12 // Maximum num. of seconds to wait for a program to print its output text
#define MIN_VALID_PROC_OUTPUT 30 // Minimum number of characters that the executed process is expected to print

int get_current_exec_path(char *exec_path, size_t path_buff_len)
  {
   int ret_error;
   if(path_buff_len > 0)
     {
      char exec_path_buff[PATH_MAX+1];
      size_t chars_written;

      chars_written=readlink("/proc/self/exe", exec_path_buff, PATH_MAX);
      if(chars_written != -1) // Success
        {
         char *exec_dir;
         exec_path_buff[chars_written]='\0';
         exec_dir=dirname(exec_path_buff);
         if(path_buff_len > strlen(exec_dir)+1) // If there is enough space in supplied buffer:
           {
            strcpy(exec_path,exec_dir);
            strcat(exec_path,"/");
            ret_error=0;
           }
         else
           {
            exec_path[0]='\0';
            ret_error=EINVAL; // Invalid input parameter (input buffer size)
           }
        }
      else
        {
         exec_path[0]='\0';
         ret_error=errno;
        }
     }
   else
      ret_error=EINVAL;
   return(ret_error);
  }

const char *initial_strstr(const char *str, const char *sub_str)
  {
   const char *sub_str_occ = str;
   while((sub_str_occ = strstr(sub_str_occ, sub_str)) != NULL)
     {
      // if the substring occurrence is not at the beginning of the input string,
      // check that it is preceded by a space char
      if(sub_str_occ != str)
        {
         char prev_char = *(sub_str_occ-1);
         if(prev_char != ' ')
            sub_str_occ++; // discard occurrence and continue searching
         else
            break; // we have found a valid occurrence (with a preceding space); exit
        }
      else
         break; // we have found a valid occurrence (at the beginnig of str): exit
     }
   return(sub_str_occ);
  }

size_t count_initial_substrings(const char *str, const char *sub_str)
  {
   size_t n_strs = 0;
   const char *cur_str = str;
   size_t sub_str_len = strlen(sub_str);
   while((cur_str = initial_strstr(cur_str, sub_str)) != NULL)
    {
      n_strs++;
      cur_str += sub_str_len;
    }
   return(n_strs);
}

void replace_initial_substrings(char *dest_str, const char *src_str, const char *old_sub_str, const char *new_sub_str)
  {
   const char *prev_src_str = src_str;
   char *prev_dest_str = dest_str;
   const char *cur_src_str = src_str;
   size_t old_sub_str_len = strlen(old_sub_str);
   size_t new_sub_str_len = strlen(new_sub_str);
   while((cur_src_str = initial_strstr(prev_src_str, old_sub_str)) != NULL)
     {// For each occurrence of the searched substring in the source string:
      size_t prev_segm_len = cur_src_str - prev_src_str;
      // Copy the segment of string previous to the occurrence
      strncpy(prev_dest_str, prev_src_str, prev_segm_len);
      prev_dest_str += prev_segm_len;
      // Copy the new substring instead of the old sobstring to the
      // destination string
      strncpy(prev_dest_str, new_sub_str, new_sub_str_len);
      prev_dest_str += new_sub_str_len;
      prev_src_str = cur_src_str + old_sub_str_len;
     }
   // copy the remaining part of the source string to the destination string
   // including the '\0' string terminating char
   strcpy(prev_dest_str, prev_src_str);
  }

char *alloc_replace_initial_substrings(const char *src_str, const char *old_sub_str, const char *new_sub_str)
  {
   char *dest_str;
   size_t old_sub_str_len = strlen(old_sub_str);
   size_t new_sub_str_len = strlen(new_sub_str);
   size_t sub_str_occs = count_initial_substrings(src_str, old_sub_str);

   size_t size_dest_str_len = strlen(src_str) - sub_str_occs*old_sub_str_len + sub_str_occs*new_sub_str_len;
   dest_str = (char *)malloc(size_dest_str_len+1); // +1 for the '\0' terminating char
   if(dest_str != NULL)
      replace_initial_substrings(dest_str, src_str, old_sub_str, new_sub_str);

   return(dest_str);
  }

char **replace_initial_substring_array(char * const src_str_array[], const char *old_sub_str, const char *new_sub_str)
  {
   char **new_str_array;
   size_t n_arr_strs;

   // Count the number of strings in the array
   for(n_arr_strs=0;src_str_array[n_arr_strs] != NULL;n_arr_strs++);

   new_str_array = (char **)malloc(sizeof(char *)*(n_arr_strs+1));
   if(new_str_array != NULL)
     {
      size_t arr_str_ind;
      for(arr_str_ind=0;arr_str_ind<n_arr_strs;arr_str_ind++)
        {
         char *new_arr_str = alloc_replace_initial_substrings(src_str_array[arr_str_ind], old_sub_str, new_sub_str);
         new_str_array[arr_str_ind] = new_arr_str;
         if(new_arr_str == NULL) // Error allocating memory: free the already allocated new-array memory and exit
            break;
        }
      if(arr_str_ind == n_arr_strs) // The loop completed successfully
         new_str_array[n_arr_strs] = NULL; // Set the sentinel value (termination mark)
      else // An error occurred: free allocated array memory
        {
         free_substring_array(new_str_array);
         new_str_array = NULL;
        }
     }

   return(new_str_array);
  }

void free_substring_array(char **src_str_array)
  {
   char **arr_str_ptr;
   for(arr_str_ptr=src_str_array;*arr_str_ptr != NULL;arr_str_ptr++)
      free(*arr_str_ptr);
   free(src_str_array);
  }

char **replace_relative_path_array(char * const argv[])
  {
   char cur_abs_path[PATH_MAX+1];
   char *used_cur_abs_path;
   int get_path_err;

   get_path_err = get_current_exec_path(cur_abs_path, sizeof(cur_abs_path));
   if(get_path_err == 0)
      used_cur_abs_path = cur_abs_path;
   else
     {
      log_printf("Substituting relative paths by absolute ones of %s: Error obtaining current path. errno=%d\n",argv[0],errno);
      used_cur_abs_path = ""; // Error obtaining path: do not use absolute paths
     }

   return(replace_initial_substring_array(argv, "./", used_cur_abs_path));
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

int run_background_command(pid_t *new_proc_id, const char *exec_filename, char *const exec_argv[])
  {
   int ret;

   *new_proc_id = fork(); // Fork off the parent process

   if(*new_proc_id == 0) // Fork off child
     {
      int null_fd_rd;
      if(Log_file_handle != NULL)
        {
         if(dup2(fileno(Log_file_handle), STDOUT_FILENO) == -1)
            log_printf("Creating process %s: child failed to redirect standard output. errno=%d\n",exec_filename,errno);
         if(dup2(fileno(Log_file_handle), STDERR_FILENO) == -1)
            log_printf("Creating process %s: child failed to redirect standard error output. errno=%d\n",exec_filename,errno);
         fclose(Log_file_handle);
        }
      if(Event_file_handle != NULL)
         fclose(Event_file_handle);
      null_fd_rd=open ("/dev/null", O_RDONLY);
      if(null_fd_rd != -1)
        {
         if(dup2(null_fd_rd, STDIN_FILENO) == -1)
            log_printf("Creating process %s: child failed to redirect standard input. errno=%d\n",exec_filename,errno);
         close(null_fd_rd);
        }
      else
        {
         log_printf("Creating process %s: child could not open null device for reading. errno=%d\n",exec_filename,errno);
         close(STDIN_FILENO); // The dup2() fn already silently close the new_fd
        }

      // When executable file is not in the current directory, at least the
      // first execvp() parameter must be preceded by the path to the executable file
      execvp(exec_filename, exec_argv);

      log_printf("Creating process %s: failed to execute program. errno=%d\n",exec_filename,errno);
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

#define PIPE_READ_END 0
#define PIPE_WRITE_END 1
int run_background_command_out_fd(pid_t *new_proc_id, int *output_fd, const char *exec_filename, char *const exec_argv[])
  {
   int pipe_stdout[2];
   int ret;

   if(pipe(pipe_stdout) == 0)
    {
      *new_proc_id = fork(); // Fork off the parent process

      if(*new_proc_id == 0) // Fork off child
        {
         int null_fd_rd;

         // The child does not use this file
         if(Event_file_handle != NULL)
            fclose(Event_file_handle);

         // We close the file descriptors of the pipe that the child does not need
         // The child just writes to the pipe, so we close the unused read end
         close(pipe_stdout[PIPE_READ_END]);

         if(dup2(pipe_stdout[PIPE_WRITE_END], STDOUT_FILENO) == -1)
            log_printf("Creating process %s: child failed to redirect standard output. errno=%d\n",exec_filename,errno);
         if(dup2(pipe_stdout[PIPE_WRITE_END], STDERR_FILENO) == -1)
            log_printf("Creating process %s: child failed to redirect standard error output. errno=%d\n",exec_filename,errno);
         close(pipe_stdout[PIPE_WRITE_END]); // this fd should already be duplicated by dup2() so we close the original fd

         null_fd_rd=open ("/dev/null", O_RDONLY);
         if(null_fd_rd != -1)
           {
            if(dup2(null_fd_rd, STDIN_FILENO) == -1)
               log_printf("Creating process %s: child failed to redirect standard input. errno=%d\n",exec_filename,errno);
            close(null_fd_rd);
           }
         else
          {
            log_printf("Creating process %s: child could not open null device for reading. errno=%d\n",exec_filename,errno);
            close(STDIN_FILENO); // dup2() already closes the new_fd, so in case of success closing it is not requiered
          }

         execvp(exec_filename, exec_argv);

         log_printf("Creating process %s: child failed to execute program. errno=%d\n",exec_filename,errno);
         if(Log_file_handle != NULL)
            fclose(Log_file_handle); // We should have previously closed this fd if the execvp() will succeed, but we do not know
         exit(errno); // exec failed, exit child
        }
      else // We are in the parent process
        {
         if(*new_proc_id > 0) // fork() returned the child PID: success
           {
            // We close the file descriptors of the pipe that the parent does not need
            // The parent just reads from the pipe, so we close the unused write end
            close(pipe_stdout[PIPE_WRITE_END]);

            if(output_fd != NULL)
               *output_fd = pipe_stdout[PIPE_READ_END];
            else
               close(pipe_stdout[PIPE_READ_END]);
            ret=0;
           }
         else // < 0: An error occurred
           {
            ret=errno;
            log_printf("Creating process %s: fork failed. errno=%d\n",exec_filename,errno);
           }
        }
     }
   else // pipe() returned -1: An error occurred
     {
      ret=errno;
      log_printf("Creating process %s: pipe creation failed. errno=%d\n",exec_filename,errno);
     }
    return(ret);
}


int run_background_command_out_array(pid_t *new_proc_id, char *output_array, size_t output_array_len, const char *exec_filename, char *const exec_argv[])
  {
   int ret;
   int output_fd;

   ret=run_background_command_out_fd(new_proc_id, &output_fd, exec_filename, exec_argv);
   if(ret==0)
     {
      int num_fds_ready;
      size_t num_output_chars = 0;
      int output_fd_flags;
      time_t init_sec_count = time(NULL);
      time_t num_elapsed_secs;

      do
        {
         fd_set read_fd_set;
         struct timeval read_timeout;

         FD_ZERO(&read_fd_set); // initially we clear all the set
         FD_SET(output_fd, &read_fd_set); // we add our file descriptor to the set

         read_timeout.tv_sec = 0;
         read_timeout.tv_usec = 500000; // Each iteration waits for process output during 0.5 s

         num_fds_ready = select(output_fd + 1, &read_fd_set, NULL, NULL, &read_timeout);
         if(num_fds_ready > 0) // there is some data to read
           {
            int bytes_read;
            bytes_read = read(output_fd, output_array + num_output_chars, output_array_len - num_output_chars - 1);
            if(bytes_read >= 0) // read() succeeded
               num_output_chars += bytes_read;
            else
               num_fds_ready = -1; // read() failed: indicate an error to exit
           }
         num_elapsed_secs = time(NULL) - init_sec_count;
        } // we continue looping if select returned no error (or was interrupted by a signal) and there is space left in the array
      while((num_fds_ready > 0 || (num_fds_ready == -1 && errno == EINTR) || num_output_chars < MIN_VALID_PROC_OUTPUT) && num_elapsed_secs < PROC_OUTPUT_READ_TIMEOUT && num_output_chars < output_array_len - 1);

      if(num_fds_ready >= 0) // select() finally indicated a timeout as expected or the array is full: success
         ret=0;
      else // select() or read() returned -1 indicating an error
        {
         ret=errno;
         log_printf("Creating process %s: fork failed. errno=%d\n",exec_filename,errno);
        }

      output_array[num_output_chars] = '\0';

      // After finshing reading, we must deal with the file descriptor for reading.
      // If we closed it (close(output_fd)), the writer (child process) would get an error or signal when it tries to write.
      // So we just set the pipe as non blocking in case it becomes full
      // (maybe this should be done in the other pipe end, which is no accesible here?)
      output_fd_flags = fcntl(output_fd, F_GETFL, 0);
      fcntl(output_fd, F_SETFL, output_fd_flags | O_NONBLOCK);
     }

   return(ret);
  }

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

// This fn is not used since daemon() is preferred
int daemonize(char *working_dir)
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