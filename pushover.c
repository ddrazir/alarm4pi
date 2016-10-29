#include <stdio.h> /* printf, sprintf */
#include <stdlib.h> /* exit, atoi, malloc, free */
#include <unistd.h> /* read, write, close */
#include <string.h> /* memcpy, memset */
#include <errno.h>
#include <limits.h> // max hostname length
#include <sys/socket.h> /* socket, connect */
#include <netinet/in.h> /* struct sockaddr_in, struct sockaddr */
#include <arpa/inet.h> // for inet_ntoa (deprecated)
#include <netdb.h> /* struct hostent, gethostbyname */

#include "log_msgs.h"
#include "public_ip.h"
#include "pushover.h"

// The macro TOSTRING allows us to convert a literal number to a string containg that number (used to set fscanf string limits)
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#define MAX_CONF_STR_LEN 80
#define MAX_URL_LEN 2083
#define SERVER_URL_START "http://"
#define DEFAULT_SERVER_PORT 80

// Global variables
char Token_id[MAX_CONF_STR_LEN+1];
char User_id[MAX_CONF_STR_LEN+1];
struct in_addr Server_ip;
int Server_port;

int pushover_init(char *conf_filename)
  {
   int ret_error;
   FILE *conf_fd;
   conf_fd=fopen(conf_filename, "rt");
   if(conf_fd != NULL)
     {
      char server_url[MAX_URL_LEN+1];

      // Init variables to empty strings.
      // If they are not empty after loading, we assume that they have been correctly loaded
      server_url[0]='\0';
      // Delete global string variables
      Token_id[0]='\0';
      User_id[0]='\0';

      ret_error = 0;
      while(!feof(conf_fd) && ret_error == 0)
        {
         // Try to read any of the recognized variables
         // It is necessary that all the variables names start with a different letter, so that
         // fscanf does not get chars from file buffer if the corresponding variable is not readed
         if(fscanf(conf_fd, " server_url: %" TOSTRING(MAX_URL_LEN) "s\n", server_url) == 0 &&
            fscanf(conf_fd, " token: %" TOSTRING(MAX_CONF_STR_LEN) "s\n", Token_id) == 0 &&
            fscanf(conf_fd, " user: %" TOSTRING(MAX_CONF_STR_LEN) "s\n", User_id) == 0)
           {
            log_printf("Error loading Pushover config file: unknown variable name found in file\n");
            ret_error = EINVAL; // Exit loop
           }
        }
      if(ret_error == 0) // No error so far
        {
         if(strlen(server_url) > 0) // If Pushover server URL could be loaded
           {
            if(strlen(Token_id) > 0) // If token ID loaded
              {
               if(strlen(User_id) > 0) // If user ID loaded
                 {

                  printf("%s-%s-%s.\n", server_url, Token_id, User_id);
                  if(strncmp(server_url, SERVER_URL_START, strlen(SERVER_URL_START)) == 0) // URL seems to be correct
                    {
                     char server_name[HOST_NAME_MAX+1];
                     char *hostname_start_ptr, *hostname_end_ptr;
                     size_t server_name_len;

                     // Parse URL to get host name and port
                     // Look for the server hostname start in the URL 
                     hostname_start_ptr=strchr(server_url+strlen(SERVER_URL_START),'@');
                     if(hostname_start_ptr == NULL) // If not user name found, assume it is the end of the URL
                        hostname_start_ptr=server_url+strlen(SERVER_URL_START);
                     else // @ sign found, skip it to get hostname start
                        hostname_start_ptr++;

                     // Look for ':' (end of domain name and beginning of port)
                     hostname_end_ptr=strchr(hostname_start_ptr,':');
                     if(hostname_end_ptr == NULL) // If port not found, search for path start (/)
                       {
                        Server_port=DEFAULT_SERVER_PORT;
                        // Look for '/' (end of domain name)
                        hostname_end_ptr=strchr(hostname_start_ptr,'/');
                        if(hostname_end_ptr == NULL) // If path not found, assume hostname end is the end of the URL
                           hostname_end_ptr=hostname_start_ptr+strlen(hostname_start_ptr);
                       }
                     else // Port number specified
                       {
                        if(sscanf(hostname_end_ptr+1,"%i",&Server_port) == 0) // Port could not be obtained
                           Server_port=DEFAULT_SERVER_PORT;
                       }

                     server_name_len=hostname_end_ptr-hostname_start_ptr;
                     printf("%p %p %p\n",server_url,hostname_start_ptr,hostname_end_ptr);
                     if(server_name_len <= HOST_NAME_MAX)
                       {
                        memcpy(server_name, hostname_start_ptr, server_name_len);
                        server_name[server_name_len]='\0';

                        // Resolve Pushover hostname
                        ret_error=hostname_to_ip(server_name, &Server_ip);
                        if(ret_error==0)
                          {
                           log_printf("Using Pushover server %s for notifications\n",inet_ntoa(Server_ip));
                          }
                       }
                     else
                       {
                        log_printf("Error loading Pushover config file: server URL is too long (more than " TOSTRING(HOST_NAME_MAX) " characters)\n");
                        ret_error = EINVAL;
                       }
                    }
                  else
                    {
                     log_printf("Error loading Pushover config file: server URL start is not "SERVER_URL_START"\n");
                     ret_error = EINVAL;
                    }

                 }
               else
                 {
                  log_printf("Error loading Pushover config file: user id not found\n");
                  ret_error = EINVAL;
                 }
              }
            else
              {
               log_printf("Error loading Pushover config file: token id not found\n");
               ret_error = EINVAL;
              }
           }
         else
           {
            log_printf("Error loading Pushover config file: server URL not found\n");
            ret_error = EINVAL;
           }
        }
      fclose(conf_fd);
     }
   else
     {
      log_printf("Error opening Pushover config file %s: errno=%d\n",conf_filename, errno);
      ret_error=errno;
     }

   return(ret_error);
  }

void error(const char *msg) { perror(msg); exit(0); }

int send_notification()
{
    int ret_error=0;

    /* first where are we going to send it? */
    int portno = 8008;
    char *host = "localhost";

    struct hostent *server;
    struct sockaddr_in serv_addr;
    int sockfd, bytes, sent, received, total, message_size;
    char *message, response[4096];


    /* How big is the message? */
    message_size=1024;
    /* allocate space for the message */
    message=malloc(message_size);

    /* fill in the parameters */

    
    sprintf(message,"%s %s HTTP/1.0\r\n",
        "POST",                  /* method         */
        "/");                    /* path           */
    sprintf(message+strlen(message),"Content-Length: %lu\r\n",strlen("hola"));
    strcat(message,"\r\n");                                /* blank line     */
        strcat(message,"hola");                           /* body           */
    

    /* What are we going to send? */
    printf("Request:\n%s\n",message);

    /* create the socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) error("ERROR opening socket");

    /* lookup the ip address */
    server = gethostbyname(host);
    if (server == NULL) error("ERROR, no such host");

    /* fill in the structure */
    memset(&serv_addr,0,sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(portno);
    memcpy(&serv_addr.sin_addr.s_addr,server->h_addr,server->h_length);

    /* connect the socket */
    if (connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0)
        error("ERROR connecting");

    /* send the request */
    total = strlen(message);
    sent = 0;
    do {
        bytes = write(sockfd,message+sent,total-sent);
        if (bytes < 0)
            error("ERROR writing message to socket");
        if (bytes == 0)
            break;
        sent+=bytes;
    } while (sent < total);

    /* receive the response */
    memset(response,0,sizeof(response));
    total = sizeof(response)-1;
    received = 0;
    do {
        bytes = read(sockfd,response+received,total-received);
        if (bytes < 0)
            error("ERROR reading response from socket");
        if (bytes == 0)
            break;
        received+=bytes;
    } while (received < total);

    if (received == total)
        error("ERROR storing complete response from socket");

    /* close the socket */
    close(sockfd);

    /* process response */
    printf("Response:\n%s\n",response);

    free(message);
    return ret_error;
}