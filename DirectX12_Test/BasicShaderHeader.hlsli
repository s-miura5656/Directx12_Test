Texture2D<float4> tex : register(t0);
Texture2D<float4> sph : register(t1);
Texture2D<float4> spa : register(t2);

SamplerState smp : register(s0);

cbuffer cbuff0 : register(b0)
{
    matrix world;
    matrix viewproj;
};

cbuffer Material : register(b1)
{
    float4 diffuse;
    float4 specular;
    float3 ambient;
};

struct Output
{
    float4 svpos  : SV_POSITION; // �V�X�e���p���_���W
    float4 normal : NORMAL;
    float2 uv     : TEXCOORD;
};