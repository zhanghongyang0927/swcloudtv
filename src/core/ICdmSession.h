///
/// \file ICdmSession.h
///
/// \brief CloudTV Nano SDK DRM interface.
///
///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <string>
#include <map>

#include <inttypes.h>

namespace ctvc {

struct IStreamDecrypt;

/// \brief CDM session interface.
///
/// A client only needs to implement this interface when CDM/DRM support is required. Object instances will be created by means
/// of a call to ICdmSessionFactory::create().
///
/// \note It is up to the implementation to handle a call to \a setup() and \a terminate() asynchronous (i.e. non-blocking).
/// However, it is strongly recommended to do so because otherwise a non-responsive or slow CDM/DRM server, or poor network
/// conditions, will also block the handling of other RFB\-TV protocol messages (like key presses).
struct ICdmSession
{
    ICdmSession()
    {
    }

    virtual ~ICdmSession()
    {
    }

    /// \brief Get a related stream decryption engine.
    /// \return Pointer to an instance of a decryption engine.
    /// Returning a NULL pointer indicates no decryption engine is available.
    /// This method allows passing a decryption engine that can be used to decrypt encrypted streams
    /// that are related to this CdmSession instance.
    /// The set-up and control of the stream decryption engine, as well as the decryption algorithm
    /// used is to be defined by the user. Returning a valid pointer makes sure that any encrypted
    /// stream is routed through the registered object for decryption.
    /// If a valid pointer is returned, it should remain valid until terminate() is called or until
    /// the CdmSession object is destroyed.
    ///
    /// \see IStreamDecrypt
    virtual IStreamDecrypt *get_stream_decrypt_engine() = 0;

    /// \brief Result values of \a setup().
    enum SetupResult
    {
        SETUP_OK, SETUP_DRM_SYSTEM_ERROR, SETUP_NO_LICENSE_SERVER, SETUP_LICENSE_NOT_FOUND, SETUP_UNSPECIFIED_ERROR
    };

    /// \brief Callback interface to indicate asynchronous events from the CdmSession object.
    ///
    /// \see ICdmSession
    struct ICallback
    {
        virtual ~ICallback()
        {
        }

        /// \brief Values for \a reason parameter in terminate_indication().
        enum TerminateReason
        {
            TERMINATE_USER_STOP, TERMINATE_END_OF_STREAM, TERMINATE_LICENSE_EXPIRED, TERMINATE_UNSPECIFIED
        };

        /// \brief Indicate termination of a CdmSession.
        ///
        /// This can be called by the CdmSession object at any time between a successful setup() and terminate().
        /// The use of this callback is optional. It is intended to be used in case a running session suddenly
        /// gets into a (fatal) error state and the server needs to be signaled.
        /// The SDK will call terminate() in response and destroy the session afterward.
        /// \param [in] reason The reason why the termination is requested.
        /// \note The SDK may already have deleted the calling CdmSession object when terminate_indication()
        /// returns. The calling code must be aware of that and take appropriate precautions.
        virtual void terminate_indication(TerminateReason reason) = 0;

        /// \brief Report the result of the setup() call.
        ///
        /// Call this to pass the result of the \a setup() call back to the SDK when the setup is complete.
        /// \param [in] result SETUP_OK if successful or one of the other ICdmSession::SetupResult values otherwise.
        /// \param [in] response The DRM-specific response data, packed as key-value pairs, as result of to the call to \a setup().
        virtual void setup_result(ICdmSession::SetupResult result, const std::map<std::string, std::string> &response) = 0;

        /// \brief Report the result of the terminate() call.
        ///
        /// Call this to pass the result of the \a terminate() call back to the SDK when the terminate is complete.
        /// \param [in] stop_data The DRM-specific stop data, packed as key-value pairs, as a result of to the call to \a terminate().
        virtual void terminate_result(const std::map<std::string, std::string> &stop_data) = 0;
    };

    /// \brief Setup a new CdmSession.
    ///
    /// This is called exactly once for each CdmSession object, typically right after construction.
    ///
    /// \note It is highly recommended to process this call asynchronously (i.e. non-blocking) and post the resulting stop data
    /// by calling \a setup_result() once the session setup is complete.
    ///
    /// \param [in] session_type The session type. Currently, can only be "temporary".
    /// \param [in] init_data The DRM-specific session initialization data, packed as key-value pairs.
    /// \param [in] callback A callback object that the CdmSession must use to tell the result of the setup (or to indicate premature session termination).
    virtual void setup(const std::string &session_type, const std::map<std::string, std::string> &init_data, ICallback &callback) = 0;

    /// \brief Terminate a CdmSession.
    ///
    /// This is called exactly once for each CdmSession object, typically before destruction.
    /// It must be possible, however, to delete a CdmSession object without terminate() having been called first.
    ///
    /// \note It is highly recommended to process this call asynchronously (i.e. non-blocking) and post the resulting stop data
    /// by calling \a terminate_result() once the session termination is complete.
    ///
    /// \param [in] callback A callback object that the CdmSession must use to tell the result of the terminate.
    virtual void terminate(ICallback &callback) = 0;
};

/// \brief CDM session object factory.
///
/// The ICdmSessionFactory is registered and bound to a specific DRM type. This allows the owner
/// to create CdmSession instances for the proper DRM system when required.
/// A client must register factories by calling Session::register_drm_system().
/// \see ICdmSession
///
struct ICdmSessionFactory
{
    ICdmSessionFactory()
    {
    }
    virtual ~ICdmSessionFactory()
    {
    }

    /// \brief Return the DRM system ID of this CdmSessionFactory.
    /// \param [out] id The DRM system ID.
    virtual void get_drm_system_id(uint8_t (&id)[16]/*out*/) = 0;

    /// \brief Create a new instance of a CdmSession object.
    ///
    /// \return Pointer to a CdmSession object that implements ICdmSession.
    /// \note Deletion of the returned object will be done by calling destroy().
    virtual ICdmSession *create() = 0;

    /// \brief Destroy a previously created instance of a CdmSession object.
    ///
    /// Free all related resources of the object that is pointed to by \a cdm_session, including any threads that may have been
    /// created to support asynchronous handling.
    /// \param [in] cdm_session Pointer to CdmSession object.
    virtual void destroy(ICdmSession *cdm_session) = 0;
};

} // namespace
