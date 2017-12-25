///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#define NO_MIN_MAX

#include <winsock2.h>
#undef HOST_NOT_FOUND
#include <porting_layer/Socket.h>
#include <porting_layer/Thread.h>
#include <porting_layer/Log.h>
#include <porting_layer/ClientContext.h>

#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#ifdef ENABLE_SSL
#include <openssl/rand.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif

#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <stdio.h>

using namespace ctvc;

static int inet_aton(const char *cp, struct in_addr *addr)
{
    addr->s_addr = inet_addr(cp);
    return (addr->s_addr == INADDR_NONE) ? 0 : 1;
}

static bool thread_must_stop()
{
    Thread *thread = Thread::self();
    return thread && thread->must_stop();
}

class WindowsSocketApiInitializer
{
public:
    WindowsSocketApiInitializer()
    {
        WSADATA wsaData;
        WORD wVersionRequested = MAKEWORD(2, 2);
        if (WSAStartup(wVersionRequested, &wsaData) != 0) {
            CTVC_LOG_ERROR("WSAStartup() failed (%d)", WSAGetLastError());
        } else {
            CTVC_LOG_DEBUG("WSAStartup() successful");
        }
    }
};

static WindowsSocketApiInitializer s_socket_api_initializer;

const ResultCode Socket::SOCKET_NOT_OPEN("Trying to access a socket that is not open");
const ResultCode Socket::READ_ERROR("Cannot receive message from the socket");
const ResultCode Socket::WRITE_ERROR("Cannot send message to the socket");
const ResultCode Socket::BIND_ERROR("Cannot bind the socket");
const ResultCode Socket::HOST_NOT_FOUND("The given host is not found by the DNS");
const ResultCode Socket::CONNECTION_REFUSED("TCP connection failed to open due to the connection being refused");
const ResultCode Socket::CONNECT_FAILED("TCP connection failed to open");
const ResultCode Socket::CONNECT_TIMEOUT("TCP connection failed to open because remote server did not respond in time");
const ResultCode Socket::LISTEN_FAILED("Listen failed on the TCP socket");
const ResultCode Socket::SOCKET_OPTION_ACCESS_FAILED("Failed to get or set a socket option");
const ResultCode Socket::THREAD_SHUTDOWN("A blocking call was interrupted because the calling thread is shut down");

static const int SOCKET_CONNECT_TIMEOUT_TIME_SECONDS = 10;
static const int SOCKET_SELECT_TIMEOUT_TIME_MICROSECONDS = 5000;

class SocketImpl : public Socket::ISocket
{
public:
    SocketImpl();
    virtual ~SocketImpl();

    virtual void open();
    virtual void close();

    virtual ResultCode connect(const char *host, int port);
    virtual ResultCode bind(const char *host, int port);

    virtual ResultCode send(const uint8_t *data, uint32_t length);
    virtual ResultCode receive(uint8_t *data, uint32_t size, uint32_t &length/*out*/);

    virtual ResultCode set_receive_buffer_size(uint32_t size);
    virtual ResultCode set_reuse_address(bool on);

protected:
    SOCKET m_socket;
    struct sockaddr_in m_local_address;
    struct sockaddr_in m_remote_address;

    virtual ResultCode set_non_blocking(bool on);
    virtual ResultCode set_address(const char *host, int port, struct sockaddr_in &);
    virtual int createSocket() = 0;
    virtual ResultCode do_connect() = 0;
    virtual ssize_t do_send(const uint8_t *data, uint32_t length) = 0;
    virtual ResultCode do_receive(uint8_t *data, uint32_t length, ssize_t &received_data_length) = 0;
    int timeout_select(bool test_for_write);
};

class UdpSocketImpl : public SocketImpl
{
public:
    UdpSocketImpl();

protected:
    virtual int createSocket();
    virtual ResultCode do_connect();
    virtual ssize_t do_send(const uint8_t *data, uint32_t length);
    virtual ResultCode do_receive(uint8_t *data, uint32_t length, ssize_t &received_data_length);
};

class TcpSocketImpl : public SocketImpl
{
public:
    TcpSocketImpl();

    virtual ResultCode listen(uint32_t backlog);
    virtual TcpSocket *accept();

    virtual ResultCode set_no_delay(bool on);

protected:
    virtual int createSocket();
    virtual ResultCode do_connect();
    virtual ssize_t do_send(const uint8_t *data, uint32_t length);
    virtual ResultCode do_receive(uint8_t *data, uint32_t length, ssize_t &received_data_length);
};

class SslSocketImpl : public TcpSocketImpl
{
public:
    SslSocketImpl();
    virtual void close();

protected:
    virtual ResultCode do_connect();
    virtual ssize_t do_send(const uint8_t *data, uint32_t length);
    virtual ResultCode do_receive(uint8_t *data, uint32_t length, ssize_t &received_data_length);

private:
#ifdef ENABLE_SSL
    SSL *m_tls_handle;
    SSL_CTX *m_tls_ctx;
#endif
};

UdpSocket::UdpSocket() :
    Socket(*new UdpSocketImpl)
{
}

TcpSocket::TcpSocket() :
    Socket(*new TcpSocketImpl)
{
}

TcpSocket::TcpSocket(ISocket &impl) :
    Socket(impl)
{
}

ResultCode TcpSocket::listen(uint32_t backlog)
{
    return static_cast<TcpSocketImpl &>(m_impl).listen(backlog);
}

TcpSocket *TcpSocket::accept()
{
    return static_cast<TcpSocketImpl &>(m_impl).accept();
}

ResultCode TcpSocket::set_no_delay(bool on)
{
    return static_cast<TcpSocketImpl &>(m_impl).set_no_delay(on);
}

SslSocket::SslSocket() :
    TcpSocket(*new SslSocketImpl)
{
}

SocketImpl::SocketImpl() :
    m_socket(INVALID_SOCKET)
{
    memset(&m_local_address, 0, sizeof(m_local_address));
    memset(&m_remote_address, 0, sizeof(m_remote_address));
}

SocketImpl::~SocketImpl()
{
    close();
}

void SocketImpl::open()
{
    close();
    m_socket = createSocket();
    if (m_socket == INVALID_SOCKET) {
        CTVC_LOG_ERROR("Failed to create socket");
    }
}

void SocketImpl::close()
{
    if (m_socket != INVALID_SOCKET) {
        ::closesocket(m_socket);
        m_socket = INVALID_SOCKET;
    }
}

int SocketImpl::timeout_select(bool test_for_write)
{
    fd_set socket_set;
    struct timeval tv;

    FD_ZERO(&socket_set);
    FD_SET(m_socket, &socket_set);

    tv.tv_sec = 0;
    tv.tv_usec = SOCKET_SELECT_TIMEOUT_TIME_MICROSECONDS;

    if (test_for_write) {
        return select(m_socket + 1, NULL, &socket_set, NULL, &tv);
    }
    return select(m_socket + 1, &socket_set, NULL, NULL, &tv);
}

ResultCode SocketImpl::set_address(const char *host, int port, struct sockaddr_in &address)
{
    CTVC_LOG_DEBUG("'%s:%d'", host, port);

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    if (host == 0) {
        address.sin_addr.s_addr = INADDR_ANY;
    } else if (inet_aton(host, &address.sin_addr) == 0) {
        struct hostent *remoteHost = gethostbyname(host);
        if (remoteHost == NULL) {
            CTVC_LOG_DEBUG("gethostbyname() failed");
            return Socket::HOST_NOT_FOUND;
        }
        memcpy(&address.sin_addr, remoteHost->h_addr, remoteHost->h_length);

        const uint8_t *ip = reinterpret_cast<const uint8_t *>(remoteHost->h_addr);
        CTVC_LOG_INFO("ip:%d.%d.%d.%d, port:%d", ip[0], ip[1], ip[2], ip[3], port);
    }

    return ResultCode::SUCCESS;
}

ResultCode SocketImpl::connect(const char *host, int port)
{
    if (m_socket == INVALID_SOCKET) {
        open(); // Try to open the socket

        if (m_socket == INVALID_SOCKET) {
            CTVC_LOG_WARNING("Socket could not be opened");
            return Socket::SOCKET_NOT_OPEN;
        }
    }

    ResultCode ret = set_address(host, port, m_remote_address);
    if (ret.is_error()) {
        return ret;
    }

    ret = do_connect();
    if (ret.is_error()) {
        CTVC_LOG_ERROR("Connect failed");
        return ret;
    }

    return ResultCode::SUCCESS;
}

ResultCode SocketImpl::bind(const char *host, int port)
{
    if (m_socket == INVALID_SOCKET) {
        CTVC_LOG_WARNING("Socket not open");
        return Socket::SOCKET_NOT_OPEN;
    }

    ResultCode ret = set_address(host, port, m_local_address);
    if (ret.is_error()) {
        return ret;
    }

    if (::bind(m_socket, (struct sockaddr *)&m_local_address, sizeof(m_local_address)) != 0) {
        CTVC_LOG_ERROR("bind() failed, errno:%d", WSAGetLastError());
        return Socket::BIND_ERROR;
    }
    return ResultCode::SUCCESS;
}

ResultCode SocketImpl::send(const uint8_t *data, uint32_t length)
{
    if (m_socket == INVALID_SOCKET) {
        CTVC_LOG_WARNING("Socket not open");
        return Socket::SOCKET_NOT_OPEN;
    }

    while (length > 0) {
        int bytes_sent = do_send(data, length);
        if (bytes_sent == SOCKET_ERROR) {
            CTVC_LOG_ERROR("Send errno:%d", WSAGetLastError());
            return Socket::WRITE_ERROR;
        }

        length -= bytes_sent;
    }

    return ResultCode::SUCCESS;
}

ResultCode SocketImpl::receive(uint8_t *data, uint32_t size, uint32_t &length/*out*/)
{
    length = 0;

    if (m_socket == INVALID_SOCKET) {
        CTVC_LOG_WARNING("Socket not open");
        return Socket::SOCKET_NOT_OPEN;
    }

    ssize_t received_data_length = 0;
    ResultCode result = do_receive(data, size, received_data_length);
    length = received_data_length;

    return result;
}

ResultCode SocketImpl::set_receive_buffer_size(uint32_t size)
{
    if (m_socket == INVALID_SOCKET) {
        CTVC_LOG_WARNING("Socket not open");
        return Socket::SOCKET_NOT_OPEN;
    }

    return setsockopt(m_socket, SOL_SOCKET, SO_RCVBUF, (const char *)&size, sizeof(size)) == 0 ? ResultCode::SUCCESS : Socket::SOCKET_OPTION_ACCESS_FAILED;
}

ResultCode SocketImpl::set_reuse_address(bool on)
{
    if (m_socket == INVALID_SOCKET) {
        CTVC_LOG_WARNING("Socket not open");
        return Socket::SOCKET_NOT_OPEN;
    }

    int flag = on;
    return setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&flag, sizeof(int)) == 0 ? ResultCode::SUCCESS : Socket::SOCKET_OPTION_ACCESS_FAILED;
}

ResultCode SocketImpl::set_non_blocking(bool on)
{
    if (m_socket == INVALID_SOCKET) {
        CTVC_LOG_WARNING("Socket not open");
        return Socket::SOCKET_NOT_OPEN;
    }
    int iResult = 0;
    u_long iMode = 0;
    if (on) {
        iMode = 1;
    }

    iResult = ioctlsocket(m_socket, FIONBIO, &iMode);
    if (iResult != NO_ERROR) {
        //logPorting (CTVCP_LOG_ERROR, "ioctlsocket FIONBIO, 1 failed WSAGetLastError ():%d", WSAGetLastError ());
        return Socket::SOCKET_OPTION_ACCESS_FAILED;
    }
    return ResultCode::SUCCESS;
}

/* static */
ResultCode Socket::get_local_address(std::string &/*local_address*/)
{
    CTVC_LOG_WARNING("TODO: implement 'Socket::get_local_address()'");
    return SOCKET_OPTION_ACCESS_FAILED;
}

UdpSocketImpl::UdpSocketImpl()
{
    open(); // Creates initial socket
}

int UdpSocketImpl::createSocket()
{
    return ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
}

ResultCode UdpSocketImpl::do_connect()
{
    return ResultCode::SUCCESS;
}

ssize_t UdpSocketImpl::do_send(const uint8_t *data, uint32_t length)
{
    return ::sendto(m_socket, (char *)data, length, 0, (struct sockaddr *)&m_remote_address, sizeof(m_remote_address));
}

ResultCode UdpSocketImpl::do_receive(uint8_t *data, uint32_t length, ssize_t &received_data_length)
{
    while (1) {
        int result = -1;
        result = timeout_select(false);

        if (result == SOCKET_ERROR) {
            return Socket::READ_ERROR;
        } else if (result == 0) {
            if (thread_must_stop()) {
                return Socket::READ_ERROR;
            }
            continue;
        }

        received_data_length = ::recvfrom(m_socket, (char *)data, length, 0, 0, 0);

        if (received_data_length == SOCKET_ERROR) {
            return Socket::READ_ERROR;
        } else {
            if (thread_must_stop()) {
                return Socket::READ_ERROR;
            } else {
                return ResultCode::SUCCESS;
            }
        }
    }
}

TcpSocketImpl::TcpSocketImpl()
{
    open(); // Creates initial socket
}

int TcpSocketImpl::createSocket()
{
    return ::socket(AF_INET, SOCK_STREAM, 0);
}

ResultCode TcpSocketImpl::listen(uint32_t backlog)
{
    if (m_socket == INVALID_SOCKET) {
        CTVC_LOG_WARNING("Socket not open");
        return Socket::SOCKET_NOT_OPEN;
    }

    if (::listen(m_socket, backlog) < 0) {
        CTVC_LOG_ERROR("listen() failed:%d", WSAGetLastError());
        return Socket::LISTEN_FAILED;
    }

    return ResultCode::SUCCESS;
}

TcpSocket *TcpSocketImpl::accept()
{
    if (m_socket == INVALID_SOCKET) {
        CTVC_LOG_WARNING("Socket not open");
        return 0;
    }

    struct sockaddr_in remote_address;
    socklen_t sockaddr_len = sizeof(remote_address);
    bool checking = true;

    while (checking) {
        int result = -1;
        result = timeout_select(false);

        if (result == SOCKET_ERROR) {
            return 0;
        } else if (result == 0) {
            if (thread_must_stop()) {
                return 0;
            }
            continue;
        } else {
            checking = false;
        }
    }

    int new_socket = ::accept(m_socket, (struct sockaddr *)&remote_address, &sockaddr_len);
    if (new_socket <= 0) {
        CTVC_LOG_WARNING("accept() failed");
        return 0;
    }

    // Create new socket and hack the accepted socket into it
    TcpSocket *tcp_socket = new TcpSocket;
    TcpSocketImpl &impl(static_cast<TcpSocketImpl &>(tcp_socket->get_impl()));
    impl.close();
    impl.m_socket = new_socket;
    memcpy(&impl.m_remote_address, &remote_address, sizeof(impl.m_remote_address));

    return tcp_socket;
}

ResultCode TcpSocketImpl::do_connect()
{
    ResultCode ret = Socket::CONNECT_FAILED;

    // Set socket non-blocking because we don't want the 'connect()' call to block
    if (set_non_blocking(true).is_error()) {
        CTVC_LOG_ERROR("Failed to set socket non-blocking");
        return Socket::CONNECT_FAILED;
    }

    int retries = SOCKET_CONNECT_TIMEOUT_TIME_SECONDS * 1000000 / SOCKET_SELECT_TIMEOUT_TIME_MICROSECONDS;

    // Try to connect in non-blocking mode, using select() and getsockopt() to poll the connect status
    int connect_result = ::connect(m_socket, (struct sockaddr*)&m_remote_address, sizeof(m_remote_address));
    int socket_error = WSAGetLastError();
    // Note that WinCE returns last_error 10035 (WSAEWOULDBLOCK), but 'real' Windows' may return one of the others (as documented)
    if ((connect_result == SOCKET_ERROR) && ((socket_error == WSAEINPROGRESS) || (socket_error == WSAEALREADY) || (socket_error == WSAEWOULDBLOCK))) {
        while (true) {
            int select_result = timeout_select(true);
            if (select_result < 0) {
                CTVC_LOG_ERROR("The select() call failed with error:%d", WSAGetLastError());
                break;
            }
            if (select_result == 0) {
                if (thread_must_stop()) {
                    CTVC_LOG_INFO("Thread shutdown");
                    break;
                }
                if (retries > 0) {
                    retries--;
                    continue;
                }
                CTVC_LOG_INFO("Timeout while trying to connect to remote server");
                ret = Socket::CONNECT_TIMEOUT;
                break;
            }
            // Check the status of the socket
            socklen_t opt_length = sizeof(socket_error);
            if (getsockopt(m_socket, SOL_SOCKET, SO_ERROR, (char*)&socket_error, &opt_length) == SOCKET_ERROR) {
                CTVC_LOG_WARNING("Failed to retrieve socket error status");
                break;
            }
            if (socket_error != 0) {
                CTVC_LOG_WARNING("Connect failed with socket error %d", socket_error);
                break;
            }
            CTVC_LOG_INFO("Connection established");
            ret = ResultCode::SUCCESS;
            break;
        }
    } else if (connect_result == 0) {
        CTVC_LOG_INFO("Connection established");
        ret = ResultCode::SUCCESS;
    } else {
        CTVC_LOG_ERROR("The connect() call failed with error:%d", socket_error);
    }

    if (ret.is_error() && (socket_error == WSAECONNREFUSED)) {
        ret = Socket::CONNECTION_REFUSED;
    }

    if (set_non_blocking(false).is_error()) {
        CTVC_LOG_ERROR("Failed to set socket blocking");
        if (ret.is_ok()) {
            ret = Socket::CONNECT_FAILED;
        }
    }

    return ret;
}

ssize_t TcpSocketImpl::do_send(const uint8_t *data, uint32_t length)
{
    if (m_socket == INVALID_SOCKET) {
        CTVC_LOG_WARNING("Socket not open");
        return 0;
    }

    return ::send(m_socket, (char *)data, length, 0);
}

ResultCode TcpSocketImpl::do_receive(uint8_t *data, uint32_t length, ssize_t &received_data_length)
{
    while (1) {
        int result = -1;
        result = timeout_select(false);
        if (result == SOCKET_ERROR) {
            return Socket::READ_ERROR;
        } else if (result == 0) {
            if (thread_must_stop()) {
                return Socket::READ_ERROR;
            }
            continue;
        }

        received_data_length = ::recv(m_socket, (char *)data, length, 0);
        if (received_data_length == SOCKET_ERROR) {
            return Socket::READ_ERROR;
        } else if (received_data_length == 0) {
            CTVC_LOG_INFO("Peer closed connection");
            return ResultCode::SUCCESS;
        } else {
            if (thread_must_stop()) {
                CTVC_LOG_INFO("Thread shutdown");
                return Socket::THREAD_SHUTDOWN;
            } else {
                return ResultCode::SUCCESS;
            }
        }
    }
}

ResultCode TcpSocketImpl::set_no_delay(bool on)
{
    if (m_socket == INVALID_SOCKET) {
        CTVC_LOG_WARNING("Socket not open");
        return Socket::SOCKET_NOT_OPEN;
    }

    int flag = on;
    return setsockopt(m_socket, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int)) == 0 ? ResultCode::SUCCESS : Socket::SOCKET_OPTION_ACCESS_FAILED;
}

SslSocketImpl::SslSocketImpl()
{
#ifdef ENABLE_SSL
    m_tls_handle = 0;
    m_tls_ctx = 0;
#endif
}

void SslSocketImpl::close()
{
#ifdef ENABLE_SSL
    if (m_tls_handle) {
        SSL_shutdown(m_tls_handle);
        SSL_free(m_tls_handle);
        m_tls_handle = 0;
    }
    if (m_tls_ctx) {
        SSL_CTX_free(m_tls_ctx);
        m_tls_ctx = 0;
    }
#endif

    SocketImpl::close();
}

ResultCode SslSocketImpl::do_connect()
{
#ifdef ENABLE_SSL
    SSL_load_error_strings();
    SSL_library_init();

    m_tls_ctx = SSL_CTX_new(SSLv23_client_method());
    if (!m_tls_ctx) {
        CTVC_LOG_ERROR("Failed to create SSL context");
        return Socket::CONNECTION_REFUSED;
    }

    int ret = 0;

    const char *ca_client_path = ClientContext::instance().get_ca_client_path();
    const char *private_key_path = ClientContext::instance().get_private_key_path();

    if (ca_client_path[0] != '\0' && private_key_path[0] != '\0') {
        ret = SSL_CTX_use_certificate_file(m_tls_ctx, ca_client_path, SSL_FILETYPE_PEM);
        if (ret != 1) {
            CTVC_LOG_ERROR("Failed SSL_CTX_use_certificate_file(%s)", ca_client_path);
            return Socket::CONNECTION_REFUSED;
        }

        ret = SSL_CTX_use_PrivateKey_file(m_tls_ctx, private_key_path, SSL_FILETYPE_PEM);
        if (ret != 1) {
            CTVC_LOG_ERROR("Failed SSL_CTX_use_PrivateKey_file(%s)", private_key_path);
            return Socket::CONNECTION_REFUSED;
        }
    }

    const char *ca_path = ClientContext::instance().get_ca_path();
    ret = SSL_CTX_load_verify_locations(m_tls_ctx, ca_path, NULL);
    if (ret != 1) {
        CTVC_LOG_ERROR("Failed SSL_CTX_load_verify_locations(%s)", ca_path);
        return Socket::CONNECTION_REFUSED;
    }

    m_tls_handle = SSL_new(m_tls_ctx);
    if (m_tls_handle == NULL) {
        CTVC_LOG_ERROR("Failed SSL_new()");
        return Socket::CONNECTION_REFUSED;
    }

    ret = SSL_set_fd(m_tls_handle, m_socket);
    if (ret != 1) {
        CTVC_LOG_ERROR("Failed SSL_set_fd: ret:%d, SSL_get_error:%d", ret, SSL_get_error(m_tls_handle, ret));
        return Socket::CONNECTION_REFUSED;
    }

    //SSL_set_verify(m_tls_handle, SSL_VERIFY_NONE, NULL);

    ResultCode result = TcpSocketImpl::do_connect();
    if (result.is_error()) {
        return result;
    }

    ret = SSL_connect(m_tls_handle);
    if (ret != 1) {
        CTVC_LOG_ERROR("Failed SSL_connect() ret:%d, SSL_get_error:%d ", ret, SSL_get_error(m_tls_handle, ret));
        ret = SSL_get_verify_result(m_tls_handle);
        if (ret != X509_V_OK) {
            if (ret == X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT || ret == X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN) {
                CTVC_LOG_DEBUG("Self signed certificate");
            } else {
                CTVC_LOG_ERROR("Certificate verification error: %ld", SSL_get_verify_result(m_tls_handle));
                return Socket::CONNECTION_REFUSED;
            }
        }

        return Socket::CONNECTION_REFUSED;
    }

    return ResultCode::SUCCESS;
#else
    return Socket::CONNECTION_REFUSED;
#endif
}

ssize_t SslSocketImpl::do_send(const uint8_t *data, uint32_t length)
{
#ifdef ENABLE_SSL
    return SSL_write(m_tls_handle, data, length);
#else
    (void)data;
    (void)length;
    return -1;
#endif
}

ResultCode SslSocketImpl::do_receive(uint8_t *data, uint32_t length, ssize_t &received_data_length)
{
#ifdef ENABLE_SSL
    while(1) {
        int result = -1;
        result = timeout_select(false);

        if (result == SOCKET_ERROR) {
            return Socket::READ_ERROR;
        } else if (result == 0) {
            if (thread_must_stop()) {
                return Socket::READ_ERROR;
            }
            continue;
        }

        received_data_length = SSL_read(m_tls_handle, data, length);

        if (received_data_length <= 0) {
            return Socket::READ_ERROR;
        } else {
            if (thread_must_stop()) {
                return Socket::READ_ERROR;
            } else {
                return ResultCode::SUCCESS;
            }
        }
    }
#else
    (void)data;
    (void)length;
    (void)received_data_length;
    return Socket::SOCKET_NOT_OPEN;
#endif
}
