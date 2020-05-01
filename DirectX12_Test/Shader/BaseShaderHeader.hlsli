Texture2D<float4> tex  : register(t0);
Texture2D<float4> sph  : register(t1);
Texture2D<float4> spa  : register(t2);
Texture2D<float4> toon : register(t3);

SamplerState smp     : register(s0);
SamplerState smpToon : register(s1);

cbuffer SceneData : register(b0)
{
    matrix view; // �r���[
    matrix proj; // �v���W�F�N�V����
    float3 eye;  // �����x�N�g��
};

cbuffer Transform : register(b1)
{
    matrix world; // ���[���h�ϊ��s��
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
    float4 svpos   : SV_POSITION; // �V�X�e���p���_���W
    float4 pos     : POSITION;    // ���_���W
    float4 normal  : NORMAL0;     // �@���x�N�g��
    float4 vnormal : NORMAL1;     // �r���[�ϊ���̖@���x�N�g��
    float2 uv      : TEXCOORD;    // uv �l
    float3 ray     : VECTOR;      // �����x�N�g��
};