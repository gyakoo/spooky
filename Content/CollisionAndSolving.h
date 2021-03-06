﻿#pragma once

using namespace DirectX;
namespace SpookyAdulthood
{
    struct CollSegment
    {
        enum
        { 
            NONE        = 0, 
            WALL        = 1<<0, 
            PORTAL      = 1<<1, 
            PILLAR      = 1<<2, 
            DISABLED    = 1<<3
        };
        XMFLOAT2 start, end;
        XMFLOAT2 normal;
        int flags;

        inline bool operator ==(const CollSegment& rhs) const { return EqualTo(rhs); }
        inline bool EqualTo(const CollSegment& rhs)const { return XM2Eq(start, rhs.start) && XM2Eq(end, rhs.end) && XM2Eq(normal, XM2Neg(rhs.normal)); }
        inline bool IsValid() const { return start.x != end.x || start.y != end.y; }
        inline bool IsDisabled() const { return (flags & DISABLED) != 0; }
        inline bool IsPortalOpen() const { return IsDisabled() && (flags & PORTAL)!=0; }
        inline bool IsPortal() const { return (flags&PORTAL) != 0; }
        inline void SetDisabled(bool dis)
        {
            if (dis) flags |= DISABLED;
            else flags &= ~DISABLED;
        }
    };

    typedef std::vector<CollSegment> SegmentList;

    XMFLOAT2 CollisionAndSolving2D(const SegmentList* segs, const XMFLOAT2& curPos, const XMFLOAT2& nextPos, float radius, int iter=3);

    bool IntersectRaySegment(const XMFLOAT2& origin, const XMFLOAT2& dir, const CollSegment& seg,  XMFLOAT2& outHit, float& outFrac);
    bool IntersectRayPlane(const XMFLOAT3& origin, const XMFLOAT3& dir, const XMFLOAT3& normal, const XMFLOAT3& p, XMFLOAT3& outHit, float& outFrac);
    bool IntersectRaySphere(const XMFLOAT3& origin, const XMFLOAT3& dir, const XMFLOAT3& center, float radius, XMFLOAT3& outHit, float& outFrac);
    bool IntersectRayTriangle(const XMFLOAT3& origin, const XMFLOAT3& dir, const XMFLOAT3 V[3], XMFLOAT3& barycentric, float& outFrac);
    bool IntersectRayBillboardQuad(const XMFLOAT3& raypos, const XMFLOAT3& dir, const XMFLOAT3& quadCenter, const XMFLOAT2& quadSize, XMFLOAT3& outHit, float& outFrac);

}