// Library (for Raspberry Pi OS 64 bits) that uses the Curl library to
// capture one camera frame and store it on a local directory.
// It assumes that the mjpg-streamer (from https://github.com/ArduCAM/mjpg-streamer
// which is already included in the alarm4pi project) is running. alarm4pi launches it.
// It is equivalent to browse the streamer HTTP file:
//<html>
//<head><title>alarm4pi static img.</title></head>
//<body>
// <center><img src="./?action=snapshot" alt="This is a static snapshot" /></center>
//</body>
//</html>

#ifndef CAPTURER_H
#define CAPTURER_H

// This function initializes the capturer client library
// full_capture_path: Directory where frames will be stored
// The fn returns 0 on success or a errno error code
int capturer_init(const char *full_capture_path);

// This function connect web server and download one frame and stores it.
// filename_to_save is a pointer to a \0-terminated string containing the name of the file to create
// The fn returns 0 on success or a errno error code
int capturer_get_frame(const char *filename_to_save);

// This function deinitializes the capturer client library
void capturer_deinit(void);

#endif // CAPTURER_H
