#include "pch.h"
#include "FPSCDAndSolving.h"

using namespace DirectX;

namespace SpookyAdulthood
{
    // P Bourke my good old friend...
    static bool FPSCDRaycast(const XMFLOAT2& p1, const XMFLOAT2& p2, const XMFLOAT2& p3, const XMFLOAT2& p4, XMFLOAT2* outHit, float* outFrac)
    {
        const float den = (p4.y - p3.y)*(p2.x - p1.x) - (p4.x - p3.x)*(p2.y - p1.y);
        if (den == 0.0f) 
            return false; // parallel (no intersection)

        const float iden = 1.0f / den;
        const float fx = ( (p4.x - p3.x)*(p1.y - p3.y) - (p4.y - p3.y)*(p1.x - p3.x) ) * iden;
        if (fx < 0 || fx >1.0f) 
            return false; // out of seg bounds

        const float fy = ((p2.x - p1.x)*(p1.y - p3.y) - (p2.y - p1.y)*(p1.x - p3.x) ) * iden;
        if (fy < 0 || fy >1.0f)
            return false; // out of seg bounds

        if (fx == 0.0f && fy == 0.0f)
            return false; // same line

        *outFrac = fx;
        outHit->x = p1.x + fx*(p2.x - p1.x);
        outHit->y = p1.y + fx*(p2.y - p1.y);
        return true;
    }

    static float FPSCDClosestDistance(const XMFLOAT2& p1, const XMFLOAT2& p2, const XMFLOAT2& p3)
    {
        const XMVECTOR _p1 = XMLoadFloat2(&p1);
        const XMVECTOR _p2 = XMLoadFloat2(&p2);        
        const float den = XMVectorGetX(XMVector2Length(XMVectorSubtract(_p2, _p1)));
        if (den == 0.0f)
            return -1.0f;
        const float u = ((p3.x - p1.x)*(p2.x - p1.x) + (p3.y - p1.y)*(p2.y - p1.y)) / den;
        return u;
    }

    // this is ugly af do I work at Havok? it does not seem so :S no time , scarejam!
    // iterative (recursive actually) solving against walls.
    // cast ray from center of circle, gets the closest hit, compute wall moving direction with closest segment and check next
    XMFLOAT2 FPSCDAndSolving2D(const SegmentList* segs, const XMFLOAT2& curPos, const XMFLOAT2& nextPos, float radius, int iter)
    {
        using namespace DirectX::SimpleMath; // make life a bit easier

        if (!segs || segs->empty() || iter==0)
            return nextPos;

        typedef Vector2 Segment[2];

        //// -- raycast left, center, right to segment
        // compute three rays
        Vector2 fwDir = nextPos - curPos; 
        const float len = fwDir.Length(); if (len == 0) return nextPos;        
        fwDir *= 1.0f / len; // normalize        
        //const Vector2 riDir(fwDir.y, -fwDir.x);
        const Vector2 center(curPos);
        //const Vector2 right = center + riDir*radius;
        //const Vector2 left = center - riDir*radius;
        const Vector2 fwExt = fwDir *(len + radius);
        // casting rays from {left, center, right}
        Segment charSegments[1] =
        {
            //{left, left+ fwExt },
            {center, center + fwExt },
            //{right, right+ fwExt }
        };

        // check for all segments
        XMFLOAT2 hit;
        float frac;
        int closest = -1;
        float minFrac = FLT_MAX;
        XMFLOAT2 minHit;
        int j = 0;
        for (const auto& collSeg : *segs)
        {
            //// -- get closest intersection if any            
            for (int i = 0; i < ARRAYSIZE(charSegments); ++i)
            {
                const Segment& s = charSegments[i];
                if (FPSCDRaycast(s[0], s[1], collSeg.start, collSeg.end, &hit, &frac) )
                {
                    if (frac < minFrac)
                    {
                        minFrac = frac;
                        closest = j;
                        minHit = hit;
                    }
                }
            }
            ++j;
        }

        if (closest != -1)
        {
            const auto& collSeg = segs->at((size_t)closest);
            Vector2 wallSlideDir = collSeg.end - collSeg.start; 
            const float t = wallSlideDir.Dot(fwDir); 
            const float signMov = (t > 0.0f) - (t < 0.0f);// sign of wall mov direction
            wallSlideDir.Normalize();
            const float d = -Vector2(collSeg.normal).Dot(nextPos - curPos); // project movement to wall
            return FPSCDAndSolving2D(segs, curPos, curPos + wallSlideDir*d*signMov, radius, --iter);
        }

        return nextPos;
    }

};