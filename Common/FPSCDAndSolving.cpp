#include "pch.h"
#include "FPSCDAndSolving.h"

using namespace DirectX;

namespace SpookyAdulthood
{
    // Bourke my dearest friend
    static bool FPSCDRaycast(const XMFLOAT2& p1, const XMFLOAT2& p2, const XMFLOAT2& p3, const XMFLOAT2& p4, XMFLOAT2* outHit, XMFLOAT2* outFrac)
    {
        const float den = (p4.y - p3.y)*(p2.x - p1.x) - (p4.x - p3.x)*(p2.y - p1.y);
        if (den == 0.0f) 
            return false; // parallel (no intersection)

        const float iden = 1.0f / den;
        const float fx = ( (p4.x - p3.x)*(p1.y - p3.y) - (p4.y - p3.y)*(p1.x - p3.x) ) * iden;
        if (fx < FLT_EPSILON || fx >(1.0f + FLT_EPSILON)) 
            return false; // out of seg bounds

        const float fy = ((p2.x - p1.x)*(p1.y - p3.y) - (p2.y - p1.y)*(p1.x - p3.x) ) * iden;
        if (fy < FLT_EPSILON || fy >(1.0f + FLT_EPSILON))
            return false; // out of seg bounds

        if (fx == 0.0f && fy == 0.0f)
            return false; // same line

        outFrac->x = fx; outFrac->y = fy;
        outHit->x = p1.x + fx*(p2.x - p1.x);
        outHit->y = p1.y + fy*(p2.y - p1.y);
        return true;
    }

    static float FPSCDClosestDistance(const XMFLOAT2& p1, const XMFLOAT2& p2, const XMFLOAT2& p3)
    {
        XMVECTOR _p1 = XMLoadFloat2(&p1);
        XMVECTOR _p2 = XMLoadFloat2(&p2);        
        const float den = XMVectorGetX(XMVector2Length(XMVectorSubtract(_p2, _p1)));
        if (den == 0.0f)
            return -1.0f;
        const float u = ((p3.x - p1.x)*(p2.x - p1.x) + (p3.y - p1.y)*(p2.y - p1.y)) / den;
        return u;
    }

    XMFLOAT2 FPSCDAndSolving2D(const SegmentList* segs, const XMFLOAT2& curPos, const XMFLOAT2& nextPos, float radius)
    {
        using namespace DirectX::SimpleMath; // make life a bit easier

        if (!segs || segs->empty())
            return nextPos;

        typedef Vector2 Segment[2];

        //// -- raycast left, center, right to segment
        // compute three rays
        Vector2 fwDir = nextPos - curPos; 
        const float len = fwDir.Length(); if (len == 0) return nextPos;        
        fwDir *= 1.0f / len; // normalize        
        const Vector2 riDir(fwDir.y, -fwDir.x);
        const Vector2 center(curPos);
        const Vector2 right = center + riDir*radius;
        const Vector2 left = center - riDir*radius;
        const Vector2 fwExt = fwDir *(len /*+ radius*/);
        // casting rays from {left, center, right}
        Segment charSegments[1] =
        {
            //{left, left+ fwExt },
            {center, center + fwExt },
            //{right, right+ fwExt }
        };

        // check for all segments 
        XMFLOAT2 hit, frac;
        int closest = -1;
        float minFracLenSq = FLT_MAX;
        XMFLOAT2 minHit, minFrac;
        for (const auto& collSeg : *segs)
        {
            /// discard behind of segment
            if (fwDir.Dot(collSeg.normal) >= 0.0f) 
                continue;
            //// -- get closest intersection if any            
            for (int i = 0; i < ARRAYSIZE(charSegments); ++i)
            {
                const Segment& s = charSegments[i];
                if (FPSCDRaycast(s[0], s[1], collSeg.start, collSeg.end, &hit, &frac) )
                {
                    const float lsq = frac.x*frac.x + frac.y*frac.y;
                    if (lsq < minFracLenSq)
                    {
                        closest = i;
                        minFracLenSq = lsq;
                        minHit = hit;
                        minFrac = frac;
                    }
                }
            }
        }

        if (closest != -1)
        {
            auto& collSeg = (*segs)[closest];
            float cd = FPSCDClosestDistance(collSeg.start, collSeg.end, curPos);
            if (cd >= 0.0f)
            {
                return minHit + collSeg.normal*cd;
            }
            return curPos;// curPos + nd*len;
        }

        // final pos is closest + normal*amount_exit_penetration

        return nextPos;
    }

};