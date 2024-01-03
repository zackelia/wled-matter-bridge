#include <iostream>

#include "mdns.hpp"

using namespace wled;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"

static mdns_string_t ipv4_address_to_string(char * buffer, size_t capacity, const struct sockaddr_in * addr, size_t addrlen)
{
    char host[NI_MAXHOST]    = { 0 };
    char service[NI_MAXSERV] = { 0 };
    int ret = getnameinfo((const struct sockaddr *) addr, (socklen_t) addrlen, host, NI_MAXHOST, service, NI_MAXSERV,
                          NI_NUMERICSERV | NI_NUMERICHOST);
    int len = 0;
    if (ret == 0)
    {
        if (addr->sin_port != 0)
            len = snprintf(buffer, capacity, "%s:%s", host, service);
        else
            len = snprintf(buffer, capacity, "%s", host);
    }
    if (len >= (int) capacity)
        len = (int) capacity - 1;
    mdns_string_t str;
    str.str    = buffer;
    str.length = len;
    return str;
}

static mdns_string_t ipv6_address_to_string(char * buffer, size_t capacity, const struct sockaddr_in6 * addr, size_t addrlen)
{
    char host[NI_MAXHOST]    = { 0 };
    char service[NI_MAXSERV] = { 0 };
    int ret = getnameinfo((const struct sockaddr *) addr, (socklen_t) addrlen, host, NI_MAXHOST, service, NI_MAXSERV,
                          NI_NUMERICSERV | NI_NUMERICHOST);
    int len = 0;
    if (ret == 0)
    {
        if (addr->sin6_port != 0)
            len = snprintf(buffer, capacity, "[%s]:%s", host, service);
        else
            len = snprintf(buffer, capacity, "%s", host);
    }
    if (len >= (int) capacity)
        len = (int) capacity - 1;
    mdns_string_t str;
    str.str    = buffer;
    str.length = len;
    return str;
}

static char namebuffer[256];

static int query_callback(int sock, const struct sockaddr * from, size_t addrlen, mdns_entry_type_t entry, uint16_t query_id,
                          uint16_t rtype, uint16_t rclass, uint32_t ttl, const void * data, size_t size, size_t name_offset,
                          size_t name_length, size_t record_offset, size_t record_length, void * user_data)
{
    if (entry == MDNS_ENTRYTYPE_ADDITIONAL)
    {
        if (rtype == MDNS_RECORDTYPE_A)
        {
            struct sockaddr_in addr;
            mdns_record_parse_a(data, size, record_offset, record_length, &addr);
            mdns_string_t addrstr = ipv4_address_to_string(namebuffer, sizeof(namebuffer), &addr, sizeof(addr));
            snprintf((char *) user_data, 40, "%.*s", MDNS_STRING_FORMAT(addrstr));
        }
        if (rtype == MDNS_RECORDTYPE_AAAA)
        {
            struct sockaddr_in6 addr;
            mdns_record_parse_aaaa(data, size, record_offset, record_length, &addr);
            mdns_string_t addrstr = ipv6_address_to_string(namebuffer, sizeof(namebuffer), &addr, sizeof(addr));
            snprintf((char *) user_data, 40, "%.*s", MDNS_STRING_FORMAT(addrstr));
        }
    }

    return 0;
}

MDNS::MDNS()
{
    sock = mdns_socket_open_ipv4(NULL);
    if (sock < 0)
    {
        std::cerr << "mdns_socket_open_ipv4: " << sock << std::endl;
        abort();
    }
}

MDNS::~MDNS()
{
    mdns_socket_close(sock);
}

static char buffer[2048];
bool MDNS::send_query()
{
    // char buffer[2048];
    int ret = mdns_query_send(sock, MDNS_RECORDTYPE_PTR, service.c_str(), service.length(), buffer, sizeof(buffer), 0);
    if (ret)
    {
        std::cerr << "mdns_query_send: " << ret << std::endl;
        return false;
    }
    return true;
}

std::string MDNS::recv_query()
{
    // char buffer[2048];
    char ip[40];
    // sprintf(ip, "%s", "aaaa");
    int ret = mdns_query_recv(sock, buffer, sizeof(buffer), query_callback, ip, 0);
    if (ret < 0)
    {
        std::cerr << "mdns_query_recv: " << ret << std::endl;
        return "";
    }

    return std::string(ip);
}

#pragma GCC diagnostic pop
