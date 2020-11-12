#ifndef PORT_MAPPING_H
#define PORT_MAPPING_H

#define WEB_SERVER_PORT "8008"

// This fn nitializes the UPNP library, finds a valid IGD and print info to log file.
// This fn must be called before calling any of the other library functions.
int init_UPNP(char *wan_address);

// free resources of UPNP library.
void terminate_UPNP(void);

// Print the current port mapping of the IGD
int print_UPNP_mapping(void);

// Add the port mapping to allow the access to the web server from the Internet
int add_UPNP_mapping(void);

// Remove from the IGD the port mapping for the web server
int delete_UPNP_mapping(void);

#endif
