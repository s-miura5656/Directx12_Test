Texture2D<float4> tex : register(t0);
SamplerState smp : register(s0);

cbuffer cbuff0 : register(b0)
{
    matrix mat;
};

struct Output
{
    float4 svpos : SV_POSITION; // システム用頂点座標
    float2 uv    : TEXCOORD;
};