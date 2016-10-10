#pragma once

using namespace DirectX;
namespace SpookyAdulthood
{
    struct CollSegment
    {
        XMFLOAT2 start, end;
        XMFLOAT2 normal;
        bool IsValid() const { return start.x != end.x || start.y != end.y; }
    };

    typedef std::vector<CollSegment> SegmentList;

    XMFLOAT2 FPSCDAndSolving2D(const SegmentList* segs, const XMFLOAT2& inVec);
}