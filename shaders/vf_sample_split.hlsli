#include "vf_composite.hlsli"

static const float kPlanarCurvatureShare = 0.25;

struct DepthNeighbourhood
{
    float own;
    float nearest;
    float farthest;
    float curvature;
};

float DepthAround(float2 pixel, float2 offset)
{
    return SampleDepth(sDepth, clamp(pixel + offset, ViewportOrigin() + 0.5, ViewportLastPixelCentre()));
}

float SecondDifference(float before, float own, float after)
{
    return abs(before + after - 2 * own);
}

DepthNeighbourhood NeighbourhoodOf(float2 pixel)
{
    float depth[3][3];
    [unroll] for (int y = 0; y < 3; y++)
        [unroll] for (int x = 0; x < 3; x++)
            depth[y][x] = DepthAround(pixel, float2(x - 1, y - 1));
    DepthNeighbourhood n;
    n.own = depth[1][1];
    n.nearest = min(min(min(depth[0][0], depth[0][1]), min(depth[0][2], depth[1][0])),
                    min(min(depth[1][1], depth[1][2]), min(min(depth[2][0], depth[2][1]), depth[2][2])));
    n.farthest = max(max(max(depth[0][0], depth[0][1]), max(depth[0][2], depth[1][0])),
                     max(max(depth[1][1], depth[1][2]), max(max(depth[2][0], depth[2][1]), depth[2][2])));
    n.curvature = max(max(SecondDifference(depth[1][0], n.own, depth[1][2]),
                          SecondDifference(depth[0][1], n.own, depth[2][1])),
                      max(SecondDifference(depth[0][0], n.own, depth[2][2]),
                          SecondDifference(depth[0][2], n.own, depth[2][0])));
    return n;
}

bool SilhouetteWithin(DepthNeighbourhood n)
{
    float nearestViewZ = LinearDepth(n.nearest);
    float farthestViewZ = LinearDepth(n.farthest);
    bool separated = farthestViewZ - nearestViewZ > max(kSameSurfaceAbsoluteDepth,
                                                        nearestViewZ * kSameSurfaceRelativeDepth);
    bool planar = n.curvature <= kPlanarCurvatureShare * (n.farthest - n.nearest);
    return !SameDepthClass(n.nearest, n.farthest) || (separated && !planar);
}

float SplitDepth(DepthNeighbourhood n)
{
    return 0.5 * (n.nearest + n.farthest);
}
