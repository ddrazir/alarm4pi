#ifndef OWNCLOUD_H
#define OWNCLOUD_H

// This function initializes the owncloud client library.
// conf_filename is a pointer to a \0-terminated string containing the filename (and path) of a text
// configuration file containing: the DAV URL of the owncloud server where the captured images will
// be synchronized, the user ID (login) of the owncloud user in the server and the user password.
// The server URL must start with http://, https://, owncloud:// or ownclouds://
// The file must has a format similar to the one of the following example:
//server_url=https://myowncloudserver.com/remote.php/webdav/captures/
//user=johnbrown
//password=1234
//
// full_capture_path is the absolute path of the directory where the captured images are expeted to be.
// The fn returns 0 on success or a errno error code.
// The fn reports errors in the log files.
int owncloud_init(const char *conf_filename, const char *full_capture_path);

// This function synchronizes the content of the captured images directory with the owncloud server
// directory specified in the server_url variable of the config file.
// The fn returns 0 on success or a errno error code.
// The fn reports errors in the log files.
int upload_captures(void);

#endif // OWNCLOUD_H
