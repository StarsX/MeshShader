//--------------------------------------------------------------------------------------
// Copyright (c) XU, Tianchen. All rights reserved.
//--------------------------------------------------------------------------------------

#include "SharedConst.h"
#include "Optional/XUSGObjLoader.h"
#include "Renderer.h"
#include "D3D12MeshletGenerator.h"

#define GROUP_SIZE 96
#define ITER 100

using namespace std;
using namespace DirectX;
using namespace XUSG;

Renderer::Renderer(const Device& device) :
	m_device(device)
{
	m_shaderPool = ShaderPool::MakeUnique();
	m_graphicsPipelineCache = Graphics::PipelineCache::MakeUnique(device);
	m_computePipelineCache = Compute::PipelineCache::MakeUnique(device);
	m_meshShaderPipelineCache = MeshShader::PipelineCache::MakeUnique(device);
	m_pipelineLayoutCache = PipelineLayoutCache::MakeUnique(device);
	m_descriptorTableCache = DescriptorTableCache::MakeUnique(device, L"DescriptorTableCache");
}

Renderer::~Renderer()
{
}

bool Renderer::Init(CommandList* pCommandList, uint32_t width, uint32_t height, Format rtFormat,
	vector<Resource>& uploaders, const char* fileName, const XMFLOAT4& posScale, bool isMSSupported)
{
	m_viewport.x = static_cast<float>(width);
	m_viewport.y = static_cast<float>(height);
	m_posScale = posScale;

	// Load inputs
	ObjLoader objLoader;
	if (!objLoader.Import(fileName, true, true)) return false;
	N_RETURN(createVB(pCommandList, objLoader.GetNumVertices(), objLoader.GetVertexStride(), objLoader.GetVertices(), uploaders), false);
	N_RETURN(createIB(pCommandList, objLoader.GetNumIndices(), objLoader.GetIndices(), uploaders), false);
	N_RETURN(createMeshlets(pCommandList, objLoader.GetNumVertices(), objLoader.GetVertexStride(), objLoader.GetVertices(),
		objLoader.GetNumIndices(), objLoader.GetIndices(), uploaders), false);

	// Create a depth buffer
	m_depth = DepthStencil::MakeUnique();
	N_RETURN(m_depth->Create(m_device, width, height), false);

	// Create pipelines
	N_RETURN(createInputLayout(), false);
	N_RETURN(createPipelineLayouts(isMSSupported), false);
	N_RETURN(createPipelines(rtFormat, m_depth->GetFormat(), isMSSupported), false);
	N_RETURN(createDescriptorTables(), false);

	m_cbMatrices = ConstantBuffer::MakeUnique();
	N_RETURN(m_cbMatrices->Create(m_device, sizeof(CBMatrices), FrameCount, nullptr, MemoryType::UPLOAD, L"CBMatrices"), false);
	m_cbvMatStride = static_cast<uint32_t>(reinterpret_cast<uint8_t*>(m_cbMatrices->Map(1)) - reinterpret_cast<uint8_t*>(m_cbMatrices->Map()));

	m_cbPerFrame = ConstantBuffer::MakeUnique();
	N_RETURN(m_cbPerFrame->Create(m_device, sizeof(CBMatrices), FrameCount, nullptr, MemoryType::UPLOAD, L"CBPerFrame"), false);
	m_cbvPFStride = static_cast<uint32_t>(reinterpret_cast<uint8_t*>(m_cbPerFrame->Map(1)) - reinterpret_cast<uint8_t*>(m_cbPerFrame->Map()));

	return true;
}

void Renderer::UpdateFrame(uint32_t frameIndex, CXMMATRIX viewProj, const XMFLOAT3& eyePt)
{
	// General matrices
	//const auto world = XMMatrixScaling(m_bound.w, m_bound.w, m_bound.w) *
		//XMMatrixTranslation(m_bound.x, m_bound.y, m_bound.z);
	const auto world = XMMatrixScaling(m_posScale.w, m_posScale.w, m_posScale.w) *
		XMMatrixTranslation(m_posScale.x, m_posScale.y, m_posScale.z);
	const auto worldViewProj = world * viewProj;

	// Screen space matrices
	const auto toScreen = XMMATRIX
	(
		0.5f * m_viewport.x, 0.0f, 0.0f, 0.0f,
		0.0f, -0.5f * m_viewport.y, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f * m_viewport.x, 0.5f * m_viewport.y, 0.0f, 1.0f
	);
	const auto worldToScreen = viewProj * toScreen;
	const auto screenToWorld = XMMatrixInverse(nullptr, worldToScreen);

	// Constant buffers
	{
		const auto pCbData = reinterpret_cast<CBMatrices*>(m_cbMatrices->Map(frameIndex));
		XMStoreFloat4x4(&pCbData->WorldViewProj, XMMatrixTranspose(worldViewProj));
		XMStoreFloat3x4(&pCbData->World, world); // XMStoreFloat3x4 includes transpose.
		XMStoreFloat3x4(&pCbData->Normal, world); // XMStoreFloat3x4 includes transpose.
	}

	{
		const auto pCbData = reinterpret_cast<XMFLOAT3*>(m_cbPerFrame->Map(frameIndex));
		*pCbData = eyePt;
	}
}

void Renderer::Render(Ultimate::CommandList* pCommandList, uint32_t frameIndex,
	const Descriptor& rtv, PipelineType pipelineType)
{
	const DescriptorPool descriptorPools[] =
	{
		m_descriptorTableCache->GetDescriptorPool(CBV_SRV_UAV_POOL),
		m_descriptorTableCache->GetDescriptorPool(SAMPLER_POOL)
	};
	pCommandList->SetDescriptorPools(static_cast<uint32_t>(size(descriptorPools)), descriptorPools);

	// Clear depth
	pCommandList->ClearDepthStencilView(m_depth->GetDSV(), ClearFlag::DEPTH, 1.0f);

	// Set viewport
	Viewport viewport(0.0f, 0.0f, m_viewport.x, m_viewport.y);
	RectRange scissorRect(0, 0, static_cast<long>(m_viewport.x), static_cast<long>(m_viewport.y));
	pCommandList->RSSetViewports(1, &viewport);
	pCommandList->RSSetScissorRects(1, &scissorRect);

	pCommandList->OMSetRenderTargets(1, &rtv, &m_depth->GetDSV());

	switch (pipelineType)
	{
	case NAIVE_MS:
		renderMS(pCommandList, frameIndex);
		break;
	case PREGEN_MS:
		renderMeshlets(pCommandList, frameIndex);
		break;
	default:
		renderVS(pCommandList, frameIndex);
	}
}

bool Renderer::createVB(XUSG::CommandList* pCommandList, uint32_t numVerts,
	uint32_t stride, const uint8_t* pData, vector<Resource>& uploaders)
{
	auto& vertexBuffer = m_vertexBuffers[0];
	vertexBuffer = VertexBuffer::MakeUnique();
	N_RETURN(vertexBuffer->Create(m_device, numVerts, stride,
		ResourceFlag::NONE, MemoryType::DEFAULT), false);
	uploaders.push_back(nullptr);

	N_RETURN(vertexBuffer->Upload(pCommandList, uploaders.back(), pData, stride * numVerts, 0,
		ResourceState::VERTEX_AND_CONSTANT_BUFFER | ResourceState::NON_PIXEL_SHADER_RESOURCE), false);

	return true;
}

bool Renderer::createIB(XUSG::CommandList* pCommandList, uint32_t numIndices,
	const uint32_t* pData, vector<Resource>& uploaders)
{
	m_numIndices[0] = numIndices;
	const uint32_t byteWidth = sizeof(uint32_t) * numIndices;

	auto& indexBuffer = m_indexBuffers[0];
	indexBuffer = IndexBuffer::MakeUnique();
	N_RETURN(indexBuffer->Create(m_device, byteWidth, Format::R32_UINT,
		ResourceFlag::NONE, MemoryType::DEFAULT), false);
	uploaders.push_back(nullptr);

	N_RETURN(indexBuffer->Upload(pCommandList, uploaders.back(), pData, byteWidth, 0,
		ResourceState::INDEX_BUFFER | ResourceState::NON_PIXEL_SHADER_RESOURCE), false);

	{
		auto& meshletBuffer = m_meshletBuffers[0][NAIVE_MS];
		meshletBuffer = StructuredBuffer::MakeUnique();
		const auto numMeshlets = DIV_UP(numIndices, GROUP_SIZE);
		const uint32_t numMembers = sizeof(Meshlet) / sizeof(uint32_t);
		N_RETURN(meshletBuffer->Create(m_device, numMembers * numMeshlets, sizeof(uint32_t),
			ResourceFlag::ALLOW_UNORDERED_ACCESS, MemoryType::DEFAULT), false);
	}

	{
		auto& uniqueVertIndexBuffer = m_uniqueVertIndexBuffers[0][NAIVE_MS];
		uniqueVertIndexBuffer = StructuredBuffer::MakeUnique();
		N_RETURN(uniqueVertIndexBuffer->Create(m_device, numIndices, sizeof(uint32_t),
			ResourceFlag::ALLOW_UNORDERED_ACCESS, MemoryType::DEFAULT), false);
	}

	{
		auto& primitiveIndexBuffer = m_primitiveIndexBuffers[0][NAIVE_MS];
		primitiveIndexBuffer = StructuredBuffer::MakeUnique();
		N_RETURN(primitiveIndexBuffer->Create(m_device, numIndices / 3, sizeof(PackedTriangle),
			ResourceFlag::ALLOW_UNORDERED_ACCESS, MemoryType::DEFAULT), false);
	}

	return true;
}

bool Renderer::createMeshlets(XUSG::CommandList* pCommandList, uint32_t numVerts,
	uint32_t stride, const uint8_t* pVertData, uint32_t numIndices,
	const uint32_t* pIndexData, vector<Resource>& uploaders)
{
	vector<XMFLOAT3> positions(numVerts);
	for (auto i = 0u; i < numVerts; ++i)
		positions[i] = reinterpret_cast<const XMFLOAT3&>(pVertData[stride * i]);

	vector<Subset> meshletSubsets;
	vector<Meshlet> meshlets;
	vector<uint8_t> uniqueVertexIndices;
	vector<PackedTriangle> primitiveIndices;

	const auto maxVerts = 64u;
	const auto maxPrims = 126u;
	V_RETURN(ComputeMeshlets(maxVerts, maxPrims, pIndexData, numIndices, positions.data(), numVerts,
		meshletSubsets, meshlets, uniqueVertexIndices, primitiveIndices), cerr, false);

	{
		auto& meshletBuffer = m_meshletBuffers[0][PREGEN_MS];
		meshletBuffer = StructuredBuffer::MakeUnique();
		m_numMeshlets[0] = static_cast<uint32_t>(meshlets.size());
		N_RETURN(meshletBuffer->Create(m_device, m_numMeshlets[0], sizeof(Meshlet),
			ResourceFlag::NONE, MemoryType::DEFAULT), false);
		uploaders.push_back(nullptr);

		N_RETURN(meshletBuffer->Upload(pCommandList, uploaders.back(),
			meshlets.data(), sizeof(Meshlet) * meshlets.size(),
			0, ResourceState::NON_PIXEL_SHADER_RESOURCE), false);
	}

	{
		auto& uniqueVertIndexBuffer = m_uniqueVertIndexBuffers[0][PREGEN_MS];
		uniqueVertIndexBuffer = StructuredBuffer::MakeUnique();
		N_RETURN(uniqueVertIndexBuffer->Create(m_device,
			static_cast<uint32_t>(uniqueVertexIndices.size() / sizeof(uint32_t)),
			sizeof(uint32_t), ResourceFlag::NONE, MemoryType::DEFAULT), false);
		uploaders.push_back(nullptr);

		N_RETURN(uniqueVertIndexBuffer->Upload(pCommandList, uploaders.back(),
			uniqueVertexIndices.data(), uniqueVertexIndices.size(), 0,
			ResourceState::NON_PIXEL_SHADER_RESOURCE), false);
	}

	{
		auto& primitiveIndexBuffer = m_primitiveIndexBuffers[0][PREGEN_MS];
		primitiveIndexBuffer = StructuredBuffer::MakeUnique();
		N_RETURN(primitiveIndexBuffer->Create(m_device, static_cast<uint32_t>(primitiveIndices.size()),
			sizeof(PackedTriangle), ResourceFlag::NONE, MemoryType::DEFAULT), false);
		uploaders.push_back(nullptr);

		N_RETURN(primitiveIndexBuffer->Upload(pCommandList, uploaders.back(),
			primitiveIndices.data(), sizeof(PackedTriangle) * primitiveIndices.size(),
			0, ResourceState::NON_PIXEL_SHADER_RESOURCE), false);
	}

	return true;
}

bool Renderer::createInputLayout()
{
	// Define the vertex input layout.
	InputElementTable inputElementDescs =
	{
		{ "POSITION",	0, Format::R32G32B32_FLOAT, 0, 0,						InputClassification::PER_VERTEX_DATA, 0 },
		{ "NORMAL",		0, Format::R32G32B32_FLOAT, 0, APPEND_ALIGNED_ELEMENT,	InputClassification::PER_VERTEX_DATA, 0 }
	};

	X_RETURN(m_inputLayout, m_graphicsPipelineCache->CreateInputLayout(inputElementDescs), false);

	return true;
}

bool Renderer::createPipelineLayouts(bool isMSSupported)
{
	// Mesh-shader
	if (isMSSupported)
	{
		// Naive
		{
			// Get pipeline layout
			const auto pipelineLayout = Util::PipelineLayout::MakeUnique();
			pipelineLayout->SetRootCBV(CBV_MATRICES, 0, 0, DescriptorFlag::DATA_STATIC, Shader::MS);
			pipelineLayout->SetRootCBV(CBV_PER_FRAME, 0, 0, DescriptorFlag::DATA_STATIC, Shader::PS);
			pipelineLayout->SetRange(BUFFERS, DescriptorType::SRV, 2, 0, 0);
			pipelineLayout->SetRange(BUFFERS, DescriptorType::UAV, 3, 0);
			pipelineLayout->SetConstants(CONSTANTS, SizeOfInUint32(uint32_t), 1, 0, Shader::MS);
			pipelineLayout->SetShaderStage(BUFFERS, Shader::MS);
			X_RETURN(m_pipelineLayouts[BASEPASS_MS_LAYOUT], pipelineLayout->GetPipelineLayout(*m_pipelineLayoutCache,
				PipelineLayoutFlag::NONE, L"MSBasePassLayout"), false);
		}

		// Pre-generated
		{
			// Get pipeline layout
			const auto pipelineLayout = Util::PipelineLayout::MakeUnique();
			pipelineLayout->SetRootCBV(CBV_MATRICES, 0, 0, DescriptorFlag::DATA_STATIC, Shader::MS);
			pipelineLayout->SetRootCBV(CBV_PER_FRAME, 0, 0, DescriptorFlag::DATA_STATIC, Shader::PS);
			pipelineLayout->SetRange(BUFFERS, DescriptorType::SRV, 4, 0, 0);
			pipelineLayout->SetShaderStage(BUFFERS, Shader::MS);
			X_RETURN(m_pipelineLayouts[MESHLET_LAYOUT], pipelineLayout->GetPipelineLayout(*m_pipelineLayoutCache,
				PipelineLayoutFlag::NONE, L"MeshletLayout"), false);
		}
	}

	// Vertex-shader
	{
		// Get pipeline layout
		const auto pipelineLayout = Util::PipelineLayout::MakeUnique();
		pipelineLayout->SetRootCBV(CBV_MATRICES, 0, 0, DescriptorFlag::DATA_STATIC, Shader::VS);
		pipelineLayout->SetRootCBV(CBV_PER_FRAME, 0, 0, DescriptorFlag::DATA_STATIC, Shader::PS);
		X_RETURN(m_pipelineLayouts[BASEPASS_VS_LAYOUT], pipelineLayout->GetPipelineLayout(*m_pipelineLayoutCache,
			PipelineLayoutFlag::ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT, L"VSBasePassLayout"), false);
	}

	return true;
}

bool Renderer::createPipelines(Format rtFormat, Format dsFormat, bool isMSSupported)
{
	N_RETURN(m_shaderPool->CreateShader(Shader::Stage::PS, PS_SIMPLE, L"PSSimpleShade.cso"), false);

	// Mesh-shader
	if (isMSSupported)
	{
#if 1
		// Naive
		{
			N_RETURN(m_shaderPool->CreateShader(Shader::Stage::MS, MS_BASEPASS, L"MSBasePass.cso"), false);

			const auto state = MeshShader::State::MakeUnique();
			state->SetPipelineLayout(m_pipelineLayouts[BASEPASS_MS_LAYOUT]);
			state->SetShader(Shader::Stage::MS, m_shaderPool->GetShader(Shader::Stage::MS, MS_BASEPASS));
			state->SetShader(Shader::Stage::PS, m_shaderPool->GetShader(Shader::Stage::PS, PS_SIMPLE));
			state->OMSetNumRenderTargets(1);
			state->OMSetRTVFormat(0, rtFormat);
			state->OMSetDSVFormat(dsFormat);
			X_RETURN(m_pipelines[BASEPASS_MS], state->GetPipeline(*m_meshShaderPipelineCache, L"MeshShaderBasePass"), false);
		}
#endif

		// Pre-generated
		{
			N_RETURN(m_shaderPool->CreateShader(Shader::Stage::MS, MS_MESHLET, L"MSMeshlet.cso"), false);

			const auto state = MeshShader::State::MakeUnique();
			state->SetPipelineLayout(m_pipelineLayouts[MESHLET_LAYOUT]);
			state->SetShader(Shader::Stage::MS, m_shaderPool->GetShader(Shader::Stage::MS, MS_MESHLET));
			state->SetShader(Shader::Stage::PS, m_shaderPool->GetShader(Shader::Stage::PS, PS_SIMPLE));
			//state->RSSetState(MeshShader::RasterizerPreset::CULL_NONE, *m_meshShaderPipelineCache);
			state->OMSetNumRenderTargets(1);
			state->OMSetRTVFormat(0, rtFormat);
			state->OMSetDSVFormat(dsFormat);
			X_RETURN(m_pipelines[MESHLET], state->GetPipeline(*m_meshShaderPipelineCache, L"Meshlet"), false);
		}
	}

	// Vertex-shader
	{
		N_RETURN(m_shaderPool->CreateShader(Shader::Stage::VS, VS_BASEPASS, L"VSBasePass.cso"), false);
		
		const auto state = Graphics::State::MakeUnique();
		state->IASetInputLayout(m_inputLayout);
		state->SetPipelineLayout(m_pipelineLayouts[BASEPASS_VS_LAYOUT]);
		state->SetShader(Shader::Stage::VS, m_shaderPool->GetShader(Shader::Stage::VS, VS_BASEPASS));
		state->SetShader(Shader::Stage::PS, m_shaderPool->GetShader(Shader::Stage::PS, PS_SIMPLE));
		state->OMSetNumRenderTargets(1);
		state->OMSetRTVFormat(0, rtFormat);
		state->OMSetDSVFormat(dsFormat);
		X_RETURN(m_pipelines[BASEPASS_VS], state->GetPipeline(*m_graphicsPipelineCache, L"VertexShaderBasePass"), false);
	}

	return true;
}

bool Renderer::createDescriptorTables()
{
	// Index and vertex buffer SRVs, and meshlet-index UAVs
	for (auto i = 0u; i < NUM_MESH; ++i)
	{
		const Descriptor descriptors[] =
		{
			m_indexBuffers[i]->GetSRV(),
			m_vertexBuffers[i]->GetSRV(),
			//m_meshletIdxBuffers[i]->GetUAV()
			m_meshletBuffers[i][NAIVE_MS]->GetUAV(),
			m_uniqueVertIndexBuffers[i][NAIVE_MS]->GetUAV(),
			m_primitiveIndexBuffers[i][NAIVE_MS]->GetUAV()
		};
		const auto descriptorTable = Util::DescriptorTable::MakeUnique();
		descriptorTable->SetDescriptors(0, static_cast<uint32_t>(size(descriptors)), descriptors);
		X_RETURN(m_srvUavTables[i], descriptorTable->GetCbvSrvUavTable(*m_descriptorTableCache), false);
	}

	// Vertex buffer and meshlet-index SRVs
	for (auto i = 0u; i < NUM_MESH; ++i)
	{
		const Descriptor descriptors[] =
		{
			m_vertexBuffers[i]->GetSRV(),
			m_meshletBuffers[i][PREGEN_MS]->GetSRV(),
			m_uniqueVertIndexBuffers[i][PREGEN_MS]->GetSRV(),
			m_primitiveIndexBuffers[i][PREGEN_MS]->GetSRV()
		};
		const auto descriptorTable = Util::DescriptorTable::MakeUnique();
		descriptorTable->SetDescriptors(0, static_cast<uint32_t>(size(descriptors)), descriptors);
		X_RETURN(m_srvTables[i], descriptorTable->GetCbvSrvUavTable(*m_descriptorTableCache), false);
	}

	// Create the sampler table
	{
		const auto descriptorTable = Util::DescriptorTable::MakeUnique();
		const auto sampler = LINEAR_CLAMP;
		descriptorTable->SetSamplers(0, 1, &sampler, *m_descriptorTableCache);
		X_RETURN(m_samplerTable, descriptorTable->GetSamplerTable(*m_descriptorTableCache), false);
	}

	return true;
}

void Renderer::renderMS(Ultimate::CommandList* pCommandList, uint32_t frameIndex)
{
	// Set descriptor tables
	pCommandList->SetGraphicsPipelineLayout(m_pipelineLayouts[BASEPASS_MS_LAYOUT]);
	pCommandList->SetGraphicsRootConstantBufferView(CBV_MATRICES, m_cbMatrices->GetResource(), m_cbvMatStride * frameIndex);
	pCommandList->SetGraphicsRootConstantBufferView(CBV_PER_FRAME, m_cbPerFrame->GetResource(), m_cbvPFStride * frameIndex);

	// Set pipeline state
	pCommandList->SetPipelineState(m_pipelines[BASEPASS_MS]);

	// Record commands.
	for (auto k = 0u; k < ITER; ++k)
	{
		for (auto i = 0u; i < NUM_MESH; ++i)
		{
			pCommandList->SetGraphics32BitConstant(CONSTANTS, m_numIndices[i] / 3);
			pCommandList->SetGraphicsDescriptorTable(BUFFERS, m_srvUavTables[i]);
			pCommandList->DispatchMesh(DIV_UP(m_numIndices[i], GROUP_SIZE), 1, 1);
		}
	}
}

void Renderer::renderMeshlets(Ultimate::CommandList* pCommandList, uint32_t frameIndex)
{
	// Set descriptor tables
	pCommandList->SetGraphicsPipelineLayout(m_pipelineLayouts[MESHLET_LAYOUT]);
	pCommandList->SetGraphicsRootConstantBufferView(CBV_MATRICES, m_cbMatrices->GetResource(), m_cbvMatStride * frameIndex);
	pCommandList->SetGraphicsRootConstantBufferView(CBV_PER_FRAME, m_cbPerFrame->GetResource(), m_cbvPFStride * frameIndex);

	// Set pipeline state
	pCommandList->SetPipelineState(m_pipelines[MESHLET]);

	// Record commands.
	for (auto k = 0u; k < ITER; ++k)
	{
		for (auto i = 0u; i < NUM_MESH; ++i)
		{
			pCommandList->SetGraphicsDescriptorTable(BUFFERS, m_srvTables[i]);
			pCommandList->DispatchMesh(m_numMeshlets[i], 1, 1);
		}
	}
}

void Renderer::renderVS(CommandList* pCommandList, uint32_t frameIndex)
{
	// Set descriptor tables
	pCommandList->SetGraphicsPipelineLayout(m_pipelineLayouts[BASEPASS_VS_LAYOUT]);
	pCommandList->SetGraphicsRootConstantBufferView(CBV_MATRICES, m_cbMatrices->GetResource(), m_cbvMatStride * frameIndex);
	pCommandList->SetGraphicsRootConstantBufferView(CBV_PER_FRAME, m_cbPerFrame->GetResource(), m_cbvPFStride * frameIndex);

	// Set pipeline state
	pCommandList->SetPipelineState(m_pipelines[BASEPASS_VS]);

	// Record commands.
	pCommandList->IASetPrimitiveTopology(PrimitiveTopology::TRIANGLELIST);
	for (auto k = 0u; k < ITER; ++k)
	{
		for (auto i = 0u; i < NUM_MESH; ++i)
		{
			pCommandList->IASetVertexBuffers(0, 1, &m_vertexBuffers[i]->GetVBV());
			pCommandList->IASetIndexBuffer(m_indexBuffers[i]->GetIBV());
			pCommandList->DrawIndexed(m_numIndices[i], 1, 0, 0, 0);
		}
	}
}
