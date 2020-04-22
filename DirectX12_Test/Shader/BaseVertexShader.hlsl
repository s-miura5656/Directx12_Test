#include "BaseShaderHeader.hlsli"

Output BaseVS( float4 pos        : POSITION,
               float4 normal     : NORMAL,                
               float2 uv         : TEXCOORD,
               min16uint2 boneno : BONE_NO,
               min16uint weight  : WEIGHT
)
{
    Output output;
    pos            = mul(world, pos);
    output.svpos   = mul(mul(proj, view), pos);
    output.pos     = mul(view, pos);
    normal.w       = 0;                  // ���s�ړ������𖳌��ɂ���
    output.normal  = mul(world, normal); // �@���ɂ����[���h�ϊ����s��
    output.vnormal = mul(view, output.normal);
    output.uv      = uv;
    output.ray     = normalize(pos.xyz - eye);
    
	return output;
}