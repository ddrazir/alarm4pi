#ifndef PORT_MAPPING_H
#define PORT_MAPPING_H

#define WEB_SERVER_PORT "8008"

// This fn initializes the UPNP library, finds a valid IGD and print info to log file.
// This fn must be called before calling any of the other library functions.
// wan_address is a pointer to an array where this fn will store the wide area
// network IP address. Enough space must be available in the array to store this
// address. If the caller is not interested in this information, wan_address can
// be NULL.
// This fn returns 0 on success, or an UPNP error code.
int init_UPNP(char *wan_address);

// This fn frees the resources allocated by the UPNP library.
// After calling this fn, init_UPNP() must be called again before calling other
// librry functions.
// This fn returns 0 on success, or -1 if the library was not initialized.
int terminate_UPNP(void);

// Print the current port mapping of the IGD
// This fn returns 0 on success, -1 if the library was not initialized, or an
// UPNP error code on error.
int print_UPNP_mapping(void);

// Add the port mapping to allow the access to the web server from the Internet.
// This fn returns 0 on success, -1 if the library was not initialized, or an
// UPNP error code on error.
int add_UPNP_mapping(void);

// Remove from the IGD the port mapping for the web server.
// This fn returns 0 on success, -1 if the library was not initialized, or an
// UPNP error code on error.
int delete_UPNP_mapping(void);

#endif
