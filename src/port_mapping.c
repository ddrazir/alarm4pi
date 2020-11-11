#include <stdio.h>
#include <string.h>
#include <arpa/inet.h> //INET6_ADDRSTRLEN
#include <miniupnpc/miniupnpc.h>
#include <miniupnpc/upnpcommands.h>
#include "log_msgs.h"
#include "port_mapping.h"

char *Port_mappings[][5]=
  { // LAN addr (NULL for local host), WAN port, LAN port, protocol, description
   {NULL, "22", "22", "TCP", "SSH RPi"},
   {NULL, WEB_SERVER_PORT, WEB_SERVER_PORT, "TCP", "webcam RPi"},
   {NULL, NULL, NULL, NULL, NULL}
  };

int config_UPNP(char *wan_address)
  {
   int fn_error = 0;
   //get a list of upnp devices (asks on the broadcast address and returns the responses)
   struct UPNPDev *upnp_dev_list;

// Argument of function upnpDiscover have changed from version 1.9 to version 2.1 of libminiupnpc-dev
#if MINIUPNPC_API_VERSION > 10
   upnp_dev_list = upnpDiscover(1000,    //timeout in milliseconds
                           NULL,    //multicast address, default = "239.255.255.250"
                           NULL,    //minissdpd socket, default = "/var/run/minissdpd.sock"
                           UPNP_LOCAL_PORT_ANY,       //source port, default = 1900
                           0,       //0 = IPv4, 1 = IPv6
                           2,       //TTL default value 2
                           &fn_error); //error output

#else
   upnp_dev_list = upnpDiscover(1000,    //timeout in milliseconds
                           NULL,    //multicast address, default = "239.255.255.250"
                           NULL,    //minissdpd socket, default = "/var/run/minissdpd.sock"
                           0,       //source port, default = 1900
                           0,       //0 = IPv4, 1 = IPv6
                           &fn_error); //error output
#endif

   if(upnp_dev_list != NULL)
     {
      struct UPNPDev *cur_upnp_dev;
      char lan_address[INET6_ADDRSTRLEN]; //maximum length of an ipv6 address string
      struct UPNPUrls upnp_urls;
      struct IGDdatas upnp_data;
      int get_IGD_status;

      log_printf("List of UPNP devices found on the network :\n");
      for(cur_upnp_dev = upnp_dev_list; cur_upnp_dev != NULL; cur_upnp_dev = cur_upnp_dev->pNext)
         log_printf("-Descr. URL: %s\n service type: %s\n", cur_upnp_dev->descURL, cur_upnp_dev->st);

      get_IGD_status = UPNP_GetValidIGD(upnp_dev_list, &upnp_urls, &upnp_data, lan_address, sizeof(lan_address));
      if(get_IGD_status > 0) // 0 = NO IGD found: fn failed
        {
         int get_ip_ret;
         int map_index;
         char wan_address_buf[INET6_ADDRSTRLEN];
         int get_entry_error;

         log_printf(" LAN IP address: %s (according to UPNP IGD)\n",lan_address);
         // get the external (WAN) IP address
         get_ip_ret=UPNP_GetExternalIPAddress(upnp_urls.controlURL, upnp_data.first.servicetype, wan_address_buf);
         if(get_ip_ret == 0)
           {
            log_printf(" External IP: %s (according to UPNP IGD)\n", wan_address_buf);
            if(wan_address != NULL)
               strcpy(wan_address,wan_address_buf);
           }
         else
           {
            log_printf(" Could not get external IP address. ret error=%i\n",get_ip_ret);
            if(wan_address != NULL)
               wan_address[0]='\0';
           }
         fn_error=0;

         // add a new TCP port mappings from WAN port to local port
         for(map_index=0;Port_mappings[map_index][1]!=NULL;map_index++)
           {
            char *curr_lan_addr;

            // Port_mappings[]={LAN addr, WAN port, LAN port, protocol, description}
            curr_lan_addr=(Port_mappings[map_index][0] == NULL)?lan_address:Port_mappings[map_index][0];

            fn_error = UPNP_AddPortMapping(
               upnp_urls.controlURL,
               upnp_data.first.servicetype,
               Port_mappings[map_index][1],  // external (WAN) port requested
               Port_mappings[map_index][2],  // internal (LAN) port to which packets will be redirected
               curr_lan_addr, // internal (LAN) address to which packets will be redirected
               Port_mappings[map_index][4], // text description to indicate why or who is responsible for the port mapping
               Port_mappings[map_index][3], // protocol must be either TCP or UDP
               NULL, // remote (peer) host address or nullptr for no restriction
               "0"); // port map lease duration (in seconds) or zero for "as long as possible"

            if(fn_error)
               log_printf(" Failed to map port num. %i. ret error=%i\n",map_index,fn_error);
           }

         log_printf(" Lan address\tWAN->LANport\tProt\tDur\tEn?\tR.Host\tDescr.\n");
         // list all port mappings

         get_entry_error=0;
         for(map_index = 0;get_entry_error==0;map_index++) // loop until no more port mappings available
           {
            char map_wan_port           [6]  = "";
            char map_lan_address        [16] = "";
            char map_lan_port           [6]  = "";
            char map_protocol           [4]  = "";
            char map_description        [80] = "";
            char map_mapping_enabled    [4]  = "";
            char map_remote_host        [64] = "";
            char map_lease_duration     [16] = ""; // original time, not remaining time :(
            char map_index_str[10];

            sprintf(map_index_str, "%d", map_index);
            get_entry_error = UPNP_GetGenericPortMappingEntry(
                    upnp_urls.controlURL            ,
                    upnp_data.first.servicetype     ,
                    map_index_str                   ,
                    map_wan_port                    ,
                    map_lan_address                 ,
                    map_lan_port                    ,
                    map_protocol                    ,
                    map_description                 ,
                    map_mapping_enabled             ,
                    map_remote_host                 ,
                    map_lease_duration              );
            if(get_entry_error==0)
               log_printf(" %s\t%s -> %s\t%s\t%s\t%s\t\"%s\"\t%s\n", map_lan_address, map_wan_port, map_lan_port, map_protocol,
                                         map_lease_duration, map_mapping_enabled, map_remote_host, map_description);
           }

         FreeUPNPUrls(&upnp_urls);
        }
      else
        {
         log_printf(" No valid Internet Gateway Device could be connected to. fn ret state=%i\n",get_IGD_status);
         fn_error=-1;
        }
      freeUPNPDevlist(upnp_dev_list);
     }
   else
      log_printf("Could not discover upnp device. ret error=%i\n",fn_error);
   return(fn_error);
  }
