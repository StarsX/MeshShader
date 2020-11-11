//--------------------------------------------------------------------------------------
// Copyright (c) XU, Tianchen. All rights reserved.
//--------------------------------------------------------------------------------------

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
RWBuffer<uint>		g_meshletIdxBuffer;

void GenerateMeshlet(uint GTid, uint Gid, uint primCount, uint localIdxBase, uint uniqueIdxBase, uint vertCountIdx)
{
	// Generate meshlet
	const uint idxBase = Gid * MAX_VERT_COUNT;
	const uint vid = g_indexBuffer[idxBase + GTid];

	// Seek existing index
	for (uint i = 0; i < GTid; ++i)
		if (vid == g_indexBuffer[idxBase + i]) break;

	const bool isUnique = (i == GTid);
	uint appendIdx = 0;
	if (isUnique)
	{
		// Record the local index of the unique vertex thread
		InterlockedAdd(g_meshletIdxBuffer[vertCountIdx], 1, appendIdx);
		g_meshletIdxBuffer[uniqueIdxBase + GTid] = appendIdx;
	}

	DeviceMemoryBarrierWithGroupSync();

	// Broadcast the local index to the non-unique vertex thread
	if (!isUnique) g_meshletIdxBuffer[uniqueIdxBase + GTid] = g_meshletIdxBuffer[uniqueIdxBase + i];

	DeviceMemoryBarrierWithGroupSync();

	// Endcode primitive local indices
	if (GTid < primCount)
	{
		uint3 tri;
		[unroll]
		for (uint n = 0; n < 3; ++n)
			tri[n] = g_meshletIdxBuffer[uniqueIdxBase + GTid * 3 + n];
		g_meshletIdxBuffer[localIdxBase + GTid] = tri.x | (tri.y << 10) | (tri.z << 20);
	}

	DeviceMemoryBarrierWithGroupSync();

	// Ouput unique vertex index
	if (isUnique) g_meshletIdxBuffer[uniqueIdxBase + appendIdx] = vid;
	//DeviceMemoryBarrierWithGroupSync();
}
