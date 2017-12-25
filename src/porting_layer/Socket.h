///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include "ResultCode.h"

#include <inttypes.h>
#include <string>

namespace ctvc {

/// \brief Generic socket interface.
///
/// The implementation is done as part of the porting layer.  All implementation is forwarded to the
/// private Impl class.  Coding this inline should speed-up code and reduce code size.
///
/// This class should not be used directly, but one of the derived classes instead \see UdpSocket,
/// TcpSocket, SslSocket
class Socket
{
public:
    static const ResultCode SOCKET_NOT_OPEN;             ///< The socket is not open
    static const ResultCode READ_ERROR;                  ///< Error reading from the socket
    static const ResultCode WRITE_ERROR;                 ///< Error writing to the socket
    static const ResultCode BIND_ERROR;                  ///< Error when binding the socket to a port
    static const ResultCode HOST_NOT_FOUND;              ///< Host not found error
    static const ResultCode CONNECTION_REFUSED;          ///< The connection could not be established because it was refused by the server
    static const ResultCode CONNECT_FAILED;              ///< The connection could not be established
    static const ResultCode CONNECT_TIMEOUT;             ///< Timeout while trying to establish a connection
    static const ResultCode LISTEN_FAILED;               ///< The port could not be listened
    static const ResultCode SOCKET_OPTION_ACCESS_FAILED; ///< Error in the given options
    static const ResultCode THREAD_SHUTDOWN;             ///< A blocking call was interrupted because the calling thread is shut down

    /// \brief Interface for the implementation of socket functionality
    ///
    /// Socket uses an object that implements this interface to expose socket functionality.
    /// There is a one-to-one mapping between ISocket methods and Socket's, i.e. they have
    /// the same syntax and semantics \see Socket
    struct ISocket
    {
        /// \{
        ISocket() {}
        virtual ~ISocket() {}

        // Create a new socket if necessary (only needed if the socket is reused after a close())
        virtual void open() = 0;
        // Close the socket
        virtual void close() = 0;

        // Connect to remote host/port for TCP, set remote address/port for UDP
        virtual ResultCode connect(const char *host, int port) = 0;
        // Bind to local address/port for both TCP and UDP
        virtual ResultCode bind(const char *host, int port) = 0;

        virtual ResultCode send(const uint8_t *data, uint32_t length) = 0;
        virtual ResultCode receive(uint8_t *data, uint32_t size, uint32_t &length/*out*/) = 0;

        virtual ResultCode set_receive_buffer_size(uint32_t size) = 0;
        virtual ResultCode set_reuse_address(bool on) = 0;
        virtual ResultCode set_non_blocking(bool on) = 0;
        /// \}
    };

    /// \brief Constructor of Socket
    /// \param[in] impl Object that implements ISocket interface to implement the required
    /// functionality
    Socket(ISocket &impl) :
        m_impl(impl)
    {
    }

    virtual ~Socket()
    {
        delete &m_impl;
    }

    /// \brief Open the socket
    void open()
    {
        m_impl.open();
    }

    /// \brief Close the socket
    void close()
    {
        m_impl.close();
    }

    /// \brief Establish a connection with a host in the specified port
    /// \param[in] host Destination address in either dotted notation or FQDN
    /// \param[in] port Destination port
    /// \retval ResultCode::SUCCESS When the connection has been established
    /// \retval SOCKET_NOT_OPEN When the socket has not been previously opened
    /// \retval HOST_NOT_FOUND When the hostname cannot be resolved or be reached
    /// \retval CONNECTION_REFUSED When the server has refused the connection request
    ResultCode connect(const char *host, int port)
    {
        return m_impl.connect(host, port);
    }

    /// \brief Bind a port to a local address and in a specific port
    ///
    /// After successfully binding the port, data can be received using the method receive()
    /// \param[in] host Local address in either dotted notation or FQDN
    /// \param[in] port Local port
    /// \retval ResultCode::SUCCESS If socket has been successfully bound.
    /// \retval SOCKET_NOT_OPEN When the socket has not been previously opened
    /// \retval HOST_NOT_FOUND When the hostname cannot be resolved or be reached
    /// \retval BIND_ERROR When the given host/port could not be bound
    ResultCode bind(const char *host, int port)
    {
        return m_impl.bind(host, port);
    }

    /// \brief Send data to the host
    ///
    /// This method only can be used after a successful connection has been established \see
    /// connect().
    /// \param[in] data Array of bytes to be sent
    /// \param[in] length Number of bytes to be sent
    /// \retval ResultCode::SUCCESS If the entire buffer has been successfully sent
    /// \retval SOCKET_NOT_OPEN When the socket has not been previously opened
    /// \retval WRITE_ERROR When there was an error with the writing, e.g. the socket was not
    /// opened or the connection is closed.
    ResultCode send(const uint8_t *data, uint32_t length)
    {
        return m_impl.send(data, length);
    }

    /// \brief Receive data from the socket
    ///
    /// This method only can be used after a successful binding.
    /// \param[in] data Buffer where the received data will be stored
    /// \param[in] size Size of the buffer "data"
    /// \param[out] length Number of bytes received
    /// \retval ResultCode::SUCCESS If the operation succeeded. This does not imply that actual data
    /// was received. If length == 0, this indicates that the connection was closed by the peer.
    /// \retval SOCKET_NOT_OPEN When the socket has not been previously opened.
    /// \retval READ_ERROR When there was an error with the reading, e.g. the socket was not opened.
    /// \retval THREAD_SHUTDOWN When the call was interrupted because the calling thread is shut down.
    ResultCode receive(uint8_t *data, uint32_t size, uint32_t &length/*out*/)
    {
        return m_impl.receive(data, size, length);
    }

    /// \brief Set the size of the socket buffer of the platform
    /// \param[in] size Size of the socket buffer
    /// \return ResultCode::SUCCESS If the new size has been successfully set.
    /// \retval SOCKET_NOT_OPEN When the socket has not been previously opened.
    /// \retval SOCKET_OPTION_ACCESS_FAILED If the operation failed.
    ResultCode set_receive_buffer_size(uint32_t size)
    {
        return m_impl.set_receive_buffer_size(size);
    }

    /// \brief The local address can be re-used for binding operations.
    /// \param[in] on Whether the local address can be reused.
    /// \retval ResultCode::SUCCESS If the operation succeeded.
    /// \retval SOCKET_NOT_OPEN When the socket has not been previously opened.
    /// \retval SOCKET_OPTION_ACCESS_FAILED If the operation failed.
    ResultCode set_reuse_address(bool on)
    {
        return m_impl.set_reuse_address(on);
    }

    /// \brief Get the address that is bound to the network adapter (usually DHCP assigned).
    /// \param[out] local_address The local address in dotted IP notatation (e.g. 172.178.16.128).
    /// \retval ResultCode::SUCCESS If the operation succeeded.
    /// \retval SOCKET_NOT_OPEN When the temporary socket failed to open.
    /// \retval SOCKET_OPTION_ACCESS_FAILED If the operation failed.
    static ResultCode get_local_address(std::string &local_address);

    /// \{
    ISocket &get_impl()
    {
        return m_impl;
    }
    /// \}

protected:
    /// \{
    ISocket &m_impl;
    /// \}

private:
    Socket(const Socket &);
    Socket &operator=(const Socket &);
};

/// \brief UDP socket interface
class UdpSocket : public Socket
{
public:
    UdpSocket();
};

/// \brief TCP socket interface
class TcpSocket : public Socket
{
public:
    TcpSocket();

    /// \brief Listen into an specific port for incoming connection request
    /// \param[in] backlog Maximum queue length. When the backlog is exhausted, new connection will
    /// be rejected.
    /// \retval ResultCode::SUCCESS If the operation succeeded.
    /// \retval SOCKET_NOT_OPEN When the socket has not been previously opened.
    /// \retval LISTEN_FAILED If the operation failed.
    virtual ResultCode listen(uint32_t backlog);

    /// \brief Accept a new connection
    ///
    /// This method can only be used after listen() succeeded.
    /// \return A pointer to an TcpSocket object to send or receive data or 0 if accept() failed or
    ///         was interrupted because the calling thread is shut down.
    virtual TcpSocket *accept();

    /// \brief If set, the data will be sent as soon as possible
    ///
    /// When the amount of data is small, the platform will try to wait for more data before sending
    /// it. If this flag is set, the data is sent as soon as possible.
    /// \param[in] on True to send the data as soon as possible.
    /// \retval ResultCode::SUCCESS If the operation succeeded.
    /// \retval SOCKET_NOT_OPEN When the socket has not been previously opened.
    /// \retval SOCKET_OPTION_ACCESS_FAILED If the operation failed.
    virtual ResultCode set_no_delay(bool on);

protected:
    /// \{
    TcpSocket(ISocket &); // For SslSocket
    /// \}
};

/// \brief SSL socket interface
class SslSocket : public TcpSocket
{
public:
    SslSocket();
};

} // namespace
