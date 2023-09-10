#include "ifaddrs.h"
#include "string.h"
#include "netinet/in.h"
#include "arpa/inet.h"
#include "logger.cpp"

void printServerIpAddresses()
{
    // credits for interface ip adress: 
    // https://dev.to/fmtweisszwerg/cc-how-to-get-all-interface-addresses-on-the-local-device-3pki
    logger.info("Server IP addresses:");
    struct ifaddrs *ptr_ifaddrs = NULL;

    int result = getifaddrs(&ptr_ifaddrs);
    if (result != 0)
    {
        logger.error("getifaddrs() failed: ");
    }
    
    for (
        struct ifaddrs *ptr_entry = ptr_ifaddrs;
        ptr_entry != NULL;
        ptr_entry = ptr_entry->ifa_next)
    {
        char ipaddress_human_readable_form[256];
        char netmask_human_readable_form[256];

        char interface_name[256];
        strcpy(interface_name, ptr_entry->ifa_name) ;
        sa_family_t address_family = ptr_entry->ifa_addr->sa_family;
        if (address_family == AF_INET)
        {
            // IPv4

            // Be aware that the `ifa_addr`, `ifa_netmask` and `ifa_data` fields might contain nullptr.
            // Dereferencing nullptr causes "Undefined behavior" problems.
            // So it is need to check these fields before dereferencing.
            if (ptr_entry->ifa_addr != NULL)
            {
                char buffer[INET_ADDRSTRLEN] = {
                    0,
                };
                inet_ntop(
                    address_family,
                    &((struct sockaddr_in *)(ptr_entry->ifa_addr))->sin_addr,
                    buffer,
                    INET_ADDRSTRLEN);

                strcpy(ipaddress_human_readable_form, buffer);
            }
            logger.info("%s: IP address = %s", interface_name, ipaddress_human_readable_form);
        }
        else if (address_family == AF_INET6)
        {
            //Temporarily disabling printing of IPv6 addresses
            continue;
            // IPv6
            uint32_t scope_id = 0;
            if (ptr_entry->ifa_addr != NULL)
            {
                char buffer[INET6_ADDRSTRLEN] = {
                    0,
                };
                inet_ntop(
                    address_family,
                    &((struct sockaddr_in6 *)(ptr_entry->ifa_addr))->sin6_addr,
                    buffer,
                    INET6_ADDRSTRLEN);

                strcpy(ipaddress_human_readable_form,buffer);
                scope_id = ((struct sockaddr_in6 *)(ptr_entry->ifa_addr))->sin6_scope_id;
            }
            logger.info("%s: IP address = %s", interface_name, ipaddress_human_readable_form);
        }
        else
        {
            // AF_UNIX, AF_UNSPEC, AF_PACKET etc.
            // If ignored, delete this section.
        }
    }

    freeifaddrs(ptr_ifaddrs);
}