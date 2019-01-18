#ifndef HEALTHCHECKAPI_H
#define HEALTHCHECKAPI_H

#include "net/http_client.h"
#include "net/http_base.h"
#include <string>

namespace supernode
{

class HealthcheckAPI
{
public:
    HealthcheckAPI(const std::string &daemonAddress);

    bool processHealthchecks(const std::string uri, epee::net_utils::http::http_response_info& response_info);

private:
    bool cryptonodeCheck();

    epee::net_utils::http::http_simple_client m_http_client;
};

}

#endif // HEALTHCHECKAPI_H
