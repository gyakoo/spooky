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

        inline bool IsValid() const { return start.x != end.x || start.y != end.y; }
        inline bool IsDisabled() const { return (flags & DISABLED) != 0; }
        inline bool IsPortalOpen() const { return IsDisabled() && (flags & PORTAL)!=0; }
    };

    typedef std::vector<CollSegment> SegmentList;

    XMFLOAT2 CollisionAndSolving2D(const SegmentList* segs, const XMFLOAT2& curPos, const XMFLOAT2& nextPos, float radius, int iter=3);

    bool IntersectRaySegment(const XMFLOAT2& origin, const XMFLOAT2& dir, const CollSegment& seg,  XMFLOAT2& outHit, float& outFrac);
    bool IntersectRayPlane(const XMFLOAT3& origin, const XMFLOAT3& dir, const XMFLOAT3& normal, const XMFLOAT3& p, XMFLOAT3& outHit, float& outFrac);
    bool IntersectRaySphere(const XMFLOAT3& origin, const XMFLOAT3& dir, const XMFLOAT3& center, float radius, XMFLOAT3& outHit, float& outFrac);
    bool IntersectRayTriangle(const XMFLOAT3& origin, const XMFLOAT3& dir, const XMFLOAT3 V[3], XMFLOAT3& barycentric, float& outFrac);
    bool IntersectRayBillboardQuad(const XMFLOAT3& raypos, const XMFLOAT3& dir, const XMFLOAT3& quadCenter, const XMFLOAT2& quadSize, XMFLOAT3& outHit, float& outFrac);

}