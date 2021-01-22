//--------------------------------------------------------------------------------------
// Copyright (c) XU, Tianchen. All rights reserved.
//--------------------------------------------------------------------------------------

#define GROUP_SIZE		128
#define MAX_PRIM_COUNT	126
#define MAX_VERT_COUNT	64

#define main VSMain
#include "VSBasePass.hlsl"
#undef main

struct Meshlet
{
	uint VertCount;
	uint VertOffset;
	uint PrimCount;
	uint PrimOffset;
};

//--------------------------------------------------------------------------------------
// Texture and buffers
//--------------------------------------------------------------------------------------
// IA buffers
StructuredBuffer<VSIn>	g_vertexBuffer;
StructuredBuffer<Meshlet> g_meshlets;
StructuredBuffer<uint> g_uniqueVertexIndices;
StructuredBuffer<uint> g_primitiveIndices;

//--------------------------------------------------------------------------------------
// Meshlet processing
//--------------------------------------------------------------------------------------
[NumThreads(GROUP_SIZE, 1, 1)]
[OutputTopology("triangle")]
void main(
	uint GTid : SV_GroupThreadID,
	uint Gid : SV_GroupID,
	out indices uint3 tris[MAX_PRIM_COUNT],
	out vertices VSOut verts[MAX_VERT_COUNT])
{
	Meshlet m = g_meshlets[Gid];
	SetMeshOutputCounts(m.VertCount, m.PrimCount);

	if (GTid < m.VertCount)
	{
		// Vertex processing
		const uint vid = g_uniqueVertexIndices[m.VertOffset + GTid];
		const VSIn input = g_vertexBuffer[vid];
		verts[GTid] = VSMain(input, vid);
	}

	if (GTid < m.PrimCount)
	{
		// Index processing
		const uint prim = g_primitiveIndices[m.PrimOffset + GTid];
		tris[GTid] = uint3(prim & 0x3FF, (prim >> 10) & 0x3FF, (prim >> 20) & 0x3FF);
	}
}
