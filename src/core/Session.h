///
/// \file Session.h
///
/// \brief CloudTV Nano SDK Northbound interface.
///
///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include "ClientErrorCode.h"

#include <string>

#include <inttypes.h>

namespace ctvc {

class ClientContext;
struct IMediaPlayerFactory;
struct IProtocolExtension;
struct IDefaultProtocolHandler;
struct IHandoffHandler;
struct IOverlayCallbacks;
struct IInput;
struct IControl;
struct IStreamDecrypt;
struct IMediaChunkAllocator;
struct ICdmSessionFactory;
struct IContentLoader;

/// \brief CloudTV Nano SDK session management.
class Session
{
public:
    /// \brief Values returned by get_state().
    enum State
    {
        STATE_DISCONNECTED = 1,     ///< Disconnected.
        STATE_CONNECTING = 2,       ///< Session is being set up.
        STATE_CONNECTED = 4,        ///< Session is running.
        STATE_SUSPENDED = 8,        ///< Suspended.
        STATE_ERROR = 16             ///< Unrecoverable error.
    };

    struct ISessionCallbacks;

    /// \brief Constructs a new session object with references to the client \a context and \a callbacks.
    ///
    /// \pre The client \a context must be initialized, because it is used to query the
    ///      unique client identifier (serial number or MAC address), the STB vendor name
    ///      and the STB model name.
    ///
    /// \param [in] context            Reference to client context.
    /// \param [in] session_callbacks  Pointer to object that implements the ISessionCallbacks interface.
    /// \param [in] overlay_callbacks  Pointer to object that implements the IOverlayCallbacks interface.
    /// \note Any of the pointers being NULL signals that the corresponding functionality is not implemented
    ///       by the client.
    Session(ClientContext &context, ISessionCallbacks *session_callbacks, IOverlayCallbacks *overlay_callbacks);

    virtual ~Session();

    /// \brief Get reference to control component.
    /// \result IControl object
    IControl &get_control() const;

    /// \brief Get reference to input processing component.
    /// \result IInput object
    IInput &get_input() const;

    /// \brief Get current session state.
    /// \result Session::State
    State get_state() const;

    /// \brief Session callback interface.
    /// A client implementation has to implement these callbacks if it wants to be notified
    /// of relevant Session state changes.
    /// \todo Implement the methods of Session::ISessionCallbacks in your own derived class.
    struct ISessionCallbacks
    {
        virtual ~ISessionCallbacks() {}

        /// \brief This is called to notify the recipient of a state change of the session.
        /// This can be, but does not have to be, related to a call to one of the IControl or IInput methods.
        /// \param [in] state The new state of the session. This value would match the state returned by get_state()
        /// until the next call to state_update().
        /// If the session is closed, the session is either in STATE_DISCONNECTED or STATE_ERROR.
        /// \param [in] error_code Error code as documented in 'CloudTV Client Error Code Specification' version 1.4.
        /// This only has a meaning in STATE_DISCONNECTED or STATE_ERROR.
        /// An error code of CLIENT_ERROR_CODE_OK means no error, so this indicates normal session termination.
        /// The client should retune to whatever was running before the session started, e.g. to the last known
        /// TV channel.
        /// An error code of CLIENT_ERROR_CODE_OK_AND_DO_NOT_RETUNE is special: it also indicates 'no error'
        /// but the client should not retune after having closed the session; rather it should stay tuned to
        /// whatever was showing when the session was still active.
        /// \note In STATE_DISCONNECTED or STATE_ERROR, the remote server has indicated that the session has ended
        /// with \a error_code.
        /// Session termination error codes that have to be presented to the user, for example by means of an
        /// on-screen message dialog. The error codes are described in detail in the platform troubleshooting guide.
        virtual void state_update(State state, ClientErrorCode error_code) = 0;
    };


    // ********* Customizable extensions *********

    /// \brief Bind a protocol to a content source for the loading of streams.
    /// \param [in] protocol_id Protocol identifier to bind to \a media_player, e.g. "http".
    /// \param [in] factory IMediaPlayerFactory to handle the download of \a protocol_id.
    /// \result true if successful, false otherwise.
    /// \note Registering again for the same protocol, replaces the previous \a factory
    ///       in the registry of the Session object.
    bool register_media_player(const std::string &protocol_id, IMediaPlayerFactory &factory);

    /// \brief Un-bind a protocol from a content source for the loading of streams.
    /// \param [in] protocol_id Protocol identifier to unbind, e.g. "http".
    /// \result true if successful, false otherwise.
    bool unregister_media_player(const std::string &protocol_id);

    /// \brief Register a content loader of static resources, such as images used for overlays.
    ///        If the client does not register a content loader, then client side images will
    ///        be received as in-band data in the RFB-TV protocol (provided both the cloud application
    ///        and the client support overlays). If the client does register a content loader, then the
    ///        server can decide to provide images by means of download URIs instead of in-band data.
    /// \param [in] content_loader Object that implements IContentLoader interface.
    /// \returns True if the content_loader could be properly registered
    bool register_content_loader(IContentLoader *content_loader);

    /// \brief Register a protocol extension.
    /// \param [in] protocol_extension Reference to instance of a protocol extension.
    ///
    /// Optionally register an instance of a protocol extension class. Do
    /// this to receive messages for the registered protocol in a class that
    /// is derived from IProtocolExtension and instantiated in your client.
    ///
    /// \see IProtocolExtension
    /// \result true if successful, false otherwise.
    /// \note Registering again for the same protocol, replaces the previous
    ///       \a protocol_extension in the registry of the Session object.
    bool register_protocol_extension(IProtocolExtension &protocol_extension);

    /// \brief Unregister a protocol extension.
    /// \param [in] protocol_extension Reference to instance of a protocol extension.
    ///
    /// Call this when the client is no longer interested in messages for
    /// a specific protocol extension.
    ///
    /// \see IProtocolExtension
    /// \result true if successful, false otherwise.
    bool unregister_protocol_extension(IProtocolExtension &protocol_extension);

    /// \brief Register a protocol extension to handle non-registered protocols
    /// \param [in] protocol_handler Pointer to instance of a default protocol handler. Passing a NULL
    /// pointer un-registers the current protocol_handler.
    ///
    /// This method registers a default receiver in case there is no registered IProtocolExtension
    /// object for a received message.
    ///
    /// \see IProtocolExtension
    /// \see IDefaultProtocolHandler
    void register_default_protocol_handler(IDefaultProtocolHandler *protocol_handler);

    /// \brief Register a chunked media memory allocator
    /// \param [in] media_chunk_allocator Pointer to an instance of a media memory allocator engine.
    ///
    /// Passing a NULL pointer or a new allocator un-registers any current allocator, freeing up any
    /// memory allocated using the previously registered allocator, if any.
    /// The chunked media allocator will be used to allocate memory for the deep media buffer.
    ///
    /// \see IMediaChunkAllocator
    void register_media_chunk_allocator(IMediaChunkAllocator *media_chunk_allocator);

    /// \brief Register a DRM system in the form of an ICdmSessionFactory.
    /// \param [in] factory The CdmSession factory to register.
    /// \result true if successful, false otherwise.
    /// \note Registering again for the same DRM system replaces the previous \a factory
    ///       in the registry of the Session object.
    bool register_drm_system(ICdmSessionFactory &factory);

    /// \brief Un-register a DRM system.
    /// \param [in] factory The CdmSession factory to unregister.
    /// \result true if successful, false otherwise.
    /// \note Unregistering any ICdmSessionFactory will close all active CDM sessions, if any.
    bool unregister_drm_system(ICdmSessionFactory &factory);

    /// \brief Register a session handoff handler with the Session object.
    /// \param [in] handoff_scheme Scheme to register \a handoff_handler, e.g. "vod".
    ///        RFB-TV 2.0 defines "vod", "hls", "dash", "mss", "app", "url", 'rfbtv" and 'rfbtvs".
    /// \param [in] handoff_handler IHandoffHandler to handle the handoff of given \a handoff_scheme.
    /// \result true if successful, false otherwise.
    /// \note Registering again for the same handoff_scheme replaces the previous \a handoff_handler
    ///       in the registry of the Session object.
    bool register_handoff_handler(const std::string &handoff_scheme, IHandoffHandler &handoff_handler);

    /// \brief Unregister a session handoff handler.
    /// \param [in] handoff_scheme Scheme to unregister, e.g. "vod".
    /// \result true if successful, false otherwise.
    bool unregister_handoff_handler(const std::string &handoff_scheme);

private:
    Session(const Session &);
    Session &operator=(const Session &);
    class Impl;
    Impl &m_impl;
};

    // Example reference files for Doxygen generated documentation (clients/example)

    /// \example main.cpp
    /// \example Application.cpp
    /// \example Application.h

} // namespace
