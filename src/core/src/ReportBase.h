///
/// \file ReportBase.h
///
/// \brief CloudTV Nano SDK client report base class.
/// It contains no real functionality (yet) but is meant to group all reports
/// into one single class.
///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

namespace ctvc
{

class ReportBase
{
public:
    ReportBase()
    {
    }

    virtual ~ReportBase()
    {
    }

    virtual void reset() = 0;

private:
    ReportBase(const ReportBase &rhs);
    ReportBase &operator=(const ReportBase &rhs);
};

} // namespace
