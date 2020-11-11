//--------------------------------------------------------------------------------------
// Copyright (c) XU, Tianchen. All rights reserved.
//--------------------------------------------------------------------------------------

#define MAX_PRIM_COUNT	32
#define GROUP_SIZE	96
#define MAX_VERT_COUNT	GROUP_SIZE

#define main VSMain
#include "VSBasePass.hlsl"
#undef main
#include "MeshletGen.hlsli"

//--------------------------------------------------------------------------------------
// Meshlet processing
//--------------------------------------------------------------------------------------
[NumThreads(GROUP_SIZE, 1, 1)]
[OutputTopology("triangle")]
void main(//uint DTid : SV_DispatchThreadID,
	uint GTid : SV_GroupThreadID,
	uint Gid : SV_GroupID,
	out indices uint3 tris[MAX_PRIM_COUNT],
	out vertices VSOut verts[MAX_VERT_COUNT])
	//out primitives uint prims[PRIM_COUNT] : SV_PrimitiveID)
{
	const uint primCount = (Gid + 1) * MAX_PRIM_COUNT > g_primCount ? g_primCount % MAX_PRIM_COUNT : MAX_PRIM_COUNT;
	const uint uniqueIdxBase = (MAX_PRIM_COUNT + MAX_VERT_COUNT + 1) * Gid;
	const uint localIdxBase = uniqueIdxBase + primCount * 3;
	const uint vertCountIdx = localIdxBase + primCount;

	const uint vertexCount = g_meshletIdxBuffer[vertCountIdx];
	//DeviceMemoryBarrierWithGroupSync();
	
	SetMeshOutputCounts(vertexCount, primCount);

	if (vertexCount == 0)
	{
		// Generate meshlet
		if (GTid < primCount * 3)
			GenerateMeshlet(GTid, Gid, primCount, localIdxBase, uniqueIdxBase, vertCountIdx);
	}
	else
	{
		// Meshlet generated
		if (GTid < vertexCount)
		{
			// Vertex processing
			const uint vid = g_meshletIdxBuffer[uniqueIdxBase + GTid];
			const VSIn input = g_vertexBuffer[vid];
			verts[GTid] = VSMain(input, vid);
		}

		if (GTid < primCount)
		{
			// Index processing
			const uint prim = g_meshletIdxBuffer[localIdxBase + GTid];
			tris[GTid] = uint3(prim & 0x3FF, (prim >> 10) & 0x3FF, (prim >> 20) & 0x3FF);
		}
	}
}
