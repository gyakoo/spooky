#pragma once

using namespace DirectX;
namespace SpookyAdulthood
{
    struct CollSegment
    {
        enum{ NONE=0, WALL=1, PORTAL=2, DISABLED=4};
        XMFLOAT2 start, end;
        XMFLOAT2 normal;
        int flags;

        bool IsValid() const { return start.x != end.x || start.y != end.y; }
        bool IsDisabled() const { return (flags & DISABLED) != 0; }
        bool IsPortalOpen() const { return (flags & (PORTAL | DISABLED)) != 0; }
    };

    typedef std::vector<CollSegment> SegmentList;

    XMFLOAT2 CollisionAndSolving2D(const SegmentList* segs, const XMFLOAT2& curPos, const XMFLOAT2& nextPos, float radius, int iter=3);

    bool CollisionIntersectSegment(const CollSegment& seg, const XMFLOAT2& origin, const XMFLOAT2& dir, XMFLOAT2& outHit, float& outFrac);
}