float4 cLocalLightControl : register(c53);
sampler2D sLocalLights : register(s8);

float4 LocalLightParameter(int index)
{
    return tex2Dlod(sLocalLights, float4((index + 0.5) / 32.0, 0.5, 0, 0));
}

float LocalLayerScattering(FogLayer layer, float3 viewDirection, float3 lightPosition, float3 attenuation,
                           float radius, float intervalStart, float intervalEnd, float fraction,
                           float cameraHeight, float heightPerYard, float skyDensityScale)
{
    float distanceAlongRay = lerp(intervalStart, intervalEnd, fraction);
    float3 toLight = lightPosition - viewDirection * distanceAlongRay;
    float distanceSquared = dot(toLight, toLight);
    float lightDistance = sqrt(max(distanceSquared, 1e-6));
    float falloff = rcp(max(attenuation.x + attenuation.y * lightDistance + attenuation.z * distanceSquared, 1));
    float boundaryFade = saturate((radius - lightDistance) * 4 / max(radius, 1e-3));
    float phase = lerp(PhaseHG(layer.g, dot(viewDirection, toLight / lightDistance)), 1, layer.isotropic);
    float density = layer.density * skyDensityScale * DistanceCurve(layer, distanceAlongRay) *
                    HeightProfile(layer, cameraHeight + heightPerYard * distanceAlongRay);
    [branch] if (layer.densityVariation > 0 && cDensityVariation.x > 0)
        density *= DensityVariation(CameraPositionWorld() +
                                     ViewToWorldDirection(viewDirection) * distanceAlongRay);
    return density * phase * falloff * boundaryFade;
}

float3 LocalLightScattering(FogLayer layer, float3 viewDirection, float stepStart, float stepEnd,
                            float cameraHeight, float heightPerYard, float skyDensityScale)
{
    float3 scattering = 0;
    [loop] for (int light = 0; light < cLocalLightControl.x; ++light)
    {
        float4 positionRadius = LocalLightParameter(light * 3);
        float projected = dot(positionRadius.xyz, viewDirection);
        float perpendicularSquared = max(dot(positionRadius.xyz, positionRadius.xyz) - projected * projected, 0);
        float discriminant = positionRadius.w * positionRadius.w - perpendicularSquared;
        [branch] if (discriminant > 0)
        {
            float halfChord = sqrt(discriminant);
            float intervalStart = max(max(stepStart, layer.start), projected - halfChord);
            float intervalEnd = min(min(stepEnd, layer.limit), projected + halfChord);
            [branch] if (intervalEnd > intervalStart && layer.density > 0)
            {
                float3 attenuation = LocalLightParameter(light * 3 + 2).xyz;
                float quadrature = 0;
                [loop] for (int sample = 0; sample < 2; ++sample)
                {
                    float fraction = 0.2113248654 + sample * 0.5773502692;
                    quadrature += LocalLayerScattering(layer, viewDirection, positionRadius.xyz, attenuation,
                        positionRadius.w, intervalStart, intervalEnd, fraction,
                        cameraHeight, heightPerYard, skyDensityScale);
                }
                scattering += LocalLightParameter(light * 3 + 1).rgb *
                              (0.5 * quadrature * (intervalEnd - intervalStart));
            }
        }
    }
    return scattering;
}
