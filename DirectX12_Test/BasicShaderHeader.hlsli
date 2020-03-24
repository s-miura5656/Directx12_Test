Texture2D<float4> tex : register(t0);
Texture2D<float4> sph : register(t1);
Texture2D<float4> spa : register(t2);

SamplerState smp : register(s0);

cbuffer cbuff0 : register(b0)
{
    matrix world;
    matrix view;
    matrix proj;
};

cbuffer Material : register(b1)
{
    float4 diffuse;
    float4 specular;
    float3 ambient;
};

struct Output
{
    float4 svpos   : SV_POSITION; // システム用頂点座標
    float4 pos     : POSITION;    // 頂点座標
    float4 normal  : NORMAL0;     // 法線ベクトル
    float4 vnormal : NORMAL1;     // ビュー変換後の法線ベクトル
    float2 uv      : TEXCOORD;    // uv 値
};