///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include "dialapplication.h"

DialApplication::DialApplication(std::string name, std::string ip_addr) :
    m_running(false),
    m_name(name),
    m_ip_addr(ip_addr)
{
}

std::string DialApplication::get_name() const
{
    return m_name;
}

bool DialApplication::is_running() const
{
    return m_running;
}

void DialApplication::set_running(bool running)
{
    m_running = running;
}

const char *DialApplication::get_status() const
{
    return m_running ? "running" : "stopped";
}

void DialApplication::set_additional_data(const char *data)
{
    m_additional_data = data;
}

std::string DialApplication::additional_data() const
{
    return m_additional_data;
}

void DialApplication::set_additional_data_url(const char *url)
{
    m_additional_data_url = url;
}

std::string DialApplication::additional_data_url() const
{
    return m_additional_data_url;
}

std::string DialApplication::ip_addr() const
{
    return m_ip_addr;
}
