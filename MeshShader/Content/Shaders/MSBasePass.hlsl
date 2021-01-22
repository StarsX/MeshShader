//--------------------------------------------------------------------------------------
// Copyright (c) XU, Tianchen. All rights reserved.
//--------------------------------------------------------------------------------------

#define MAX_PRIM_COUNT	32
#define GROUP_SIZE		96
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
	uint m[MeshletEntryCount];
	const uint meshletIdx = Gid * MeshletEntryCount;
	m[VertCount] = g_meshlets[meshletIdx + VertCount];
	m[PrimCount] = g_meshlets[meshletIdx + PrimCount];

	SetMeshOutputCounts(m[VertCount], m[PrimCount]);

	if (m[VertCount] == 0)
	{
		m[PrimCount] = (Gid + 1) * MAX_PRIM_COUNT > g_primCount ? g_primCount % MAX_PRIM_COUNT : MAX_PRIM_COUNT;

		// Generate meshlet
		if (GTid < m[PrimCount] * 3)
		{
			m[PrimOffset] = m[PrimCount] * Gid;
			m[VertOffset] = m[PrimOffset] * 3;
			GenerateMeshlet(GTid, Gid, m);
		}
	}
	else
	{
		// Meshlet generated
		if (GTid < m[VertCount])
		{
			// Vertex processing
			m[VertOffset] = g_meshlets[meshletIdx + VertOffset];
			const uint vid = g_uniqueVertexIndices[m[VertOffset] + GTid];
			const VSIn input = g_vertexBuffer[vid];
			verts[GTid] = VSMain(input, vid);
		}

		if (GTid < m[PrimCount])
		{
			// Index processing
			m[PrimOffset] = g_meshlets[meshletIdx + PrimOffset];
			const uint prim = g_primitiveIndices[m[PrimOffset] + GTid];
			tris[GTid] = uint3(prim & 0x3FF, (prim >> 10) & 0x3FF, (prim >> 20) & 0x3FF);
		}
	}
}
