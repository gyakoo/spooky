#include "pch.h"
#include "CollisionAndSolving.h"
#include "../Common/DeviceResources.h"

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
    XMFLOAT2 CollisionAndSolving2D(const SegmentList* segs, const XMFLOAT2& curPos, const XMFLOAT2& nextPos, float radius, int iter)
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
        const Vector2 riDir(fwDir.y, -fwDir.x);
        const Vector2 center(curPos);
        const Vector2 right = center + riDir*radius;
        const Vector2 left = center - riDir*radius;
        const Vector2 fwExt = fwDir *(len + radius);
        // casting rays from {left, center, right}
        Segment charSegments[3] =
        {
            {left, left+ fwExt },
            {center, center + fwExt },
            {right, right+ fwExt }
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
            if (!collSeg.IsDisabled()) // if segment isn't disabled
            {
                //// -- get closest intersection if any            
                for (int i = 0; i < ARRAYSIZE(charSegments); ++i)
                {
                    const Segment& s = charSegments[i];
                    if (FPSCDRaycast(s[0], s[1], collSeg.start, collSeg.end, &hit, &frac))
                    {
                        if (frac < minFrac)
                        {
                            minFrac = frac;
                            closest = j;
                            minHit = hit;
                        }
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
            const float signMov = float(t > 0.0f) - (t < 0.0f);// sign of wall mov direction
            wallSlideDir.Normalize();
            //const float d = -Vector2(collSeg.normal).Dot(nextPos - curPos); // project movement to wall
            const float d = len;
            return CollisionAndSolving2D(segs, curPos, curPos + wallSlideDir*d*signMov, radius, --iter);
        }

        return nextPos;
    }

    bool IntersectRaySegment(const XMFLOAT2& origin, const XMFLOAT2& dir, const CollSegment& seg, XMFLOAT2& outHit, float& outFrac)
    {
        XMFLOAT2 endP(origin.x + dir.x*1000.0f, origin.y + dir.y*1000.0f);
        return FPSCDRaycast(origin, endP, seg.start, seg.end, &outHit, &outFrac);
    }

    static inline bool solveQuadratic(const float &a, const float &b, const float &c, float &x0, float &x1)
    {
        const float discr = b * b - 4 * a * c;
        if (discr < 0) return false;
        else if (discr == 0) x0 = x1 = -0.5f * b / a;
        else {
            float q = (b > 0) ?
                -0.5f * (b + sqrt(discr)) :
                -0.5f * (b - sqrt(discr));
            x0 = q / a;
            x1 = c / q;
        }
        if (x0 > x1) std::swap(x0, x1);

        return true;
    }

    static inline float DotProduct(const XMFLOAT3& a, const XMFLOAT3& b)
    {
        return a.x*b.x + a.y*b.y + a.z*b.z;
    }

    bool IntersectRaySphere(const XMFLOAT3& origin, const XMFLOAT3& dir, const XMFLOAT3& center, float radius, XMFLOAT3& outHit, float& outFrac)
    {
        const XMFLOAT3 L(origin.x - center.x, origin.y - center.y, origin.z - center.z);
        const float a = DotProduct(dir, dir);
        const float b = 2.0f * DotProduct(dir, L);
        const float c = DotProduct(L,L) - radius;
        float t0, t1;
        if (!solveQuadratic(a, b, c, t0, t1))
            return false;
        if (t0 > t1)
            std::swap(t0, t1);
        if (t0 < 0.0f)
        {
            t0 = t1; // if t0 is negative, use t1 instead
            if (t0 < 0.0f) 
                return false; // both negatives
        }
        outFrac = t0;
        outHit.x = origin.x + dir.x *t0;
        outHit.y = origin.y + dir.y *t0;
        outHit.z = origin.z + dir.z *t0;
        return true;
    }

    bool IntersectRayPlane(const XMFLOAT3& origin, const XMFLOAT3& dir, const XMFLOAT3& normal, const XMFLOAT3& p, XMFLOAT3& outHit, float& outFrac)
    {
        const float den = DotProduct(normal, dir);
        if (den > 1e-6f )
        {
            XMFLOAT3 po(p.x - origin.x, p.y - origin.y, p.z - origin.z);
            const float f = DotProduct(po, normal) / den;
            outHit.x = origin.x + dir.x*f;
            outHit.y = origin.y + dir.y*f;
            outHit.z = origin.z + dir.z*f;
            outFrac = f;
            return f >= 0.0f;
        }
        return false;
    }

    bool IntersectRayTriangle(const XMFLOAT3& P, const XMFLOAT3& w, const XMFLOAT3 V[3], XMFLOAT3& barycentric, float& t)
    {
        // Edge vectors
        const XMFLOAT3 e_1 = XM3Sub(V[1], V[0]);
        const XMFLOAT3 e_2 = XM3Sub(V[2], V[0]);

        // Face normal
        const XMFLOAT3 n = XM3Normalize(XM3Cross(e_1, e_2));
        const XMFLOAT3 q = XM3Cross(w, e_2);
        const float a = XM3Dot(e_1, q);

        // Backfacing or nearly parallel?
        if (/* XM3Dot(n,w) >= 0.0f || */abs(a) <= 0.0001f) 
            return false;

        const float inva = 1.0f / a;
        const XMFLOAT3 s = XM3Mul(XM3Sub(P, V[0]), inva);
        const XMFLOAT3 r = XM3Cross(s, e_1);
        barycentric.x = XM3Dot(s, q);
        barycentric.y = XM3Dot(r, w);
        barycentric.z = 1.0f - barycentric.x - barycentric.y;

        // Intersected outside triangle?
        if ((barycentric.x < 0.0f) || (barycentric.y < 0.0f) || (barycentric.z <0.0f)) 
            return false;
        t = XM3Dot(e_2, r);
        return t >= 0.0f;
    }

    inline void TransformQuad(XMFLOAT3* fourVertices, float yaw, const XMFLOAT3& pos, const XMFLOAT2& size)
    {
        using namespace DirectX::SimpleMath;

        XMMATRIX mr = XMMatrixRotationY(yaw);
        mr.r[3] = XMVectorSet(pos.x, pos.y, pos.z, 1.0f);
        XMMATRIX ms = XMMatrixScaling(size.x, size.y, 1.0f);
        XMMATRIX model = XMMatrixMultiply(ms, mr);
        Matrix modelMatrix(model);

        for (int i = 0; i < 4; ++i)
        {
            fourVertices[i] = Vector3::Transform(fourVertices[i], modelMatrix);
        }
    }

    bool IntersectRayBillboardQuad(const XMFLOAT3& raypos, const XMFLOAT3& dir, const XMFLOAT3& quadCenter, const XMFLOAT2& quadSize, XMFLOAT3& outHit, float& outFrac )
    {
        // two triangles (these values correspond to sprite.cpp)
        // todo: store these values already transformed in the sprite rather than transforming here.
        typedef XMFLOAT3 v3;
        static v3 vb[4] = {
            v3(-0.5f,-0.5f,0),
            v3(0.5f,-0.5f,0),
            v3(0.5f, 0.5f,0),
            v3(-0.5f, 0.5f, 0)
        };
        v3 vbLocal[4];
        memcpy_s(vbLocal, sizeof(v3) * 4, vb, sizeof(v3) * 4);
        auto& cam = DX::GameResources::instance->m_camera;
        TransformQuad(vbLocal, cam.m_pitchYaw.y, quadCenter, quadSize);
        XMFLOAT3 tri[3] = { vbLocal[0], vbLocal[2], vbLocal[3] };
        XMFLOAT3 bar;
        if (IntersectRayTriangle(raypos, dir, tri, bar, outFrac))
        {
            outHit = XM3Mad(raypos, dir, outFrac);
            return true;
        }
        else
        {
            tri[0] = vbLocal[0];
            tri[1] = vbLocal[1];
            tri[2] = vbLocal[2];
            if (IntersectRayTriangle(raypos, dir, tri, bar, outFrac))
            {
                outHit = XM3Mad(raypos, dir, outFrac);
                return true;
            }
        }

        return false;
    }


};

