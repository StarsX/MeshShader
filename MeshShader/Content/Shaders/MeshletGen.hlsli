//--------------------------------------------------------------------------------------
// Copyright (c) XU, Tianchen. All rights reserved.
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// Constant buffer
//--------------------------------------------------------------------------------------
cbuffer cbPerObject : register (b2)
{
	uint g_meshIdx;
	uint g_primCount;
};

//--------------------------------------------------------------------------------------
// Texture and buffers
//--------------------------------------------------------------------------------------
// IA buffers
Buffer<uint>			g_indexBuffers[]	: register(t0, space0);
StructuredBuffer<VSIn>	g_vertexBuffers[]	: register(t0, space1);
RWBuffer<uint>		g_meshletIdxBuffers[]	: register(u0);

void GenerateMeshlet(uint GTid, uint Gid, uint primCount, uint localIdxBase, uint uniqueIdxBase, uint vertCountIdx)
{
	// Generate meshlet
	const RWBuffer<uint> meshletIdxBuffer = g_meshletIdxBuffers[g_meshIdx];
	const Buffer<uint> indexBuffers = g_indexBuffers[g_meshIdx];
	const uint idxBase = Gid * MAX_VERT_COUNT;
	const uint vid = indexBuffers[idxBase + GTid];

	// Seek existing index
	for (uint i = 0; i < GTid; ++i)
		if (vid == indexBuffers[idxBase + i]) break;

	const bool isUnique = (i == GTid);
	uint appendIdx = 0;
	if (isUnique)
	{
		// Record the local index of the unique vertex thread
		InterlockedAdd(meshletIdxBuffer[vertCountIdx], 1, appendIdx);
		meshletIdxBuffer[uniqueIdxBase + GTid] = appendIdx;
	}

	DeviceMemoryBarrierWithGroupSync();

	// Broadcast the local index to the non-unique vertex thread
	if (!isUnique) meshletIdxBuffer[uniqueIdxBase + GTid] = meshletIdxBuffer[uniqueIdxBase + i];

	DeviceMemoryBarrierWithGroupSync();

	// Endcode primitive local indices
	if (GTid < primCount)
	{
		uint3 tri;
		[unroll]
		for (uint n = 0; n < 3; ++n)
			tri[n] = meshletIdxBuffer[uniqueIdxBase + GTid * 3 + n];
		meshletIdxBuffer[localIdxBase + GTid] = tri.x | (tri.y << 10) | (tri.z << 20);
	}

	DeviceMemoryBarrierWithGroupSync();

	// Ouput unique vertex index
	if (isUnique) meshletIdxBuffer[uniqueIdxBase + appendIdx] = vid;
	//DeviceMemoryBarrierWithGroupSync();
}
