///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <porting_layer/DataStore.h>
#include <porting_layer/Log.h>
#include <porting_layer/X11KeyMap.h>
#include <porting_layer/Mutex.h>

#include <string>
#include <set>

namespace ctvc {

/// \brief Log output forwarding interface
struct ILogOutput
{
    virtual ~ILogOutput() {}

    /// \brief Receives a formatted log message string with a severance as indicated by message_type
    /// \param[in] message_type Log level \see Log.h
    /// \param[in] message String containing the log message
    virtual void log_message(LogMessageType message_type, const char *message) = 0;
};

/// \brief ClientContext stores all client-specific context information such as device manufacturer or device
/// model.
///
/// This class follows the singleton pattern, thus it is not necessary to be explicitly passed to the Session
/// object. However, its values shall be filled in before setting up a new session with the CloudTV platform.
class ClientContext
{
public:
    /// \brief Get the one-and-only instance as per the singleton pattern.
    ///
    /// \return Reference to the ClientContext singleton.
    static ClientContext &instance();

    /// \brief Set the device manufacturer, e.g. "Arris".
    /// \note This is a mandatory parameter that must be set by the client.
    /// \param[in] manufacturer String indicating the device manufacturer.
    void set_manufacturer(const char *manufacturer);

    /// \brief Returns the device manufacturer that was previously set.
    /// \return String with the device manufacturer. This is an empty string if it was not previously set.
    const char *get_manufacturer() const;

    /// \brief Set the device type/model, e.g. "VIP1113".
    /// \note This is a mandatory parameter that must be set by the client.
    /// \param[in] devicetype String indicating the device type/model.
    void set_device_type(const char *devicetype);

    /// \brief Returns the device type/model that was previously set.
    /// \return String with device type/model. This is an empty string if it was not previously set.
    const char *get_device_type() const;

    /// \brief Set the unique device identifier, e.g. the MAC hardware address or serial number.
    /// \note This is a mandatory parameter that must be set by the client.
    /// \param[in] unique_id String indicating the unique device identifier.
    void set_unique_id(const char *unique_id);

    /// \brief Returns the unique device identifier that was previously set.
    /// \return String with the unique device identifier. This is an empty string if it was not previously set.
    const char *get_unique_id() const;

    /// \brief Set the path to the CA certificate file, in PEM format
    /// \param[in] path Name of the file containing the certificate
    void set_ca_path(const char *path);

    /// \brief Returns the path to the CA certificate file that was previously set.
    /// \return String indicating the path to the CA certificate file. This is an emtpy string if it was not previously set.
    const char *get_ca_path() const;

    /// \brief Set the path to the TLS certificate file, in PEM format
    /// \param[in] path Name of the file containing the certificate
    void set_ca_client_path(const char *path);

    /// \brief Returns the path to the TLS certificate file.
    /// \return String with the path to the TLS certificate file. This is an empty string if it was not previously set.
    const char *get_ca_client_path() const;

    /// \brief Set the path to the TLS private key file, in PEM format
    /// \param[in] path Name of the file containing the key
    void set_private_key_path(const char *path);

    /// \brief Returns the path to the TLS private key file.
    /// \return String with the path to the TLS private key file. This is an empty string if it was not previously set.
    const char *get_private_key_path() const;

    /// \brief Registers a private logging output with the porting layer
    /// \param[in] log_output Reference to an ILogOutput interface
    /// Re-registering an object that was already registered has no effect.
    void register_log_output(ILogOutput &log_output);

    /// \brief Unregisters a private logging output with the porting layer
    /// \param[in] log_output Reference to an ILogOutput interface
    /// Re-unregistering an object that was already unregistered has no effect.
    void unregister_log_output(ILogOutput &log_output);

    /// \brief Sets the log formatting string. All successive log messages will be printed using this format.
    /// \param[in] log_format The format string to specify. If 0, the default format is selected.
    /// The format string uses a simplified printf-style format:
    /// All characters will be copied as-is, except when prepended with a '%' character or when explicitly excluded..
    /// The '%' character escape sequences are the following:
    /// "%%": Print a single '%' character.
    /// "%T": Print the time of the log message (in hh:mm:ss.ms format).
    /// "%t": Print the type (debug, info, warning, error) of the log message.
    /// "%F": Print the function name from which the log was called.
    /// "%f": Print the file name from which the log was called.
    /// "%l": Print the line number from which the log was called.
    /// "%n": Print the name of the thread from which the log was called.
    /// "%m": Print the log message contents.
    /// "%[": Text to include if a log message is output (and excluded if an empty log message is output).
    /// "%]": End of text section to include if a log message is output.
    ///
    /// \note The default log format is "<%T> Type:<%t> at %f:%l, %F%[, Message:<%m>%]\r\n"
    void set_log_format(const char *log_format);

    /// \brief Forwards a log message to all registered ILogOutput interfaces
    /// \param[in] message_type Log level \see Log.h
    /// \param[in] file Name of the source file issuing the log.
    /// \param[in] line Line number of the source code issuing the log.
    /// \param[in] function Function that issues the log.
    /// \param[in] message String containing the log message
    /// If no logging output is set, the porting layer will output all logging to stderr.
    void log_message(LogMessageType message_type, const char *file, int line, const char *function, const char *message) const;

    /// \brief Forwards a log message to all registered ILogOutput interfaces
    /// \param[in] message_type Log level \see Log.h
    /// \param[in] message String containing the log message
    /// If no logging output is set, the porting layer will output all logging to stderr.
    void log_message(LogMessageType message_type, const char *message) const
    {
        log_message(message_type, 0, 0, 0, message);
    }

    /// \brief Set base store path for get/set/delete_secure_data and cookie files
    /// \param[in] path The path
    void set_base_store_path(const char *path)
    {
        m_data_store.set_base_store_path(path);
    }

    /// \brief Get access to the DataStore object of this client
    ///
    /// \return Reference to the DataStore object that ClientContext holds.
    DataStore &get_data_store()
    {
        return m_data_store;
    }

    /// \brief Get key map for translating native keys to X11 key codes.
    /// \result The key translation map.
    X11KeyMap &get_keymap();

private:
    // Constructor and destructor are private for as long as ClientContext is a singleton
    ClientContext();
    ~ClientContext();

    std::string m_manufacturer;
    std::string m_devicetype;
    std::string m_unique_id;
    std::string m_ca_path;
    std::string m_ca_client_path;
    std::string m_private_key_path;

    std::string m_log_format;
    std::set<ILogOutput *> m_log_outputs;

    DataStore m_data_store;

    X11KeyMap m_keymap;

    mutable Mutex m_mutex;
};

} // namespace
