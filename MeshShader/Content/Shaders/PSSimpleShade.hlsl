//--------------------------------------------------------------------------------------
// Copyright (c) XU, Tianchen. All rights reserved.
//--------------------------------------------------------------------------------------

#define PI  3.1415926

struct PSIn
{
	float4	Pos		: SV_POSITION;
	float3	PosW	: POSWORLD;
	float3	Nrm		: NORMAL;
};

cbuffer cbPerFrame : register (b1)
{
	float3 g_eyePt;
};

const static float3 g_lightPos = float3(1.0, 2.0, -1.0);
const static float4 g_light = float4(1.0, 0.8, 0.6, 5.0);
const static float4 g_ambient = float4(0.4, 0.7, 0.9, 1.6);

float4 main(PSIn input) : SV_TARGET
{
	const float3 L = normalize(g_lightPos);
	const float3 N = normalize(input.Nrm);
	const float NoL = saturate(dot(N, L));

	const float3 lightColor = g_light.xyz * g_light.w;
	const float3 ambient = g_ambient.xyz * g_ambient.w;
	float3 result = NoL * lightColor + (N.y * 0.5 + 0.5) * ambient;
	result /= dot(result, float3(0.25, 0.5, 0.25)) + 1.0;

	return float4(result, 1.0);
}
