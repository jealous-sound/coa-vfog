row_major float4x4 cViewToClip : register(c0);
float4 cFog : register(c30);
float4 cViewDepthRow : register(c33);

struct Vertex
{
    float4 position : POSITION;
    float4 colour : COLOR0;
};

struct Interpolants
{
    float4 position : POSITION;
    float4 colour : COLOR0;
    float fog : FOG;
};

Interpolants main(Vertex v)
{
    Interpolants o;
    o.position = mul(v.position, cViewToClip);
    o.colour = v.colour;
    float viewDepth = dot(cViewDepthRow, v.position);
    o.fog = min(pow(max(viewDepth * cFog.x + cFog.y, 0), cFog.z), 1);
    return o;
}
