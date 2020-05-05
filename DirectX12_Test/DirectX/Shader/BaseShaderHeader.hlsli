Texture2D<float4> tex  : register(t0);
Texture2D<float4> sph  : register(t1);
Texture2D<float4> spa  : register(t2);
Texture2D<float4> toon : register(t3);

SamplerState smp     : register(s0);
SamplerState smpToon : register(s1);

cbuffer SceneData : register(b0)
{
    matrix view; // ビュー
    matrix proj; // プロジェクション
    float3 eye;  // 視線ベクトル
};

cbuffer Transform : register(b1)
{
    matrix world; // ワールド変換行列
    matrix bones[256];
}

cbuffer Material : register(b2)
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
    float3 ray     : VECTOR;      // 視線ベクトル
};