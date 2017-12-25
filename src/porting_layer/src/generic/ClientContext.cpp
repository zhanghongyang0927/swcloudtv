///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include <porting_layer/ClientContext.h>
#include <porting_layer/AutoLock.h>
#include <porting_layer/TimeStamp.h>
#include <porting_layer/Thread.h>

#include <utils/utils.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>

using namespace ctvc;

static const char *DEFAULT_LOG_FORMAT = "<%T> Type:<%t> at %f:%l, %F%[, Message:<%m>%]\r\n";

ClientContext &ClientContext::instance()
{
    static ClientContext s_instance;
    return s_instance;
}

ClientContext::ClientContext() :
    m_log_format(DEFAULT_LOG_FORMAT)
{
    srand(static_cast<unsigned int>(TimeStamp::now().get_as_microseconds()));
}

ClientContext::~ClientContext()
{
}

void ClientContext::set_manufacturer(const char *manufacturer)
{
    m_manufacturer = manufacturer;
}

const char *ClientContext::get_manufacturer() const
{
    return m_manufacturer.c_str();
}

void ClientContext::set_device_type(const char *devicetype)
{
    m_devicetype = devicetype;
}

const char *ClientContext::get_device_type() const
{
    return m_devicetype.c_str();
}

void ClientContext::set_unique_id(const char *unique_id)
{
    m_unique_id = unique_id;
}

const char *ClientContext::get_unique_id() const
{
    return m_unique_id.c_str();
}

void ClientContext::set_ca_path(const char *path)
{
    m_ca_path = path ? path : "";
}

const char *ClientContext::get_ca_path() const
{
    return m_ca_path.c_str();
}

void ClientContext::set_ca_client_path(const char *path)
{
    m_ca_client_path = path ? path : "";
}

const char *ClientContext::get_ca_client_path() const
{
    return m_ca_client_path.c_str();
}

void ClientContext::set_private_key_path(const char *path)
{
    m_private_key_path = path ? path : "";
}

const char *ClientContext::get_private_key_path() const
{
    return m_private_key_path.c_str();
}

void ClientContext::register_log_output(ILogOutput &log_output)
{
    AutoLock lck(m_mutex);

    m_log_outputs.insert(&log_output);
}

void ClientContext::unregister_log_output(ILogOutput &log_output)
{
    AutoLock lck(m_mutex);

    m_log_outputs.erase(&log_output);
}

void ClientContext::set_log_format(const char *log_format)
{
    AutoLock lck(m_mutex);

    m_log_format = log_format ? log_format : DEFAULT_LOG_FORMAT;
}

void ClientContext::log_message(LogMessageType message_type, const char *file, int line, const char *function, const char *message) const
{
    AutoLock lck(m_mutex);

    std::string log_message;
    bool is_copy_mode = true;
    for (size_t i = 0; i < m_log_format.size(); i++) {
        char c = m_log_format[i];
        if (c != '%') {
            if (is_copy_mode) {
                // Copy all characters that are not '%', if in copy mode
                log_message += c;
            }
        } else if (++i < m_log_format.size()) {
            char c = m_log_format[i];
            switch (c) {
            case 'T': // Print time
                if (is_copy_mode) {
                    struct timeval tv;
                    tv.tv_sec = 0;
                    tv.tv_usec = 0;
                    gettimeofday(&tv, 0); // Don't check error code as logging any error would recursively get into here

                    time_t t = tv.tv_sec;
                    struct tm *tm_info = localtime(&t);
                    char timestamp[25];
                    strftime(timestamp, sizeof(timestamp), "%H:%M:%S", tm_info);

                    string_printf_append(log_message, "%s.%03u", timestamp, static_cast<unsigned int>(tv.tv_usec / 1000U));
                }
                break;
            case 't': // Print type
                if (is_copy_mode) {
                    switch (message_type) {
                    case LOG_ERROR:
                        log_message += "error";
                        break;
                    case LOG_WARNING:
                        log_message += "warning";
                        break;
                    case LOG_INFO:
                        log_message += "info";
                        break;
                    case LOG_DEBUG:
                        log_message += "debug";
                        break;
                    }
                }
                break;
            case 'F': // Print function name
                if (function && is_copy_mode) {
                    // Strip the return value and the arguments from the __PRETTY_FUNCTION__ string but keep the class and namespace
                    const char *e = strchr(function, '('); // End of the function name == start of the argument list
                    if (e) {
                        // Skip type & return value (all space-separated words before the function name) if present
                        const char *s = function;
                        const char *s2 = strchr(s, ' ');
                        while (s2 && s2 < e) {
                            s = s2 + 1;
                            s2 = strchr(s, ' ');
                        }
                        // Skip * and & signature, if present
                        while (s && (*s == '*' || *s == '&')) {
                            s++;
                        }
                        log_message += std::string(s).substr(0, e - s);
                        log_message += "()";
                    }
                }
                break;
            case 'f': // Print file name
                if (file && is_copy_mode) {
                    log_message += file;
                }
                break;
            case 'l': // Print line number
                if (is_copy_mode) {
                    string_printf_append(log_message, "%d", line);
                }
                break;
            case 'n': {// Print the current thread name
                Thread *current_thread = Thread::self();
                if (current_thread) {
                    log_message += current_thread->get_name();
                } else {
                    log_message += "main";
                }
            }
                break;
            case 'm': // Print the message
                if (message) {
                    log_message += message;
                }
                break;
            case '[': // Keep copy mode if message has non-zero length, otherwise mute copying
                is_copy_mode = message && (message[0] != '\0');
                break;
            case ']': // Restore copy mode
                is_copy_mode = true;
                break;
            default:
                if (is_copy_mode) {
                    // Copy all escape character that are not recognized, if in copy mode
                    log_message += c;
                }
                break;
            }
        }
    }

    // Forward the log message to all registered outputs
    for (std::set<ILogOutput *>::const_iterator i = m_log_outputs.begin(); i != m_log_outputs.end(); ++i) {
        (*i)->log_message(message_type, log_message.c_str());
    }

    // If the log message was not forwarded, print it ourselves.
    if (m_log_outputs.empty()) {
        fputs(log_message.c_str(), stderr);
    }
}

X11KeyMap &ClientContext::get_keymap()
{
    return m_keymap;
}
