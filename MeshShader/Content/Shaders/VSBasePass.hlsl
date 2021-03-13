//--------------------------------------------------------------------------------------
// Copyright (c) XU, Tianchen. All rights reserved.
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// Structs
//--------------------------------------------------------------------------------------
struct VSIn
{
	float3	Pos : POSITION;
	float3	Nrm : NORMAL;
};

struct VSOut
{
	float4	Pos		: SV_POSITION;
	float3	PosW	: POSWORLD;
	float3	Nrm		: NORMAL;
};

//--------------------------------------------------------------------------------------
// Constant buffers
//--------------------------------------------------------------------------------------
cbuffer cbMatrices
{
	matrix g_worldViewProj;
	float4x3 g_world;
	float3x3 g_normal;
};

//--------------------------------------------------------------------------------------
// Base vertex processing
//--------------------------------------------------------------------------------------
VSOut main(VSIn input, uint vid : SV_VertexID)
{
	VSOut output;

	const float4 pos = float4(input.Pos, 1.0);
	output.Pos = mul(pos, g_worldViewProj);
	output.Nrm = mul(input.Nrm, g_normal);
	output.PosW = mul(pos, g_world);

	return output;
}
