#pragma once

using namespace DirectX;
namespace SpookyAdulthood
{
    struct Segment
    {
        XMFLOAT2 start, end;
    };

    typedef std::vector<Segment> SegmentList;

    XMFLOAT2 FPSCDAndSolving2D(const SegmentList* segs, const XMFLOAT2& inVec);
}