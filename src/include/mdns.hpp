#pragma once

#include <string>

#include <netdb.h>
#include <sys/socket.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#include <mdns.h>
#pragma GCC diagnostic pop

namespace wled {
class MDNS
{
public:
    MDNS();
    ~MDNS();

    MDNS(const MDNS &)              = delete;
    MDNS & operator=(const MDNS &)  = delete;
    MDNS(MDNS && other)             = delete;
    MDNS & operator=(MDNS && other) = delete;

    int socket() { return sock; }

    bool send_query();
    std::string recv_query();

private:
    const std::string service = "_wled._tcp.local.";
    int sock                  = -1;
};
} // namespace wled
