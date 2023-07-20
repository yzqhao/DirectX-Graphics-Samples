
#pragma once

#include "pch.h"
#include "GameCore.h"
#include "GraphicsCore.h"
#include "SystemTime.h"
#include "TextRenderer.h"
#include "GameInput.h"
#include "CommandContext.h"
#include "RootSignature.h"
#include "PipelineState.h"
#include "BufferManager.h"
#include "CameraController.h"
#include "Camera.h"
#include "TextureManager.h"
#include "UploadBuffer.h"
#include "../Common/Macros.h"
#include "../Common/Application.h"

#include <unordered_map>

struct ObjectConstants
{
	Math::Matrix4 World{};
	Math::Matrix4 PreWorld{};
	UINT     MaterialIndex;
	float roughness;
	float metalness;
	int pbrMaterials;
};

struct PointLight
{
	Math::Vector4  mPos;
	Math::Vector4  mCol;
	float mRadius;
	float mIntensity;
	char  _pad[8];
};

struct DirectionalLight
{
	Math::Vector4 mPos;
	Math::Vector4 mCol;    //alpha is the intesity
	Math::Vector4 mDir;
};

#define MaxLights 16

struct PassConstants
{
	Math::Matrix4 View{};
	Math::Matrix4 InvView{};
	Math::Matrix4 Proj{};
	Math::Matrix4 InvProj{};
	Math::Matrix4 ViewProj{};
	Math::Matrix4 PreViewProj{};
	Math::Matrix4 InvViewProj{};
	Math::Vector4 EyePosW;	// xyz 
	Math::Vector4 RenderTargetSize;	// xy : size, zw : InvRenderTargetSize
	float NearZ = 0.0f;
	float FarZ = 0.0f;
	float TotalTime = 0.0f;
	float DeltaTime = 0.0f;
	Math::Vector4 ViewPortSize;
	DirectX::XMFLOAT4 AmbientLight = { 0.0f, 0.0f, 0.0f, 1.0f };

	// Indices [0, NUM_DIR_LIGHTS) are directional lights;
	// indices [NUM_DIR_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHTS) are point lights;
	// indices [NUM_DIR_LIGHTS+NUM_POINT_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHT+NUM_SPOT_LIGHTS)
	// are spot lights for a maximum of MaxLights per object.
	DirectionalLight DLights[MaxLights];
	PointLight PLights[MaxLights];
	int amountOfDLights;
	int amountOfPLights;
};

struct HolepatchingConstants
{
	uint  renderMode;
	float useHolePatching;
	float useExpensiveHolePatching;
	float useNormalMap;		// no using

	float intensity;
	float useFadeEffect;	// no using
	float padding01;
	float padding02;
};

struct UniformSSSRConstantsData
{
	Math::Matrix4 g_inv_view_proj;
	Math::Matrix4 g_proj;
	Math::Matrix4 g_inv_proj;
	Math::Matrix4 g_view;
	Math::Matrix4 g_inv_view;
	Math::Matrix4 g_prev_view_proj;

	uint       g_frame_index = 0;
	uint       g_max_traversal_intersections = 0;
	uint       g_min_traversal_occupancy = 0;
	uint       g_most_detailed_mip = 0;
	float      g_temporal_stability_factor = 0;
	float      g_depth_buffer_thickness = 0;
	uint       g_samples_per_quad = 0;
	uint       g_temporal_variance_guided_tracing_enabled = 0;
	float      g_roughness_threshold = 0;
	uint       g_skip_denoiser = 0;
};

struct MaterialData
{
	UINT DiffuseMapIndex = 0;
};

struct Vertex
{
	Math::Vector3 Pos;
	Math::Vector3 Normal;
	Math::Vector2 TexC;
};

struct SubmeshGeometry
{
	uint IndexCount = 0;
	uint StartIndexLocation = 0;
	int BaseVertexLocation = 0;
};

enum
{
	SCENE_ONLY = 0,
	REFLECTIONS_ONLY = 1,
	SCENE_WITH_REFLECTIONS = 2,
	SCENE_EXCLU_REFLECTIONS = 3,
};

class ScreenSpaceReflections : public Application
{
public:

	ScreenSpaceReflections()
		: mGBufferDepth(1.0f)
	{
	}

	virtual void _DoStartup(void) override;
	virtual void Cleanup(void) override;

	virtual void Update(float deltaT) override;
	virtual void _DoRenderScene(void) override;

private:

	enum RootBindings
	{
		kMeshCBV,
		kCommonCBV,
		kSkyRTV,
		kIrradianceRTV,
		kSpecularRTV,
		kTextureSRVs,
		kMaterialSRV,

		kNumRootBindings
	};

	Camera m_Camera;
	std::unique_ptr<CameraController> m_CameraController;

	std::unordered_map<std::string, RootSignature> mRootSignatures;
	std::unordered_map<std::string, ComputePSO> mCompPSO;
	std::unordered_map<std::string, GraphicsPSO> mGraphicsPSO;
	std::unordered_map<std::string, TextureRef> mTextures;
	std::unordered_map<std::string, ByteAddressBuffer> mGeometryBuffers;
	std::unordered_map<std::string, D3D12_VERTEX_BUFFER_VIEW> mVertexViews;
	std::unordered_map<std::string, D3D12_INDEX_BUFFER_VIEW> mIndexViews;

	ColorBuffer mBRDFIntegration;
	ColorBuffer mSkybox;
	ColorBuffer mIrradianceMap;
	ColorBuffer mSpecularMap;

	ColorBuffer mGBufferA;	// base color
	ColorBuffer mGBufferB;	// normal + 
	ColorBuffer mGBufferC;	//
	ColorBuffer mGBufferD;	// 
	DepthBuffer mGBufferDepth;
	ColorBuffer mRenderSceneBrdf;	// render brdf
	ColorBuffer mRenderOutBuffer;	// render out buffer

	StructuredBuffer mMaterialConstants;

	DescriptorHeap mTextureHeap;
	DescriptorHandle mGameObjectTextures;

	std::unordered_map<std::string, SubmeshGeometry> mDrawSubMeshNames;
	std::vector<ObjectConstants> mObjectCbData;
	PassConstants mPassCbData;
	UniformSSSRConstantsData mCSUniformCbData;
	HolepatchingConstants mHolepatchingUniformData;
};