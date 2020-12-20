#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h> // for readlink, read, write
#include <errno.h> // for errno var and value definitions
#include <libgen.h> // For basename
#include <linux/limits.h> // For PATH_MAX
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

int millisleep(unsigned long ms_pause)
  {
   struct timespec time_to_wait;
   int ret_err;

   time_to_wait.tv_sec = ms_pause / 1000;
   time_to_wait.tv_nsec = (ms_pause % 1000) * 1000000;

   do
     {
      ret_err = nanosleep(&time_to_wait, &time_to_wait);
     }
   // continue looping if a signal has interrupted nanosleep and at least 1 ms is left
   while(ret_err == -1 && errno == EINTR && (time_to_wait.tv_sec > 0 || time_to_wait.tv_nsec >= 1000000));

   if(ret_err == -1 && errno == EINTR)
      ret_err = 0;

   return(ret_err);
  }

// Get the time elapsed since a specified moment using a monotonic clock.
// If since_time is NULL, just the time is measured from a particular
// unspecified start moment.
// If the function fails obtaining the time, returns a zero time.
struct timespec get_elapsed_time(struct timespec *since_time_ptr)
  {
   struct timespec time_diff, curr_time;

   if(clock_gettime(CLOCK_MONOTONIC, &curr_time) == 0)
     {
      if(since_time_ptr != NULL)
        {
         time_diff.tv_sec = curr_time.tv_sec - since_time_ptr->tv_sec;
         time_diff.tv_nsec = curr_time.tv_nsec - since_time_ptr->tv_nsec;
         if(time_diff.tv_nsec < 0)
           {
            time_diff.tv_sec--;
            time_diff.tv_nsec += 1000000000L;
           }
        }
      else
         time_diff = curr_time;
     }
   else
     {
      time_diff.tv_sec = 0;
      time_diff.tv_nsec = 0;
     }

   return(time_diff);
  }

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

int get_absolute_path(char *full_abs_path, const char *orig_file_path)
  {
   int ret_error;
   if(orig_file_path == NULL || strlen(orig_file_path) > PATH_MAX)
      return(EINVAL);

   if(orig_file_path[0] != '/') // Relative path specified: we fist obtain the current executable $
     {
      ret_error = get_current_exec_path(full_abs_path, PATH_MAX);
      if(ret_error == 0) // Directory of current executable successfully obtained
        {
         if(strlen(full_abs_path)+strlen(orig_file_path) <= PATH_MAX) // we check that the total p$
            strcat(full_abs_path, orig_file_path);
         else // Error path too long: we will try to open files with relative path anyway
            strcpy(full_abs_path, orig_file_path);
        }
      else // Error getting executable dir: return the relative path
        {
         ret_error = errno;
         strcpy(full_abs_path, orig_file_path);
        }
     }
   else // Absolute path specified: return it with no changes
      strcpy(full_abs_path, orig_file_path);

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

int update_child_processes_state(pid_t *process_ids, size_t n_processes)
  {
   int proc_status; // status of child process
   int wait_ret;

   // -1=check for any child process. WNOHANG=return immediately even if no child has exited
   // Loop while a new child process is reported to have changed its state
   while((wait_ret = waitpid(-1, &proc_status, WNOHANG)) > 0)
     {
      // Valid process ID returned: a child changed its state
      if(WIFEXITED(proc_status) || WIFSIGNALED(proc_status)) // Did this process exit?
        {
         int n_child;
         // We check if the process that exited was a process executed by alarm4pi
         for(n_child=0;n_child<n_processes;n_child++)
            // Is this process of the list the one that has just exited?
            if(process_ids[n_child] == wait_ret)
              {
               if(WIFEXITED(proc_status)) // The child porcess exited normally
                  log_printf("The executed child process num. %i with PID=%i has exited normally returning a status code=%i.\n", n_child, wait_ret, WEXITSTATUS(proc_status));
               else // They child process was terminated by a signal
                  log_printf("The executed child process num. %i with PID=%i was terminated by the signal %i.\n", n_child, wait_ret, WTERMSIG(proc_status));
               process_ids[n_child] = -1; // Mark process as dead
               break;
              }
         if(n_child == n_processes) // Process not found in the child list
            log_printf("The terminated child process with PID=%i is not in the executed child list.\n", wait_ret);
        }
     }
   if(wait_ret == -1)
     {
      if(errno == ECHILD) // There is no child processes: this is not an error
         wait_ret=0;
      else
         wait_ret=errno;
     }
   return(wait_ret);
  }

// This fn seach the list of PIDs process_ids for child processes that have
// exited (PID=1) and run them again.
// The length of process_ids is n_processes.
// processes_exec_args is an array of pointers to the argv list of arguments for
// each process of the n_processes.
// This fn returns the number of processes that it has tried to create (execute),
// or -1 on error, in this case errno is set.
int exec_exited_child_processes(pid_t *process_ids, char * const * const processes_exec_args[], size_t n_processes)
  {
   int proc_ind;
   int n_run_procs; // number of processes than this fn has tried to run

   n_run_procs=0; // deafult return value=0
   for(proc_ind=0;proc_ind<n_processes &&  n_run_procs>=0;proc_ind++)
      if(process_ids[proc_ind] == -1)
        {
         int run_cmd_ret;
         char **exec_abs_args;

         exec_abs_args = replace_relative_path_array(processes_exec_args[proc_ind]);
         run_cmd_ret = run_background_command_out_log(process_ids+proc_ind, exec_abs_args[0], exec_abs_args);
         free_substring_array(exec_abs_args);

         if(run_cmd_ret == 0)
            log_printf("Created again child process %s with PID=%i\n", processes_exec_args[proc_ind][0], process_ids[proc_ind]);
         else
           {
            n_run_procs=-1; // Error: exit loop
            log_printf("Error executing again the child process %s. errno %i: %s\n", processes_exec_args[proc_ind][0], errno, strerror(errno));
           }
         n_run_procs++;
        }

   return(n_run_procs);
  }

int count_running_child_processes(pid_t *process_ids, size_t n_processes)
  {
   // Count the number of children that has not exited
   int proc_ind, n_remaining_procs;

   n_remaining_procs = 0;
   for(proc_ind=0;proc_ind<n_processes;proc_ind++)
      if(process_ids[proc_ind] != -1)
         n_remaining_procs++;

   return(n_remaining_procs);
  }

int wait_child_processes(pid_t *process_ids, char * const * const processes_exec_args[], size_t n_processes, volatile int *exit_flag, time_t exec_retry_per, int exec_retries_left)
  {
   int ret_error, fn_ret;
   sigset_t sigchld_signal_set, old_blocked_signals; // POSIX signal sets:
   const struct timespec signal_timeout = { exec_retry_per, 0 };

   fn_ret = sigemptyset(&sigchld_signal_set);
   if(fn_ret == 0)
      fn_ret = sigaddset (&sigchld_signal_set, SIGCHLD); // Set the SIGCHLD signal in the set
   if(fn_ret == -1)
     {
      ret_error=errno; // get waitpid() error code and exit
      log_printf("Error assigning the child-process-exit singal to the set of signals. errno %i: %s\n", ret_error, strerror(ret_error));
      return(ret_error);
     }
   // Specify that the SIGCHLD signal delivery must be blocked for the calling thread so that
   // it is detected by sigtimedwait(), and fetch the previous blocked-signal mask.
   // SIGCHLD is delivered when a child process exits, is interrupted, or resumes after being interrupted.
   fn_ret = sigprocmask(SIG_BLOCK, &sigchld_signal_set, &old_blocked_signals);
   if(fn_ret == -1)
     {
      ret_error=errno; // get sigprocmask error code and exit
      log_printf("Error setting the current thread to block the delivery of child-process-exit signal. errno %i: %s\n", ret_error, strerror(ret_error));
      return(ret_error);
     }

   // Check if any child process exited before blocking the SIGCHLD signal delivery because
   // these will not be detected later by sigtimedwait()
   fn_ret = update_child_processes_state(process_ids, n_processes);
   if(fn_ret != 0)
      log_printf("Error finding out the child processes that have exited immediately. returned error %i: %s\n", fn_ret, strerror(fn_ret));

   ret_error=0;

   // Loop while:
   //  - no error has been reported and
   //    - any child process is still running or
   //    - exit flag not signaled and there are execution retries left
   while(ret_error==0 && (count_running_child_processes(process_ids, n_processes)>0 || (!*exit_flag && (exec_retries_left>0 || exec_retries_left==-1))))
     {
      int wait_ret;

      // wait for any child process to change its running state (SIGCHLD) or timeout
      wait_ret = sigtimedwait(&sigchld_signal_set, NULL, &signal_timeout);
      //log_printf("SIGTIMEDWAIT ret: %i errno: %i: %s\n", wait_ret, errno, strerror(errno));
      if(wait_ret == SIGCHLD)
        {
         fn_ret = update_child_processes_state(process_ids, n_processes);
         if(fn_ret != 0)
            log_printf("Error finding out the child processes that have exited. returned error %i: %s\n", fn_ret, strerror(fn_ret));
        }
      else
         if(wait_ret == -1) // sigtimedwait() indicated an error
           {
            if(errno == EAGAIN) // timeout waiting
              {
               if(!*exit_flag && (exec_retries_left > 0 || exec_retries_left == -1))
                 {
                  fn_ret = exec_exited_child_processes(process_ids, processes_exec_args, n_processes);
                  if(fn_ret > 0 && exec_retries_left > 0) // Some child process has been tried to be created
                     exec_retries_left--;
                 }
              }
            else
              {
               if(errno != EINTR) // we do not consider ENTR an error
                 {
                  ret_error=errno;
                  log_printf("Error waiting for child processes to exit. returned error %i: %s\n", ret_error, strerror(ret_error));
                 }
              }
           }
     }

   fn_ret = sigprocmask(SIG_SETMASK, &old_blocked_signals, NULL);
   if(fn_ret == -1)
     {
      ret_error=errno; // get sigprocmask error code
      log_printf("Error restoring the signals whose delivery will be blocked by the current thread. errno %i: %s\n", ret_error, strerror(ret_error));
     }

   return(ret_error);
  }

int run_background_command_out_log(pid_t *new_proc_id, const char *exec_filename, char *const exec_argv[])
  {
   int ret;

   if(Log_file_handle != NULL)
      fflush(Log_file_handle);
   if(Event_file_handle != NULL)
      fflush(Event_file_handle);

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

      close(STDOUT_FILENO);
      close(STDERR_FILENO);

      log_printf("Failed to execute program %s. errno=%d\n",exec_filename,errno);
      if(Log_file_handle != NULL)
         fclose(Log_file_handle);
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
      if(Log_file_handle != NULL)
         fflush(Log_file_handle);
      if(Event_file_handle != NULL)
         fflush(Event_file_handle);

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

         close(STDOUT_FILENO);
         close(STDERR_FILENO);

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
            close(pipe_stdout[PIPE_READ_END]);
            close(pipe_stdout[PIPE_WRITE_END]);
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
      struct timespec init_time = get_elapsed_time(NULL); // Get the current time
      struct timespec elapsed_time;

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
              {
               if(bytes_read == 0) // end of file indicated: the other end of the pipe has been closed
                  break; // exit loop
               num_output_chars += bytes_read;
              }
            else
               num_fds_ready = -1; // read() failed: indicate an error to exit
           }
         elapsed_time = get_elapsed_time(&init_time); // Get the time elapsed since the fn start
         // log_printf("Elapsed %d s (). Total read %d chars (max. %d). DFs: %d. Errno: %d\n",elapsed_time.tv_sec, num_output_chars, output_array_len, num_fds_ready, errno);

         // we continue looping if:
         //  - (select() returned no error or was interrupted by a signal) and
         //  - the fn has not reached the read timeout and
         //  - there is still free space in the array and
         //  - (select() did not indicate a timeout or num. of read chars if lower than the min.)
        }
      while((num_fds_ready >= 0 || (num_fds_ready == -1 && errno == EINTR)) &&
            elapsed_time.tv_sec < PROC_OUTPUT_READ_TIMEOUT &&
            num_output_chars < output_array_len - 1 &&
            (num_fds_ready != 0 || num_output_chars < MIN_VALID_PROC_OUTPUT));

      if(num_fds_ready >= 0 || (num_fds_ready == -1 && errno == EINTR)) // select() did not return an error: success
         ret=0;
      else // select() or read() returned -1 indicating an error
        {
         ret=errno;
         log_printf("Reading the child-process %s output: the waiting-for-reading operation or the reading operation itself failed. errno=%d\n",exec_filename,errno);
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

int run_background_command_in_fd(pid_t *new_proc_id, int *input_fd, const char *exec_filename, char *const exec_argv[])
  {
   int pipe_stdin[2];
   int ret;

   if(pipe(pipe_stdin) == 0)
    {
      if(Log_file_handle != NULL)
         fflush(Log_file_handle);
      if(Event_file_handle != NULL)
         fflush(Event_file_handle);

      *new_proc_id = fork(); // Fork off the parent process

      if(*new_proc_id == 0) // Fork off child
        {
         // We close the file descriptors of the pipe that the child does not need
         // The child just reads from the pipe, so we close the unused write end
         close(pipe_stdin[PIPE_WRITE_END]);

         if(Log_file_handle != NULL)
           {
            if(dup2(fileno(Log_file_handle), STDOUT_FILENO) == -1)
               log_printf("Creating process %s: child failed to redirect standard output. errno=%d\n",exec_filename,errno);
            if(dup2(fileno(Log_file_handle), STDERR_FILENO) == -1)
               log_printf("Creating process %s: child failed to redirect standard error output. errno=%d\n",exec_filename,errno);
           }
         if(Event_file_handle != NULL)
            fclose(Event_file_handle);

         if(dup2(pipe_stdin[PIPE_READ_END], STDIN_FILENO) == -1)
            log_printf("Creating process %s: child failed to redirect standard input. errno=%d\n",exec_filename,errno);
         close(pipe_stdin[PIPE_READ_END]); // this fd should already be duplicated by dup2() so we close the original fd

         // When executable file is not in the current directory, at least the
         // first execvp() parameter must be preceded by the path to the executable file
         execvp(exec_filename, exec_argv);

         close(STDOUT_FILENO);
         close(STDERR_FILENO);

         log_printf("Failed to execute program %s. errno=%d\n",exec_filename,errno);
         if(Log_file_handle != NULL)
            fclose(Log_file_handle);
         exit(errno); // exec failed, exit child
        }
      else
        {
         if(*new_proc_id > 0)
           {
            // We close the file descriptors of the pipe that the parent does not need
            // The parent just writes to the pipe, so we close the unused end for reading
            close(pipe_stdin[PIPE_READ_END]);

            if(input_fd != NULL)
               *input_fd = pipe_stdin[PIPE_WRITE_END];
            else
               close(pipe_stdin[PIPE_WRITE_END]);

            ret=0; // success
           }
         else // < 0: An error occurred
           {
            ret=errno;
            close(pipe_stdin[PIPE_WRITE_END]);
            close(pipe_stdin[PIPE_READ_END]);
            log_printf("Creating process %s: first fork failed. errno=%d\n",exec_filename,errno);
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

int run_background_command_in_array(pid_t *new_proc_id, char *input_array, const char *exec_filename, char *const exec_argv[])
  {
   int ret;
   int input_fd;

   ret=run_background_command_in_fd(new_proc_id, &input_fd, exec_filename, exec_argv);
   if(ret==0)
     {
      int num_fds_ready;
      size_t num_input_chars = 0;
      struct timespec init_time = get_elapsed_time(NULL); // Get the current time
      struct timespec elapsed_time;
      size_t input_array_len = strlen(input_array);

      do
        {
         fd_set write_fd_set;
         struct timeval write_timeout;

         FD_ZERO(&write_fd_set); // initially we clear all the set
         FD_SET(input_fd, &write_fd_set); // we add our file descriptor to the set

         write_timeout.tv_sec = 0;
         write_timeout.tv_usec = 500000; // Each iteration waits for process output during 0.5 s

         num_fds_ready = select(input_fd + 1, NULL, &write_fd_set, NULL, &write_timeout);
         if(num_fds_ready > 0) // the fd is available for writing
           {
            int bytes_written;
            bytes_written = write(input_fd, input_array + num_input_chars, input_array_len - num_input_chars);
            if(bytes_written >= 0) // write() succeeded
               num_input_chars += bytes_written;
            else
               num_fds_ready = -1; // write() failed: indicate an error to exit
           }
         elapsed_time = get_elapsed_time(&init_time); // Get the time elapsed since the fn start
         // log_printf("Elapsed %d s (). Total written %d chars (max. %d). DFs: %d. Errno: %d\n",elapsed_time.tv_sec, num_input_chars, input_array_len, num_fds_ready, errno);

         // we continue looping if:
         //  - (select() returned no error or was interrupted by a signal) and
         //  - the time is not up and
         //  - there are pending chars to write
        }
      while((num_fds_ready >= 0 || (num_fds_ready == -1 && errno == EINTR)) &&
             elapsed_time.tv_sec < PROC_OUTPUT_READ_TIMEOUT &&
             num_input_chars < input_array_len);

      if(num_fds_ready >= 0 || (num_fds_ready == -1 && errno == EINTR)) // we reached the timeout to write or the array has been fully written: success
         ret=0;
      else // select() or write() returned -1 indicating an error
        {
         ret=errno;
         log_printf("Reading child process %s output: waiting operation or writing operation failed. errno=%d\n",exec_filename,errno);
        }

      // After finshing writing, we must deal with the file descriptor for writing.
      // If we closed it (close(input_fd)), the reader (child process) would get an error or signal when it tries to read.
      close(input_fd);
     }

   return(ret);
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
               perror("alarm4pi daemon init error: could not open null device for reading");
            null_fd_wr=open ("/dev/null", O_WRONLY);
            if(null_fd_wr != -1)
              {
               dup2(null_fd_wr, STDERR_FILENO);
               dup2(null_fd_wr, STDOUT_FILENO);
               close(null_fd_wr);
              }
            else
               perror("alarm4pi daemon init error: could not open null device for writing");
           }
         else
           {
            ret_error=errno;
            fprintf(stderr,"alarm4pi daemon init error: second fork failed. errno=%d\n",errno);
           }
        }
      else
        {
         ret_error=errno;
         fprintf(stderr,"alarm4pi daemon init error: child process could become session leader. errno=%d\n",errno);
        }
     }
   else
     {
      ret_error=errno;
      fprintf(stderr,"alarm4pi daemon init error: first fork failed. errno=%d\n",errno);
     }

   return(ret_error);
  }
