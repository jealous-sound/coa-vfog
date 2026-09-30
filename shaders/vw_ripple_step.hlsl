float4 cRippleStep : register(c0);
float4 cRippleSegments[32] : register(c1);
float4 cRippleShapes[32] : register(c33);

sampler2D sRippleState : register(s0);

static const int kMaxDisturbances = 32;
static const float kDamping = 0.97;
static const float kEdgeRampPerUv = 16;
static const float kNeighbourWeight = 0.5;
static const float kFootprintCore = 0.20;
static const float kFootprintRim = 0.79;

float2 WindowShift()
{
    return cRippleStep.xy;
}

float InverseTexels()
{
    return cRippleStep.z;
}

float DisturbanceCount()
{
    return cRippleStep.w;
}

float2 State(float2 uv)
{
    return tex2Dlod(sRippleState, float4(uv, 0, 0)).rg;
}

float EdgeRamp(float2 uv)
{
    float2 inward = min(uv, 1 - uv) * kEdgeRampPerUv;
    return saturate(min(inward.x, inward.y));
}

float2 Propagate(float2 uv)
{
    float texel = InverseTexels();
    float2 centre = State(uv);
    float left = State(uv - float2(texel, 0)).r;
    float down = State(uv - float2(0, texel)).r;
    float right = State(uv + float2(texel, 0)).r;
    float up = State(uv + float2(0, texel)).r;
    float next = kDamping * EdgeRamp(uv) * (kNeighbourWeight * (left + down + right + up) - centre.g);
    return float2(next, centre.r);
}

float SegmentDistance(float2 p, float2 from, float2 to)
{
    float2 along = to - from;
    float lengthSquared = dot(along, along);
    float t = lengthSquared > 0 ? saturate(dot(p - from, along) / lengthSquared) : 0;
    return length(p - from - along * t);
}

float Footprint(float distance, float inverseRadius)
{
    return 1 - smoothstep(kFootprintCore, kFootprintRim, distance * inverseRadius);
}

float Injection(float2 texelCentre)
{
    float sum = 0;
    [unroll] for (int i = 0; i < kMaxDisturbances; i++)
    {
        [branch] if (i < DisturbanceCount())
            sum += cRippleShapes[i].y *
                   Footprint(SegmentDistance(texelCentre, cRippleSegments[i].xy, cRippleSegments[i].zw),
                             cRippleShapes[i].x);
    }
    return sum;
}

float4 main(float2 pixel : VPOS) : COLOR0
{
    float2 texel = floor(pixel);
    float2 state = Propagate((texel + WindowShift() + 0.5) * InverseTexels());
    return float4(state.x + Injection(texel + 0.5), state.y, 0, 0);
}
