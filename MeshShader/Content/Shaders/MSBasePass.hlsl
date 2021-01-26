//--------------------------------------------------------------------------------------
// Copyright (c) XU, Tianchen. All rights reserved.
//--------------------------------------------------------------------------------------

#define MAX_PRIMS	32
#define GROUP_SIZE	96
#define MAX_VERTS	GROUP_SIZE

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
	out indices uint3 tris[MAX_PRIMS],
	out vertices VSOut verts[MAX_VERTS])
	//out primitives uint prims[MAX_PRIMS] : SV_PrimitiveID)
{
	uint m[MeshletEntryCount];
	const uint meshletIdx = Gid * MeshletEntryCount;
	m[VertCount] = g_meshlets[meshletIdx + VertCount];
	m[PrimCount] = g_meshlets[meshletIdx + PrimCount];

	SetMeshOutputCounts(m[VertCount], m[PrimCount]);

	if (m[VertCount] == 0)
	{
		m[PrimCount] = (Gid + 1) * MAX_PRIMS > g_primCount ? g_primCount % MAX_PRIMS : MAX_PRIMS;

		// Generate meshlet
		if (GTid < m[PrimCount] * 3)
		{
			m[PrimOffset] = MAX_PRIMS * Gid;
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
