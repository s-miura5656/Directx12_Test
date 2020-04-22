#include "BaseShaderHeader.hlsli"

float4 BasePS(Output input) : SV_TARGET
{
    float3 light = normalize(float3(1, -1, 1));
    
    // �f�B�t���[�Y�v�Z
    float diffuseB = saturate(dot(-light, input.normal.xyz));
    float4 toonDif = toon.Sample(smpToon, float2(0, 1.0 - diffuseB));
    
    // ���̔��˃x�N�g��
    float3 refLight = normalize(reflect(light, input.normal.xyz));
    float specularB = pow(saturate(dot(refLight, -input.ray)), specular.a);
    
    // �X�t�B�A�}�b�v�pUV�v�Z
    float2 sphereMapUV = input.vnormal.xy;
    sphereMapUV = (sphereMapUV + float2(1, -1)) * float2(0.5, -0.5);
    
    float4 texColor = tex.Sample(smp, input.uv);
    
    return max(saturate(toonDif
             * diffuse
             * texColor
             * sph.Sample(smp, sphereMapUV))
             + saturate(spa.Sample(smp, sphereMapUV) * texColor
             + float4(specularB * specular.rgb, 1)),
               float4(texColor.rgb * (ambient * 0.25), 1));

}