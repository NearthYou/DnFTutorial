// ShaderW0.hlsl
// 텍스처를 풀스크린 사각형에 샘플링해 출력

Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

struct VS_INPUT
{
    float3 position : POSITION;  // xy: NDC(-1..1), z: unused
    float2 uv       : TEXCOORD0; // 0..1
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 uv       : TEXCOORD0;
};

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;
    output.position = float4(input.position, 1.0f);
    output.uv = input.uv;
    return output;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    return gTexture.Sample(gSampler, input.uv);
}
