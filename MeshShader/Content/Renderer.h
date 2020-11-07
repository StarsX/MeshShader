//--------------------------------------------------------------------------------------
// Copyright (c) XU, Tianchen. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "Ultimate/XUSGUltimate.h"

class Renderer
{
public:
	Renderer(const XUSG::Device& device);
	virtual ~Renderer();

	bool Init(XUSG::CommandList* pCommandList, uint32_t width, uint32_t height,
		XUSG::Format rtFormat, std::vector<XUSG::Resource>& uploaders,
		const char* fileName, const DirectX::XMFLOAT4& posScale);

	void UpdateFrame(uint32_t frameIndex, DirectX::CXMMATRIX viewProj, const DirectX::XMFLOAT3& eyePt);
	void Render(XUSG::Ultimate::CommandList* pCommandList, uint32_t frameIndex, const XUSG::Descriptor& rtv);

	static const uint32_t FrameCount = 3;

protected:
	enum PipelineLayoutIndex : uint8_t
	{
		MESH_SHADER_LAYOUT,

		NUM_PIPELINE_LAYOUT
	};

	enum PipelineLayoutSlot : uint8_t
	{
		CBV_MATRICES,
		CBV_PER_FRAME,
		CONSTANTS,
		INDEX_BUFFER,
		VERTEX_BUFFER,
		UAV_BUFFER,
		SAMPLER
	};

	enum PipelineIndex : uint8_t
	{
		MESH_SHADER_BASE,

		NUM_PIPELINE
	};

	enum SRVTable : uint8_t
	{
		SRV_TABLE_IB,
		SRV_TABLE_VB,

		NUM_SRV_TABLE
	};

	enum MeshShaderID : uint8_t
	{
		MS_BASEPASS
	};

	enum PixelShaderID : uint8_t
	{
		PS_SIMPLE
	};

	struct CBMatrices
	{
		DirectX::XMFLOAT4X4	WorldViewProj;
		DirectX::XMFLOAT3X4	World;
		DirectX::XMFLOAT3X4	Normal;
	};

	bool createVB(XUSG::CommandList* pCommandList, uint32_t numVert,
		uint32_t stride, const uint8_t* pData, std::vector<XUSG::Resource>& uploaders);
	bool createIB(XUSG::CommandList* pCommandList, uint32_t numIndices,
		const uint32_t* pData, std::vector<XUSG::Resource>& uploaders);
	bool createPipelineLayouts();
	bool createPipelines(XUSG::Format rtFormat, XUSG::Format dsFormat);
	bool createDescriptorTables();

	const static uint32_t NUM_MESH = 1;

	XUSG::Device m_device;

	XUSG::PipelineLayout		m_pipelineLayouts[NUM_PIPELINE_LAYOUT];
	XUSG::Pipeline				m_pipelines[NUM_PIPELINE];

	XUSG::DescriptorTable		m_srvTables[NUM_SRV_TABLE];
	XUSG::DescriptorTable		m_srvUavTable;
	XUSG::DescriptorTable		m_uavTable;
	XUSG::DescriptorTable		m_samplerTable;

	XUSG::VertexBuffer::uptr	m_vertexBuffers[NUM_MESH];
	XUSG::IndexBuffer::uptr		m_indexBuffers[NUM_MESH];
	XUSG::TypedBuffer::uptr		m_meshletIdxBuffers[NUM_MESH];
	XUSG::DepthStencil::uptr	m_depth;

	XUSG::ConstantBuffer::uptr	m_cbMatrices;
	XUSG::ConstantBuffer::uptr	m_cbPerFrame;
	uint32_t					m_cbvMatStride;
	uint32_t					m_cbvPFStride;

	DirectX::XMFLOAT3			m_eyePt;
	DirectX::XMFLOAT4X4			m_world;
	DirectX::XMFLOAT4X4			m_worldViewProj;

	XUSG::ShaderPool::uptr					m_shaderPool;
	XUSG::Graphics::PipelineCache::uptr		m_graphicsPipelineCache;
	XUSG::Compute::PipelineCache::uptr		m_computePipelineCache;
	XUSG::MeshShader::PipelineCache::uptr	m_meshShaderPipelineCache;
	XUSG::PipelineLayoutCache::uptr			m_pipelineLayoutCache;
	XUSG::DescriptorTableCache::uptr		m_descriptorTableCache;

	DirectX::XMFLOAT2				m_viewport;
	DirectX::XMFLOAT4				m_bound;
	DirectX::XMFLOAT4				m_posScale;
	uint32_t						m_numIndices[NUM_MESH];
};
