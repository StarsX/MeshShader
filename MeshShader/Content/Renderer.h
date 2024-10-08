//--------------------------------------------------------------------------------------
// Copyright (c) XU, Tianchen. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "Ultimate/XUSGUltimate.h"

class Renderer
{
public:
	enum PipelineType
	{
		NAIVE_MS,
		PREGEN_MS,
		LEGACY,

		PIPE_TYPE_COUNT,
		MS_PIPE_COUNT = LEGACY
	};

	Renderer();
	virtual ~Renderer();

	bool Init(XUSG::CommandList* pCommandList, const XUSG::DescriptorTableLib::sptr& descriptorTableLib,
		uint32_t width, uint32_t height, XUSG::Format rtFormat, std::vector<XUSG::Resource::uptr>& uploaders,
		const char* fileName, const DirectX::XMFLOAT4& posScale, bool isMSSupported);

	void UpdateFrame(uint8_t frameIndex, DirectX::CXMMATRIX viewProj, const DirectX::XMFLOAT3& eyePt);
	void Render(XUSG::Ultimate::CommandList* pCommandList, uint8_t frameIndex,
		const XUSG::Descriptor& rtv, PipelineType pipelineType);

	static const uint8_t FrameCount = 3;

protected:
	enum PipelineLayoutIndex : uint8_t
	{
		BASEPASS_MS_LAYOUT,
		BASEPASS_VS_LAYOUT,
		MESHLET_LAYOUT,

		NUM_PIPELINE_LAYOUT
	};

	enum PipelineLayoutSlot : uint8_t
	{
		CBV_MATRICES,
		BUFFERS,
		CONSTANTS
	};

	enum PipelineIndex : uint8_t
	{
		BASEPASS_MS,
		BASEPASS_VS,
		MESHLET,

		NUM_PIPELINE
	};

	enum SRVTable : uint8_t
	{
		SRV_TABLE_IB,
		SRV_TABLE_VB,

		NUM_SRV_TABLE
	};

	enum VertexShaderID : uint8_t
	{
		VS_BASEPASS
	};

	enum MeshShaderID : uint8_t
	{
		MS_BASEPASS,
		MS_MESHLET
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

	bool createVB(XUSG::CommandList* pCommandList, uint32_t numVerts,
		uint32_t stride, const uint8_t* pData, std::vector<XUSG::Resource::uptr>& uploaders);
	bool createIB(XUSG::CommandList* pCommandList, uint32_t numIndices,
		const uint32_t* pData, std::vector<XUSG::Resource::uptr>& uploaders);
	bool createMeshlets(XUSG::CommandList* pCommandList, uint32_t numVerts, uint32_t stride,
		const uint8_t* pVertData, uint32_t numIndices, const uint32_t* pIndexData,
		std::vector<XUSG::Resource::uptr>& uploaders);
	bool createInputLayout();
	bool createPipelineLayouts(bool isMSSupported);
	bool createPipelines(XUSG::Format rtFormat, XUSG::Format dsFormat, bool isMSSupported);
	bool createDescriptorTables();
	void renderMS(XUSG::Ultimate::CommandList* pCommandList, uint8_t frameIndex);
	void renderMeshlets(XUSG::Ultimate::CommandList* pCommandList, uint8_t frameIndex);
	void renderVS(XUSG::CommandList* pCommandList, uint8_t frameIndex);

	const static uint32_t NUM_MESH = 1;

	const XUSG::InputLayout*	m_pInputLayout;
	XUSG::PipelineLayout		m_pipelineLayouts[NUM_PIPELINE_LAYOUT];
	XUSG::Pipeline				m_pipelines[NUM_PIPELINE];

	XUSG::DescriptorTable		m_srvUavTables[NUM_MESH];
	XUSG::DescriptorTable		m_srvTables[NUM_MESH];

	XUSG::VertexBuffer::uptr	m_vertexBuffers[NUM_MESH];
	XUSG::IndexBuffer::uptr		m_indexBuffers[NUM_MESH];
	XUSG::StructuredBuffer::uptr m_meshletBuffers[NUM_MESH][MS_PIPE_COUNT];
	XUSG::StructuredBuffer::uptr m_uniqueVertIndexBuffers[NUM_MESH][MS_PIPE_COUNT];
	XUSG::StructuredBuffer::uptr m_primitiveIndexBuffers[NUM_MESH][MS_PIPE_COUNT];
	XUSG::DepthStencil::uptr	m_depth;

	XUSG::ConstantBuffer::uptr	m_cbMatrices;

	XUSG::ShaderLib::uptr				m_shaderLib;
	XUSG::Graphics::PipelineLib::uptr	m_graphicsPipelineLib;
	XUSG::Compute::PipelineLib::uptr	m_computePipelineLib;
	XUSG::Ultimate::PipelineLib::uptr	m_meshPipelineLib;
	XUSG::PipelineLayoutLib::uptr		m_pipelineLayoutLib;
	XUSG::DescriptorTableLib::sptr		m_descriptorTableLib;

	DirectX::XMFLOAT2	m_viewport;
	DirectX::XMFLOAT4	m_posScale;
	uint32_t			m_numIndices[NUM_MESH];
	uint32_t			m_numMeshlets[NUM_MESH];
};
