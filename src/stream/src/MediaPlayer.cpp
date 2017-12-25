/// \file MediaPlayer.cpp
/// \brief IMediaPlayer error codes
///
///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include <stream/IMediaPlayer.h>

using namespace ctvc;

const ResultCode IMediaPlayer::CABLE_TUNING_ERROR("There was a tuning error when trying to tune to a channel");
const ResultCode IMediaPlayer::CONNECTION_FAILED("Connection to a remote host could not be established");
