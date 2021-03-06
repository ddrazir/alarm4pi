#ifndef PROC_HELPER_H
#define PROC_HELPER_H

#include <stdlib.h>
#include <time.h>

// Header of functions for managing processes and related operations.

// Sleeps for the requested number of milliseconds.
// It returns 0 on success. In case of error -1 is returned and errno is set
// accordingly.
int millisleep(unsigned long ms_pause);

// Get the full path of the directory where the main executable is found.
int get_current_exec_path(char *exec_path, size_t path_buff_len);

// Convert the specified path to an absolute path, for this, the directory
// where the current executable file is located is used as the current
// directory.
// *orig_file_path is the original path. It must not be longer than MAX_PATH.
// If it start with '\' it is returned unchanged.
// full_abs_path is the array where the resultant path will be returned.
// if must have capacity to store at least MAX_PATH+1 characters.
// If the fn succeeds, 0 is returned, otherwise an errno code is returned.
// If an error different from EINVAL occurs, the original path is returned
// in the full_abs_apth. EINVAL is returned if orig_file_apth is NULL or
// it contains a string longer than MAX_PATH.
int get_absolute_path(char *full_abs_path, const char *orig_file_path);

// Search for a substring (as a word start) inside of a string and
// return a pointer to the substring inside the string. If it is not found,
// NULL is returned.
// A substring occurrence is only conbsidered valid and returned if it is
// at the beginning of the string or preceded by a space.
const char *initial_strstr(const char *str, const char *sub_str);

// Return the number of times that a substring is found inside a string as
// a start of a word.
size_t count_initial_substrings(const char *str, const char *sub_str);

// Replace all occurrences of old_sub_str found in src_str by new_sub_str.
// Only occurrences at the beginning of a word are consedered.
// The resultant string is stored in dest_str, so this array is supposed
// to have enough space to store the resultant string.
void replace_initial_substrings(char *dest_str, const char *src_str, const char *old_sub_str, const char *new_sub_str);

// Replace all occurrences of old_sub_str found in src_str by new_sub_str.
// Only occurrences at the beginning of a word are considered.
// The resultant string is stored in an array dynamically allocated, so
// this array must be freed after utilization.
// A pointer to this array is returned or NULL if its memory could not be
// allocated.
char *alloc_replace_initial_substrings(const char *src_str, const char *old_sub_str, const char *new_sub_str);

// Replace all the occurrences of old_sub_str found in all the strings
// of the src_str_array by new_sub_str.
// Only occurrences at the beginning of a word are considered.
// The resultant strings are stored in an new array dynamically allocated.
// so, this array must be freed after utilization by calling
// free_substring_array.
// A pointer to this array is returned or NULL if its memory could not be
// allocated.
char **replace_initial_substring_array(char * const src_str_array[], const char *old_sub_str, const char *new_sub_str);

// Free the memory allocated by replace_initial_substring_array,
// corresponding to the array and its strings.
void free_substring_array(char **src_str_array);

// Take the argv input argument of execve(), which is an array of argument
// strings to be passed to the new program, and replace the relative paths
// (paths starting with "./") by the current executable absolute path.
// The resultant array, which dynamically allocated, is returned and must
// be freed by calling free_substring_array() after being used.
char **replace_relative_path_array(char * const argv[]);

// This function blocks until all the processes of the list process_ids terminate.
// process_ids is a pointer to the PID list of length n_processes.
// if exit_flag does not becore true, the processes that exit are run again
// after exec_retry_per seconds. A maximum of exec_retries_left process
// execution retries are allowed in total. exec_retries_left=-1 means infinite retries.
// Returns 0 on success (exit condition met with no errors) or an errno
// code if an error occurred.
int wait_child_processes(pid_t *process_ids, char * const * const processes_exec_args[], size_t n_processes, volatile int *exit_flag, time_t exec_retry_per, int exec_retries_left);

// Sends the SIGTERM signal to a list processes
// The list of PIDs is pointed by process_ids and its length in n_processes
void kill_processes(pid_t *process_ids, size_t n_processes);

// Executes a program in a child process and send its output to the log.
// The PID of the created process is returned after successful execution.
// new_proc_id must point to a var where the new PID will be stored.
// exec_filename must point to a \0 terminated string containing the program filename.
// exec_argv is an array of pointers. Each poining to a string containing a program argument.
// The first argument is the program filename and the last one must be a NULL pointer.
// Nota: no declaramos exec_argv como "const char *const []" porque eso le impediría a la
// función recibir un argumento "char **". El compilador impide esto porque la función
// podría hacer a *exec_arg apuntar a una constante y luego, fuera de la función, el argumento
// pasado "char **" podría modificar esta constante, lo cual no está permitido.
int run_background_command_out_log(pid_t *new_proc_id, const char *exec_filename, char *const exec_argv[]);

// Executes a program in a child process and obtains its stdout in an array.
// output_array must point to a char array where the std output of the program
// will be stored. its size must be at least 1. The array is terminated with
// \0 by this function.
// output_array_len must be the length of the allocated array.
// exec_filename must point to a \0 terminated string containing the program filename.
// exec_argv is an array of pointers. Each poining to a string containing a program argument.
// The first argument is the program filename and the last one must be a NULL pointer.
// Returns 0 on success or an errno code.
int run_background_command_out_array(pid_t *new_proc_id, char *output_array, size_t output_array_len, const char *exec_filename, char *const exec_argv[]);

// Executes a program in a child process and feed its stdin with the content of an array.
// input_array must point to the char array whose content will be written
// to the std input of the program. The array must be terminated with \0.
// exec_filename must point to a \0-terminated string containing the program filename.
// exec_argv is an array of pointers. Each poining to a string containing a program argument.
// The first argument is the program filename and the last one must be a NULL pointer.
// Returns 0 on success or an errno code.
int run_background_command_in_array(pid_t *new_proc_id, char *input_array, const char *exec_filename, char *const exec_argv[]);

// fn equivalent to the system fn daemon()
int daemonize(char *working_dir);

#endif // PROC_HELPER
