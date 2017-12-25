///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <string>

class DialApplication
{
public:
    DialApplication(std::string name, std::string ip_addr);

    std::string get_name() const;
    bool is_running() const;
    void set_running(bool);

    const char *get_status() const;

    void set_additional_data(const char *);
    std::string additional_data() const;

    void set_additional_data_url(const char *);
    std::string additional_data_url() const;

    std::string ip_addr() const;

    virtual bool launch(std::string body) = 0;
    virtual bool kill() = 0;

protected:
    bool m_running;

private:
    std::string m_name;
    std::string m_ip_addr;
    std::string m_additional_data;
    std::string m_additional_data_url;
};
