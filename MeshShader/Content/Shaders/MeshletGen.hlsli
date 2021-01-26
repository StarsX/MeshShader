//--------------------------------------------------------------------------------------
// Copyright (c) XU, Tianchen. All rights reserved.
//--------------------------------------------------------------------------------------

enum Meshlet
{
	VertCount,
	VertOffset,
	PrimCount,
	PrimOffset,

	MeshletEntryCount
};

//--------------------------------------------------------------------------------------
// Constant buffer
//--------------------------------------------------------------------------------------
cbuffer cbPerObject
{
	uint g_primCount;
};

//--------------------------------------------------------------------------------------
// Texture and buffers
//--------------------------------------------------------------------------------------
// IA buffers
Buffer<uint>			g_indexBuffer;
StructuredBuffer<VSIn>	g_vertexBuffer;

RWStructuredBuffer<uint> g_meshlets;
RWStructuredBuffer<uint> g_uniqueVertexIndices;
RWStructuredBuffer<uint> g_primitiveIndices;

void GenerateMeshlet(uint gTid, uint gid, uint m[MeshletEntryCount])
{
	const uint meshletIdx = gid * MeshletEntryCount;
	const uint indexIdx = m[VertOffset] + gTid;
	if (!gTid)
	{
		g_meshlets[meshletIdx + VertOffset] = m[VertOffset];
		g_meshlets[meshletIdx + PrimOffset] = m[PrimOffset];
		g_meshlets[meshletIdx + PrimCount] = m[PrimCount];
	}

	// Generate meshlet
	const uint idxBase = gid * MAX_VERTS;
	const uint vid = g_indexBuffer[idxBase + gTid];

	// Seek existing index
	for (uint i = 0; i < gTid; ++i)
		if (vid == g_indexBuffer[idxBase + i]) break;

	const bool isUnique = (i == gTid);
	uint appendIdx = 0;
	if (isUnique)
	{
		// Record the local index of the unique vertex thread
		InterlockedAdd(g_meshlets[meshletIdx + VertCount], 1, appendIdx);
		g_uniqueVertexIndices[indexIdx] = appendIdx;
	}

	DeviceMemoryBarrierWithGroupSync();

	// Broadcast the local index to the non-unique vertex thread
	if (!isUnique) g_uniqueVertexIndices[indexIdx] = g_uniqueVertexIndices[m[VertOffset] + i];

	DeviceMemoryBarrierWithGroupSync();

	// Endcode primitive local indices
	if (gTid < m[PrimCount])
	{
		const uint idxBase = m[VertOffset] + gTid * 3;
		uint3 tri;
		[unroll]
		for (uint n = 0; n < 3; ++n) tri[n] = g_uniqueVertexIndices[idxBase + n];
		g_primitiveIndices[m[PrimOffset] + gTid] = tri.x | (tri.y << 10) | (tri.z << 20);
	}

	DeviceMemoryBarrierWithGroupSync();

	// Ouput unique vertex index
	if (isUnique) g_uniqueVertexIndices[m[VertOffset] + appendIdx] = vid;
	//DeviceMemoryBarrierWithGroupSync();
}
