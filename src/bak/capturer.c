/* capturer.c
 *
 * Raspberry Pi OS 64-bit
 * Requires: libcurl
 *
 * Default snapshot URL:
 *   http://127.0.0.1:8008/?action=snapshot
 *
 * You can override the URL with the environment variable CAPTURER_URL.
 */

#include <curl/curl.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "capturer.h"

#ifndef CAPTURER_DEFAULT_URL
#define CAPTURER_DEFAULT_URL "http://127.0.0.1:8008/?action=snapshot"
#endif

#ifndef CAPTURER_USER_AGENT
#define CAPTURER_USER_AGENT "capturer/1.0"
#endif

#ifndef CAPTURER_CONNECT_TIMEOUT_MS
#define CAPTURER_CONNECT_TIMEOUT_MS 3000L
#endif

#ifndef CAPTURER_TIMEOUT_MS
#define CAPTURER_TIMEOUT_MS 10000L
#endif

static struct
  {
   int initialized;
   char capture_path[PATH_MAX + 1];
   char snapshot_url[512];
  } g_capturer =
  {
   0,
   {0},
   {0}
  };

static int build_path(char *full_path, size_t full_path_size, const char *dir_path, const char *file_name)
  {
   int ret_error;
   int written;

   if(full_path == NULL || dir_path == NULL || file_name == NULL)
      return(EINVAL);

   written = snprintf(full_path, full_path_size, "%s/%s", dir_path, file_name);
   if(written < 0)
      return(EIO);

   if((size_t)written >= full_path_size)
      return(ENAMETOOLONG);

   ret_error = 0;
   return(ret_error);
  }

static int build_tmp_template(char *tmp_path, size_t tmp_path_size, const char *dir_path, const char *file_name)
  {
   int ret_error;
   int written;

   if(tmp_path == NULL || dir_path == NULL || file_name == NULL)
      return(EINVAL);

   written = snprintf(tmp_path, tmp_path_size, "%s/.%s.partXXXXXX", dir_path, file_name);
   if(written < 0)
      return(EIO);

   if((size_t)written >= tmp_path_size)
      return(ENAMETOOLONG);

   ret_error = 0;
   return(ret_error);
  }

static int ensure_directory_exists(const char *path)
  {
   struct stat st;

   if(path == NULL || path[0] == '\0')
      return(EINVAL);

   if(stat(path, &st) == 0)
     {
      if(S_ISDIR(st.st_mode))
         return(0);

      return(ENOTDIR);
     }

   if(errno != ENOENT)
      return(errno);

   if(mkdir(path, 0755) != 0)
      return(errno);

   return(0);
  }

static int validate_filename(const char *filename_to_save)
  {
   if(filename_to_save == NULL || filename_to_save[0] == '\0')
      return(EINVAL);

   if(strlen(filename_to_save) > PATH_MAX)
      return(ENAMETOOLONG);

   if(strchr(filename_to_save, '/') != NULL)
      return(EINVAL);

   return(0);
  }

static int curlcode_to_errno(CURLcode code)
  {
   switch(code)
     {
      case CURLE_OK:
         return(0);

      case CURLE_OPERATION_TIMEDOUT:
         return(ETIMEDOUT);

      case CURLE_COULDNT_RESOLVE_HOST:
         return(EHOSTUNREACH);

      case CURLE_COULDNT_CONNECT:
         return(ECONNREFUSED);

      case CURLE_URL_MALFORMAT:
         return(EINVAL);

      case CURLE_OUT_OF_MEMORY:
         return(ENOMEM);

      case CURLE_WRITE_ERROR:
         return(EIO);

      case CURLE_ABORTED_BY_CALLBACK:
         return(ECANCELED);

      case CURLE_HTTP_RETURNED_ERROR:
         return(EIO);

      default:
         return(EIO);
     }
  }

static size_t curl_write_to_file(char *ptr, size_t size, size_t nmemb, void *userdata)
  {
   FILE *file_stream;
   size_t written;

   if(ptr == NULL || userdata == NULL)
      return(0);

   file_stream = (FILE *)userdata;
   written = fwrite(ptr, size, nmemb, file_stream);

   if(written != nmemb)
      return(0);

   return(written);
  }

int capturer_init(const char *full_capture_path)
  {
   int ret_error;
   const char *env_url;

   if(full_capture_path == NULL || full_capture_path[0] == '\0')
      return(EINVAL);

   if(strlen(full_capture_path) > PATH_MAX)
      return(ENAMETOOLONG);

   if(g_capturer.initialized != 0)
      capturer_deinit();

   ret_error = curl_global_init(CURL_GLOBAL_DEFAULT);
   if(ret_error != CURLE_OK)
      return(EIO);

   ret_error = ensure_directory_exists(full_capture_path);
   if(ret_error != 0)
     {
      curl_global_cleanup();
      return(ret_error);
     }

   memset(g_capturer.capture_path, 0, sizeof(g_capturer.capture_path));
   memset(g_capturer.snapshot_url, 0, sizeof(g_capturer.snapshot_url));

   strcpy(g_capturer.capture_path, full_capture_path);

   env_url = CAPTURER_DEFAULT_URL;

   if(strlen(env_url) >= sizeof(g_capturer.snapshot_url))
     {
      curl_global_cleanup();
      memset(g_capturer.capture_path, 0, sizeof(g_capturer.capture_path));
      return(ENAMETOOLONG);
     }

   strcpy(g_capturer.snapshot_url, env_url);
   g_capturer.initialized = 1;

   return(0);
  }

int capturer_get_frame(const char *filename_to_save)
  {
   int ret_error;
   int tmp_fd;
   int file_fd;
   FILE *capture_file;
   CURL *curl_handle;
   CURLcode curl_result;
   char full_path[PATH_MAX + 1];
   char tmp_template[PATH_MAX + 1];

   if(g_capturer.initialized == 0)
      return(EPERM);

   ret_error = validate_filename(filename_to_save);
   if(ret_error != 0)
      return(ret_error);

   ret_error = build_path(full_path, sizeof(full_path), g_capturer.capture_path, filename_to_save);
   if(ret_error != 0)
      return(ret_error);

   ret_error = build_tmp_template(tmp_template, sizeof(tmp_template), g_capturer.capture_path, filename_to_save);
   if(ret_error != 0)
      return(ret_error);

   tmp_fd = mkstemp(tmp_template);
   if(tmp_fd < 0)
      return(errno);

   capture_file = fdopen(tmp_fd, "wb");
   if(capture_file == NULL)
     {
      ret_error = errno;
      close(tmp_fd);
      unlink(tmp_template);
      return(ret_error);
     }

   curl_handle = curl_easy_init();
   if(curl_handle == NULL)
     {
      fclose(capture_file);
      unlink(tmp_template);
      return(ENOMEM);
     }

   ret_error = 0;

   if(curl_easy_setopt(curl_handle, CURLOPT_URL, g_capturer.snapshot_url) != CURLE_OK)
      ret_error = EIO;
   if(ret_error == 0 && curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, curl_write_to_file) != CURLE_OK)
      ret_error = EIO;
   if(ret_error == 0 && curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, capture_file) != CURLE_OK)
      ret_error = EIO;
   if(ret_error == 0 && curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L) != CURLE_OK)
      ret_error = EIO;
   if(ret_error == 0 && curl_easy_setopt(curl_handle, CURLOPT_FAILONERROR, 1L) != CURLE_OK)
      ret_error = EIO;
   if(ret_error == 0 && curl_easy_setopt(curl_handle, CURLOPT_NOSIGNAL, 1L) != CURLE_OK)
      ret_error = EIO;
   if(ret_error == 0 && curl_easy_setopt(curl_handle, CURLOPT_CONNECTTIMEOUT_MS, CAPTURER_CONNECT_TIMEOUT_MS) != CURLE_OK)
      ret_error = EIO;
   if(ret_error == 0 && curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT_MS, CAPTURER_TIMEOUT_MS) != CURLE_OK)
      ret_error = EIO;
   if(ret_error == 0 && curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, CAPTURER_USER_AGENT) != CURLE_OK)
      ret_error = EIO;

   if(ret_error == 0)
     {
      curl_result = curl_easy_perform(curl_handle);
      if(curl_result != CURLE_OK)
         ret_error = curlcode_to_errno(curl_result);
     }

   curl_easy_cleanup(curl_handle);

   if(ret_error != 0)
     {
      fclose(capture_file);
      unlink(tmp_template);
      return(ret_error);
     }

   if(fflush(capture_file) != 0)
     {
      ret_error = errno;
      fclose(capture_file);
      unlink(tmp_template);
      return(ret_error);
     }

   file_fd = fileno(capture_file);
   if(file_fd >= 0)
     {
      if(fsync(file_fd) != 0)
        {
         ret_error = errno;
         fclose(capture_file);
         unlink(tmp_template);
         return(ret_error);
        }
     }

   if(fclose(capture_file) != 0)
     {
      ret_error = errno;
      unlink(tmp_template);
      return(ret_error);
     }

   if(rename(tmp_template, full_path) != 0)
     {
      ret_error = errno;
      unlink(tmp_template);
      return(ret_error);
     }

   return(0);
  }

void capturer_deinit(void)
  {
   if(g_capturer.initialized == 0)
      return;

   memset(g_capturer.capture_path, 0, sizeof(g_capturer.capture_path));
   memset(g_capturer.snapshot_url, 0, sizeof(g_capturer.snapshot_url));
   g_capturer.initialized = 0;

   curl_global_cleanup();
  }
