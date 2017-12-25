///
/// \file IOverlayCallbacks.h
///
/// \brief CloudTV Nano SDK Overlay drawing interface.
///
///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <vector>

#include <inttypes.h>

namespace ctvc {

/// \brief Picture parameters used by the overlay callback functions.
///
/// The \a alpha value shall be ignored by the client if the picture that is
/// referred to includes an alpha channel or another transparency mechanism.
/// Pictures shall overwrite image data including its alpha channel at the
/// overlay plane.
///
/// \note The (0,0) coordinate corresponds to the upper left corner. Prior
/// to rendering a rectangle, the client shall clear the rectangular area.
struct PictureParameters
{
    PictureParameters();

    uint16_t x;    ///< The x position on the screen where this picture should be positioned. Coordinate origin is upper left.
    uint16_t y;    ///< The y position on the screen where this picture should be positioned. Coordinate origin is upper left.
    uint16_t w;    ///< The width of the picture in pixels
    uint16_t h;    ///< The height of the picture in pixels
    uint8_t alpha; ///< The picture-transparency value shall only be used if the picture-object-data does not include an alpha
                   /// channel or another transparency mechanism. The picture-transparency parameter range is from 0 to 255,
                   /// where 0 denotes complete transparency. Pictures shall overwrite image data from previous screen updates
                   /// including its alpha channel at the overlay plane
    std::vector<uint8_t> m_data; ///< The image data. The picture-object encoding supports self-identifying picture formats such
                                 /// as JPEG (first byte is 0xFF), PNG (first byte is 0x89) and BMP (first two bytes for some
                                 /// flavors are 'BM' in ASCII).
    std::string m_url; ///< URL where the overlay was retrieved from. This may be empty if the overlay was transmited over RFB-TV.
};

/// \brief Callback interface for overlay images in graphics overlay plane.
/// \todo Implement the methods of IOverlayCallbacks in your own derived class.
/// \note All overlay handling is done in a separate thread inside the Nano SDK. So your code must
///       be prepared to receive calls to the methods in the IOverlayCallbacks interface to arrive
///       in the context of a thread that is \a different from the main thread (i.e. the thread that
///       the operating system uses to call your client's 'main()' function).
///       When called, the overlay_blit_image(), overlay_clear() and overlay_flip() methods \b must
///       block until all (graphics) processing has completed. This is necessary for the 'throttling'
///       mechanism to work: It ensures that your Set-top Box does not get flooded with overlay
///       images for the framebuffer updates.
struct IOverlayCallbacks
{
    IOverlayCallbacks() {}
    virtual ~IOverlayCallbacks() {}

    /// \brief Blit an image to the shadow graphics overlay plane.
    ///
    /// The shadow graphics overlay plane is not visible until \a overlay_flip() is called.
    /// \param [in] picture_params PictureParameters with picture data, x and y coordinates, width and height and alpha channel information.
    virtual void overlay_blit_image(const PictureParameters &picture_params) = 0;

    /// \brief Clear the shadow buffer.
    ///
    /// The buffer itself can remain intact, only the content has to be wiped (e.g, set to black and full transparency).
    virtual void overlay_clear() = 0;

    /// \brief Copy the shadow graphics overlay plane to the visible graphics overlay plane.
    virtual void overlay_flip() = 0;
};

} // namespace
