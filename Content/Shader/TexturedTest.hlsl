struct FModelContext
{
    row_major float4x4 World;
    uint MaterialIndex;
    uint Flags;
};

struct FMaterial
{
    float4 BaseColor;
    
    // Paddings
    float4 Parameters0;
    float4 Parameters1;
    float4 Parameters2;
    float4 Parameters3;
    float4 Parameters4;
    float4 Parameters5;
    float4 Parameters6;
};
StructuredBuffer<FModelContext> ModelContexts : register(t0);
#include "Lighting.hlsli"
StructuredBuffer<FMaterial> MaterialBuffer : register(t1);
Texture2D BaseColorTexture : register(t3);

SamplerState LinearWrap : register(s0);

cbuffer RootConstants : register(b0)
{
    row_major float4x4 View;
    row_major float4x4 Projection;
    row_major float4x4 ViewProjection;

    uint ModelContextStart;
    uint LightCount;
};

struct VS_INPUT
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float2 UV : TEXCOORD0;
};

struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float3 Normal : NORMAL;
    float2 UV : TEXCOORD0;
    float3 WorldPosition : TEXCOORD1;
    nointerpolation uint MaterialIndex : Jungle1;
    nointerpolation float3 ColorCoefficient : Jungle2;
    nointerpolation uint Flags : Jungle3;
};

PS_INPUT mainVS(VS_INPUT Input, uint InstanceID : SV_InstanceID)
{
    PS_INPUT Output;

    FModelContext ModelContext = ModelContexts[ModelContextStart + InstanceID];
    float4 WorldPosition = mul(float4(Input.Position, 1.0f), ModelContext.World);

    Output.Position = mul(WorldPosition, ViewProjection);
    Output.Normal = mul(Input.Normal, (float3x3)ModelContext.World);
    Output.UV = Input.UV;
    Output.WorldPosition = WorldPosition.xyz;
    Output.MaterialIndex = ModelContext.MaterialIndex;
    Output.Flags = ModelContext.Flags;
    Output.ColorCoefficient = float3(1.f, 1.f, 1.f);

    return Output;
}

float4 mainPS(PS_INPUT Input) : SV_TARGET
{
    float4 Color = MaterialBuffer[Input.MaterialIndex].BaseColor;

    uint Width;
    uint Height;
    BaseColorTexture.GetDimensions(Width, Height);

    if (Width > 0 && Height > 0)
    {
        Color = BaseColorTexture.Sample(LinearWrap, Input.UV);
    }

    Color.rgb *= Input.ColorCoefficient;
    return Color;
}