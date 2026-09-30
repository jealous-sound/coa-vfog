sampler2D sDepth : register(s0);
float4 cDepthTexelUv : register(c0);

float4 main() : COLOR0
{
    return tex2Dlod(sDepth, float4(cDepthTexelUv.xy, 0, 0)).rrrr;
}
