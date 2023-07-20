#include "ScreenSpaceReflections.h"
#include "Display.h"
#include "EsramAllocator.h"
#include "SystemTime.h"
#include "GpuTimeManager.h"

#include "CompiledShaders/BRDFIntegration.h"
#include "CompiledShaders/copyDepth.h"
#include "CompiledShaders/DepthDownsample.h"
#include "CompiledShaders/generateMips.h"
#include "CompiledShaders/IrradianceMap.h"
#include "CompiledShaders/panoToCube.h"
#include "CompiledShaders/SpecularMap.h"
#include "CompiledShaders/FillGbuffersVS.h"
#include "CompiledShaders/FillGbuffersPS.h"
#include "CompiledShaders/SkyVS.h"
#include "CompiledShaders/SkyPS.h"
#include "CompiledShaders/renderSceneBRDFVS.h"
#include "CompiledShaders/renderSceneBRDFPS.h"
#include "CompiledShaders/QuadVS.h"
#include "CompiledShaders/renderHolepatchingPS.h"
#include "CompiledShaders/FxaaPS.h"
#include "PostEffects.h"

#include "../Common/GltfLoader.h"

#include <codecvt>
#include <locale> 
#include "../Common/imgui/imgui.h"

using namespace GameCore;
using namespace Graphics;

static const int gBRDFIntegrationSize = 512;
static const uint32_t gSkyboxMips = 11;
static const uint32_t gIrradianceSize = 32;
static const uint32_t gSpecularSize = 128;
static const uint32_t gSpecularMips = 8;	// (uint)log2(gSpecularSize) + 1;
static const uint32_t gSkyboxSize = 1024;

static uint32_t gSSSR_MaxTravelsalIntersections = 128;
static uint32_t gSSSR_MinTravelsalOccupancy = 4;
static uint32_t gSSSR_MostDetailedMip = 1;
static float    pSSSR_TemporalStability = 0.99f;
static float    gSSSR_DepthThickness = 0.15f;
static int32_t  gSSSR_SamplesPerQuad = 1;
static int32_t  gSSSR_EAWPassCount = 1;
static bool     gSSSR_TemporalVarianceEnabled = true;
static float    gSSSR_RougnessThreshold = 0.1f;
static bool     gSSSR_SkipDenoiser = false;

uint32_t gMaterialIds[] = {
	0,  3,  1,  4,  5,  6,  7,  8,  6,  9,  7,  6, 10, 5, 7,  5, 6, 7,  6, 7,  6,  7,  6,  7,  6,  7,  6,  7,  6,  7,  6,  7,  6,  7,  6,
	5,  6,  5,  11, 5,  11, 5,  11, 5,  10, 5,  9, 8,  6, 12, 2, 5, 13, 0, 14, 15, 16, 14, 15, 14, 16, 15, 13, 17, 18, 19, 18, 19, 18, 17,
	19, 18, 17, 20, 21, 20, 21, 20, 21, 20, 21, 3, 1,  3, 1,  3, 1, 3,  1, 3,  1,  3,  1,  3,  1,  22, 23, 4,  23, 4,  5,  24, 5,
};

const char* pMaterialImageFileNames[] = {
	"SponzaPBR_Textures/ao",
	"SponzaPBR_Textures/ao",
	"SponzaPBR_Textures/ao",
	"SponzaPBR_Textures/ao",
	"SponzaPBR_Textures/ao",

	//common
	"SponzaPBR_Textures/ao",
	"SponzaPBR_Textures/Dielectric_metallic",
	"SponzaPBR_Textures/Metallic_metallic",
	"SponzaPBR_Textures/gi_flag",

	//Background
	"SponzaPBR_Textures/Background/Background_Albedo",
	"SponzaPBR_Textures/Background/Background_Normal",
	"SponzaPBR_Textures/Background/Background_Roughness",

	//ChainTexture
	"SponzaPBR_Textures/ChainTexture/ChainTexture_Albedo",
	"SponzaPBR_Textures/ChainTexture/ChainTexture_Metallic",
	"SponzaPBR_Textures/ChainTexture/ChainTexture_Normal",
	"SponzaPBR_Textures/ChainTexture/ChainTexture_Roughness",

	//Lion
	"SponzaPBR_Textures/Lion/Lion_Albedo",
	"SponzaPBR_Textures/Lion/Lion_Normal",
	"SponzaPBR_Textures/Lion/Lion_Roughness",

	//Sponza_Arch
	"SponzaPBR_Textures/Sponza_Arch/Sponza_Arch_diffuse",
	"SponzaPBR_Textures/Sponza_Arch/Sponza_Arch_normal",
	"SponzaPBR_Textures/Sponza_Arch/Sponza_Arch_roughness",

	//Sponza_Bricks
	"SponzaPBR_Textures/Sponza_Bricks/Sponza_Bricks_a_Albedo",
	"SponzaPBR_Textures/Sponza_Bricks/Sponza_Bricks_a_Normal",
	"SponzaPBR_Textures/Sponza_Bricks/Sponza_Bricks_a_Roughness",

	//Sponza_Ceiling
	"SponzaPBR_Textures/Sponza_Ceiling/Sponza_Ceiling_diffuse",
	"SponzaPBR_Textures/Sponza_Ceiling/Sponza_Ceiling_normal",
	"SponzaPBR_Textures/Sponza_Ceiling/Sponza_Ceiling_roughness",

	//Sponza_Column
	"SponzaPBR_Textures/Sponza_Column/Sponza_Column_a_diffuse",
	"SponzaPBR_Textures/Sponza_Column/Sponza_Column_a_normal",
	"SponzaPBR_Textures/Sponza_Column/Sponza_Column_a_roughness",

	"SponzaPBR_Textures/Sponza_Column/Sponza_Column_b_diffuse",
	"SponzaPBR_Textures/Sponza_Column/Sponza_Column_b_normal",
	"SponzaPBR_Textures/Sponza_Column/Sponza_Column_b_roughness",

	"SponzaPBR_Textures/Sponza_Column/Sponza_Column_c_diffuse",
	"SponzaPBR_Textures/Sponza_Column/Sponza_Column_c_normal",
	"SponzaPBR_Textures/Sponza_Column/Sponza_Column_c_roughness",

	//Sponza_Curtain
	"SponzaPBR_Textures/Sponza_Curtain/Sponza_Curtain_Blue_diffuse",
	"SponzaPBR_Textures/Sponza_Curtain/Sponza_Curtain_Blue_normal",

	"SponzaPBR_Textures/Sponza_Curtain/Sponza_Curtain_Green_diffuse",
	"SponzaPBR_Textures/Sponza_Curtain/Sponza_Curtain_Green_normal",

	"SponzaPBR_Textures/Sponza_Curtain/Sponza_Curtain_Red_diffuse",
	"SponzaPBR_Textures/Sponza_Curtain/Sponza_Curtain_Red_normal",

	"SponzaPBR_Textures/Sponza_Curtain/Sponza_Curtain_metallic",
	"SponzaPBR_Textures/Sponza_Curtain/Sponza_Curtain_roughness",

	//Sponza_Details
	"SponzaPBR_Textures/Sponza_Details/Sponza_Details_diffuse",
	"SponzaPBR_Textures/Sponza_Details/Sponza_Details_metallic",
	"SponzaPBR_Textures/Sponza_Details/Sponza_Details_normal",
	"SponzaPBR_Textures/Sponza_Details/Sponza_Details_roughness",

	//Sponza_Fabric
	"SponzaPBR_Textures/Sponza_Fabric/Sponza_Fabric_Blue_diffuse",
	"SponzaPBR_Textures/Sponza_Fabric/Sponza_Fabric_Blue_normal",

	"SponzaPBR_Textures/Sponza_Fabric/Sponza_Fabric_Green_diffuse",
	"SponzaPBR_Textures/Sponza_Fabric/Sponza_Fabric_Green_normal",

	"SponzaPBR_Textures/Sponza_Fabric/Sponza_Fabric_metallic",
	"SponzaPBR_Textures/Sponza_Fabric/Sponza_Fabric_roughness",

	"SponzaPBR_Textures/Sponza_Fabric/Sponza_Fabric_Red_diffuse",
	"SponzaPBR_Textures/Sponza_Fabric/Sponza_Fabric_Red_normal",

	//Sponza_FlagPole
	"SponzaPBR_Textures/Sponza_FlagPole/Sponza_FlagPole_diffuse",
	"SponzaPBR_Textures/Sponza_FlagPole/Sponza_FlagPole_normal",
	"SponzaPBR_Textures/Sponza_FlagPole/Sponza_FlagPole_roughness",

	//Sponza_Floor
	"SponzaPBR_Textures/Sponza_Floor/Sponza_Floor_diffuse",
	"SponzaPBR_Textures/Sponza_Floor/Sponza_Floor_normal",
	"SponzaPBR_Textures/Sponza_Floor/Sponza_Floor_roughness",

	//Sponza_Roof
	"SponzaPBR_Textures/Sponza_Roof/Sponza_Roof_diffuse",
	"SponzaPBR_Textures/Sponza_Roof/Sponza_Roof_normal",
	"SponzaPBR_Textures/Sponza_Roof/Sponza_Roof_roughness",

	//Sponza_Thorn
	"SponzaPBR_Textures/Sponza_Thorn/Sponza_Thorn_diffuse",
	"SponzaPBR_Textures/Sponza_Thorn/Sponza_Thorn_normal",
	"SponzaPBR_Textures/Sponza_Thorn/Sponza_Thorn_roughness",

	//Vase
	"SponzaPBR_Textures/Vase/Vase_diffuse",
	"SponzaPBR_Textures/Vase/Vase_normal",
	"SponzaPBR_Textures/Vase/Vase_roughness",

	//VaseHanging
	"SponzaPBR_Textures/VaseHanging/VaseHanging_diffuse",
	"SponzaPBR_Textures/VaseHanging/VaseHanging_normal",
	"SponzaPBR_Textures/VaseHanging/VaseHanging_roughness",

	//VasePlant
	"SponzaPBR_Textures/VasePlant/VasePlant_diffuse",
	"SponzaPBR_Textures/VasePlant/VasePlant_normal",
	"SponzaPBR_Textures/VasePlant/VasePlant_roughness",

	//VaseRound
	"SponzaPBR_Textures/VaseRound/VaseRound_diffuse",
	"SponzaPBR_Textures/VaseRound/VaseRound_normal",
	"SponzaPBR_Textures/VaseRound/VaseRound_roughness",

	"lion/lion_albedo",
	"lion/lion_specular",
	"lion/lion_normal",

};

static std::vector<uint> gSponzaTextureIndexforMaterial;

static void assignSponzaTextures()
{
	int AO = 5;
	int NoMetallic = 6;

	//00 : leaf
	gSponzaTextureIndexforMaterial.push_back(66);
	gSponzaTextureIndexforMaterial.push_back(67);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(68);
	gSponzaTextureIndexforMaterial.push_back(AO);

	//01 : vase_round
	gSponzaTextureIndexforMaterial.push_back(78);
	gSponzaTextureIndexforMaterial.push_back(79);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(80);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 02 : 16___Default (gi_flag)
	gSponzaTextureIndexforMaterial.push_back(8);
	gSponzaTextureIndexforMaterial.push_back(8);    // !!!!!!
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(8);    // !!!!!
	gSponzaTextureIndexforMaterial.push_back(AO);

	//03 : Material__57 (Plant)
	gSponzaTextureIndexforMaterial.push_back(75);
	gSponzaTextureIndexforMaterial.push_back(76);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(77);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 04 : Material__298
	gSponzaTextureIndexforMaterial.push_back(9);
	gSponzaTextureIndexforMaterial.push_back(10);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(11);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 05 : bricks
	gSponzaTextureIndexforMaterial.push_back(22);
	gSponzaTextureIndexforMaterial.push_back(23);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(24);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 06 :  arch
	gSponzaTextureIndexforMaterial.push_back(19);
	gSponzaTextureIndexforMaterial.push_back(20);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(21);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 07 : ceiling
	gSponzaTextureIndexforMaterial.push_back(25);
	gSponzaTextureIndexforMaterial.push_back(26);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(27);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 08 : column_a
	gSponzaTextureIndexforMaterial.push_back(28);
	gSponzaTextureIndexforMaterial.push_back(29);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(30);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 09 : Floor
	gSponzaTextureIndexforMaterial.push_back(60);
	gSponzaTextureIndexforMaterial.push_back(61);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(6);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 10 : column_c
	gSponzaTextureIndexforMaterial.push_back(34);
	gSponzaTextureIndexforMaterial.push_back(35);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(36);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 11 : details
	gSponzaTextureIndexforMaterial.push_back(45);
	gSponzaTextureIndexforMaterial.push_back(47);
	gSponzaTextureIndexforMaterial.push_back(46);
	gSponzaTextureIndexforMaterial.push_back(48);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 12 : column_b
	gSponzaTextureIndexforMaterial.push_back(31);
	gSponzaTextureIndexforMaterial.push_back(32);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(33);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 13 : flagpole
	gSponzaTextureIndexforMaterial.push_back(57);
	gSponzaTextureIndexforMaterial.push_back(58);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(59);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 14 : fabric_e (green)
	gSponzaTextureIndexforMaterial.push_back(51);
	gSponzaTextureIndexforMaterial.push_back(52);
	gSponzaTextureIndexforMaterial.push_back(53);
	gSponzaTextureIndexforMaterial.push_back(54);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 15 : fabric_d (blue)
	gSponzaTextureIndexforMaterial.push_back(49);
	gSponzaTextureIndexforMaterial.push_back(50);
	gSponzaTextureIndexforMaterial.push_back(53);
	gSponzaTextureIndexforMaterial.push_back(54);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 16 : fabric_a (red)
	gSponzaTextureIndexforMaterial.push_back(55);
	gSponzaTextureIndexforMaterial.push_back(56);
	gSponzaTextureIndexforMaterial.push_back(53);
	gSponzaTextureIndexforMaterial.push_back(54);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 17 : fabric_g (curtain_blue)
	gSponzaTextureIndexforMaterial.push_back(37);
	gSponzaTextureIndexforMaterial.push_back(38);
	gSponzaTextureIndexforMaterial.push_back(43);
	gSponzaTextureIndexforMaterial.push_back(44);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 18 : fabric_c (curtain_red)
	gSponzaTextureIndexforMaterial.push_back(41);
	gSponzaTextureIndexforMaterial.push_back(42);
	gSponzaTextureIndexforMaterial.push_back(43);
	gSponzaTextureIndexforMaterial.push_back(44);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 19 : fabric_f (curtain_green)
	gSponzaTextureIndexforMaterial.push_back(39);
	gSponzaTextureIndexforMaterial.push_back(40);
	gSponzaTextureIndexforMaterial.push_back(43);
	gSponzaTextureIndexforMaterial.push_back(44);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 20 : chain
	gSponzaTextureIndexforMaterial.push_back(12);
	gSponzaTextureIndexforMaterial.push_back(14);
	gSponzaTextureIndexforMaterial.push_back(13);
	gSponzaTextureIndexforMaterial.push_back(15);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 21 : vase_hanging
	gSponzaTextureIndexforMaterial.push_back(72);
	gSponzaTextureIndexforMaterial.push_back(73);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(74);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 22 : vase
	gSponzaTextureIndexforMaterial.push_back(69);
	gSponzaTextureIndexforMaterial.push_back(70);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(71);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 23 : Material__25 (lion)
	gSponzaTextureIndexforMaterial.push_back(16);
	gSponzaTextureIndexforMaterial.push_back(17);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(18);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 24 : roof
	gSponzaTextureIndexforMaterial.push_back(63);
	gSponzaTextureIndexforMaterial.push_back(64);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(65);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 25 : Material__47 - it seems missing
	gSponzaTextureIndexforMaterial.push_back(19);
	gSponzaTextureIndexforMaterial.push_back(20);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(21);
	gSponzaTextureIndexforMaterial.push_back(AO);
}

static std::wstring stringToWstring(const char* utf8Bytes)
{
	//setup converter
	using convert_type = std::codecvt_utf8<typename std::wstring::value_type>;
	std::wstring_convert<convert_type, typename std::wstring::value_type> converter;

	//use converter (.to_bytes: wstr->str, .from_bytes: str->wstr)
	return converter.from_bytes(utf8Bytes);
}

void ScreenSpaceReflections::_DoStartup(void)
{
	PostEffects::EnablePostEffects = false;

	{
		GraphicsContext& InitContext = GraphicsContext::Begin();

		EsramAllocator esram;

		esram.PushStack();

			mBRDFIntegration.Create(L"BRDF Integration Buffer", gBRDFIntegrationSize, gBRDFIntegrationSize, 1, DXGI_FORMAT_R32G32_FLOAT, esram);
			mSkybox.CreateTextureCube(L"Sky Box Buffer", gSkyboxSize, gSkyboxSize, gSkyboxMips, DXGI_FORMAT_R32G32B32A32_FLOAT, esram);
			mIrradianceMap.CreateTextureCube(L"Irradiance Map Buffer", gIrradianceSize, gIrradianceSize, 1, DXGI_FORMAT_R32G32B32A32_FLOAT, esram);
			mSpecularMap.CreateTextureCube(L"Specular Map Buffer", gSpecularSize, gSpecularSize, gSpecularMips, DXGI_FORMAT_R32G32B32A32_FLOAT, esram);

			mMaterialConstants.Create(L"ParticleEffectManager::SpriteVertexBuffer", 1024, sizeof(MaterialData));

			mGBufferA.Create(L"G Buffer A", g_DisplayWidth, g_DisplayHeight, 1, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, esram);
			mGBufferB.Create(L"G Buffer B", g_DisplayWidth, g_DisplayHeight, 1, DXGI_FORMAT_R16G16B16A16_FLOAT, esram);
			mGBufferC.Create(L"G Buffer C", g_DisplayWidth, g_DisplayHeight, 1, DXGI_FORMAT_R16G16B16A16_FLOAT, esram);
			mGBufferD.Create(L"G Buffer D", g_DisplayWidth, g_DisplayHeight, 1, DXGI_FORMAT_R16G16_FLOAT, esram);
			mGBufferDepth.Create(L"Depth Buffer A", g_DisplayWidth, g_DisplayHeight, 1, DXGI_FORMAT_D32_FLOAT, esram);
			mRenderSceneBrdf.Create(L"BRDF Buffer", g_DisplayWidth, g_DisplayHeight, 1, DXGI_FORMAT_R16G16B16A16_FLOAT, esram);
			mRenderOutBuffer.Create(L"Out Buffer", g_DisplayWidth, g_DisplayHeight, 1, DXGI_FORMAT_R16G16B16A16_FLOAT, esram);

		esram.PopStack();

		InitContext.Finish();
	}

	{
		mTextures["LA_Helipad"] = TextureManager::LoadDDSFromFile(L"Textures/LA_Helipad.dds", kWhiteOpaque2D, false);

		for (int i = 0; i < 84; ++i)
		{
			auto texMap = std::make_unique<Texture>();
			std::wstring Filename = L"Textures/" + stringToWstring(pMaterialImageFileNames[i]) + L".dds";

			bool srgb = false;
			if (strstr(pMaterialImageFileNames[i], "Albedo") || strstr(pMaterialImageFileNames[i], "diffuse"))
			{
				srgb = true;
			}

			mTextures[std::to_string(i)] = TextureManager::LoadDDSFromFile(Filename.c_str(), kWhiteOpaque2D, srgb);
		}
	}

	{
		const uint32_t DestCount = 84 + 7;
		std::vector<uint32_t> SourceCounts(DestCount, 1);
		mTextureHeap.Create(L"Scene Texture Descriptors", D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 4096);
		mGameObjectTextures = mTextureHeap.Alloc(DestCount);
		D3D12_CPU_DESCRIPTOR_HANDLE SourceTextures[DestCount] = {};
		for (int i = 0; i < 84; ++i)
		{
			SourceTextures[i] = mTextures[std::to_string(i)].GetSRV();
		}
		int idx = 84;
		SourceTextures[idx++] = mBRDFIntegration.GetSRV();
		SourceTextures[idx++] = mGBufferA.GetSRV();
		SourceTextures[idx++] = mGBufferB.GetSRV();
		SourceTextures[idx++] = mGBufferC.GetSRV();
		SourceTextures[idx++] = mGBufferD.GetSRV();
		SourceTextures[idx++] = mGBufferDepth.GetDepthSRV();
		SourceTextures[idx++] = mRenderSceneBrdf.GetSRV();
		g_Device->CopyDescriptors(1, &mGameObjectTextures, &DestCount, DestCount, SourceTextures, SourceCounts.data(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}

#define CreatePSO( ObjName, RootSignature, ShaderByteCode ) \
    ObjName.SetRootSignature(RootSignature); \
    ObjName.SetComputeShader(ShaderByteCode, sizeof(ShaderByteCode) ); \
    ObjName.Finalize();

	{	// BRDFIntegration
		RootSignature& RootSignature = mRootSignatures["BRDFIntegration"];
		RootSignature.Reset(1);
		RootSignature[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1);
		RootSignature.Finalize(L"BRDFIntegration");

		ComputePSO& PSO = mCompPSO["BRDFIntegration"];
		CreatePSO(PSO, RootSignature, g_pBRDFIntegration);
	}

	{	// panoToCube
		RootSignature& RootSignature = mRootSignatures["panoToCube"];
		RootSignature.Reset(3, 6);
		RootSignature.InitStaticSampler(0, SamplerPointWrapDesc);
		RootSignature.InitStaticSampler(1, SamplerPointClampDesc);
		RootSignature.InitStaticSampler(2, SamplerLinearWrapDesc);
		RootSignature.InitStaticSampler(3, SamplerLinearClampDesc);
		RootSignature.InitStaticSampler(4, SamplerAnisoWrapDesc);
		RootSignature.InitStaticSampler(5, SamplerAnisoClampDesc);
		RootSignature[0].InitAsConstants(0, 2);
		RootSignature[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1);
		RootSignature[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 11);
		RootSignature.Finalize(L"panoToCube");

		ComputePSO& PSO = mCompPSO["panoToCube"];
		CreatePSO(PSO, RootSignature, g_ppanoToCube);
	}

	{	// IrradianceMap
		RootSignature& RootSignature = mRootSignatures["IrradianceMap"];
		RootSignature.Reset(3, 6);
		RootSignature.InitStaticSampler(0, SamplerPointWrapDesc);
		RootSignature.InitStaticSampler(1, SamplerPointClampDesc);
		RootSignature.InitStaticSampler(2, SamplerLinearWrapDesc);
		RootSignature.InitStaticSampler(3, SamplerLinearClampDesc);
		RootSignature.InitStaticSampler(4, SamplerAnisoWrapDesc);
		RootSignature.InitStaticSampler(5, SamplerAnisoClampDesc);
		RootSignature[0].InitAsConstants(0, 2);
		RootSignature[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1);
		RootSignature[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1);
		RootSignature.Finalize(L"IrradianceMap");

		ComputePSO& PSO = mCompPSO["IrradianceMap"];
		CreatePSO(PSO, RootSignature, g_pIrradianceMap);
	}

	{	// SpecularMap
		RootSignature& RootSignature = mRootSignatures["SpecularMap"];
		RootSignature.Reset(3, 6);
		RootSignature.InitStaticSampler(0, SamplerPointWrapDesc);
		RootSignature.InitStaticSampler(1, SamplerPointClampDesc);
		RootSignature.InitStaticSampler(2, SamplerLinearWrapDesc);
		RootSignature.InitStaticSampler(3, SamplerLinearClampDesc);
		RootSignature.InitStaticSampler(4, SamplerAnisoWrapDesc);
		RootSignature.InitStaticSampler(5, SamplerAnisoClampDesc);
		RootSignature[0].InitAsConstants(0, 2);
		RootSignature[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1);
		RootSignature[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 11);
		RootSignature.Finalize(L"SpecularMap");

		ComputePSO& PSO = mCompPSO["SpecularMap"];
		CreatePSO(PSO, RootSignature, g_pSpecularMap);
	}

	{	// graphic 
		D3D12_INPUT_ELEMENT_DESC posNormalUV[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};
		D3D12_INPUT_ELEMENT_DESC posOnly[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};
		D3D12_INPUT_ELEMENT_DESC posAndUV[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		RootSignature& RootSignature = mRootSignatures["FillGbuffers"];
		RootSignature.Reset(7, 6);
		RootSignature.InitStaticSampler(0, SamplerPointWrapDesc);
		RootSignature.InitStaticSampler(1, SamplerPointClampDesc);
		RootSignature.InitStaticSampler(2, SamplerLinearWrapDesc);
		RootSignature.InitStaticSampler(3, SamplerLinearClampDesc);
		RootSignature.InitStaticSampler(4, SamplerAnisoWrapDesc);
		RootSignature.InitStaticSampler(5, SamplerAnisoClampDesc);
		RootSignature[0].InitAsConstantBuffer(0);
		RootSignature[1].InitAsConstantBuffer(1);
		RootSignature[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1);
		RootSignature[3].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
		RootSignature[4].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 1);
		RootSignature[5].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 200);
		RootSignature[6].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1, D3D12_SHADER_VISIBILITY_ALL, 1);
		RootSignature.Finalize(L"FillGbuffers", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		D3D12_DEPTH_STENCIL_DESC DepthState = DepthStateReadWrite;
		DepthState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

		D3D12_RASTERIZER_DESC RasterizerGBuffer = RasterizerTwoSided;
		RasterizerGBuffer.FrontCounterClockwise = FALSE;

		DXGI_FORMAT ColorFormats[4] = {mGBufferA.GetFormat(), mGBufferB.GetFormat(), mGBufferC.GetFormat(), mGBufferD.GetFormat()};
		GraphicsPSO& gbufferPSO = mGraphicsPSO["FillGbuffers"];
		gbufferPSO.SetRootSignature(RootSignature);
		gbufferPSO.SetRasterizerState(RasterizerGBuffer);
		gbufferPSO.SetBlendState(CD3DX12_BLEND_DESC(D3D12_DEFAULT));
		gbufferPSO.SetDepthStencilState(DepthState);
		gbufferPSO.SetInputLayout(_countof(posNormalUV), posNormalUV);
		gbufferPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
		gbufferPSO.SetRenderTargetFormats(4, ColorFormats, mGBufferDepth.GetFormat());
		gbufferPSO.SetVertexShader(g_pFillGbuffersVS, sizeof(g_pFillGbuffersVS));
		gbufferPSO.SetPixelShader(g_pFillGbuffersPS, sizeof(g_pFillGbuffersPS));
		gbufferPSO.Finalize();

		GraphicsPSO& SkyboxPSO = mGraphicsPSO["Skybox"];
		SkyboxPSO = gbufferPSO;
		DepthState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		SkyboxPSO.SetDepthStencilState(DepthState);
		SkyboxPSO.SetInputLayout(_countof(posOnly), posOnly);
		SkyboxPSO.SetVertexShader(g_pSkyVS, sizeof(g_pSkyVS));
		SkyboxPSO.SetPixelShader(g_pSkyPS, sizeof(g_pSkyPS));
		SkyboxPSO.Finalize();

		GraphicsPSO& renderSceneBRDFPSO = mGraphicsPSO["renderSceneBRDF"];
		renderSceneBRDFPSO = gbufferPSO;
		renderSceneBRDFPSO.SetDepthStencilState(DepthStateDisabled);
		renderSceneBRDFPSO.SetInputLayout(_countof(posAndUV), posAndUV);
		renderSceneBRDFPSO.SetVertexShader(g_prenderSceneBRDFVS, sizeof(g_prenderSceneBRDFVS));
		renderSceneBRDFPSO.SetPixelShader(g_prenderSceneBRDFPS, sizeof(g_prenderSceneBRDFPS));
		renderSceneBRDFPSO.SetRenderTargetFormats(1, &mRenderSceneBrdf.GetFormat(), DXGI_FORMAT_UNKNOWN);
		renderSceneBRDFPSO.Finalize();

		GraphicsPSO& renderHolepatching = mGraphicsPSO["renderHolepatching"];
		renderHolepatching = gbufferPSO;
		renderHolepatching.SetDepthStencilState(DepthStateDisabled);
		renderHolepatching.SetInputLayout(_countof(posAndUV), posAndUV);
		renderHolepatching.SetVertexShader(g_pQuadVS, sizeof(g_pQuadVS));
		renderHolepatching.SetPixelShader(g_prenderHolepatchingPS, sizeof(g_prenderHolepatchingPS));
		renderHolepatching.SetRenderTargetFormats(1, &mRenderOutBuffer.GetFormat(), DXGI_FORMAT_UNKNOWN);
		renderHolepatching.Finalize();

		GraphicsPSO& fxaa = mGraphicsPSO["fxaa"];
		fxaa = gbufferPSO;
		fxaa.SetDepthStencilState(DepthStateDisabled);
		fxaa.SetInputLayout(_countof(posAndUV), posAndUV);
		fxaa.SetVertexShader(g_pQuadVS, sizeof(g_pQuadVS));
		fxaa.SetPixelShader(g_pFxaaPS, sizeof(g_pFxaaPS));
		fxaa.SetRenderTargetFormats(1, &g_SceneColorBuffer.GetFormat(), DXGI_FORMAT_UNKNOWN);
		fxaa.Finalize();
	}

	{	// vertex index buffer
		GltfMeshStreamData mdata1;
		mdata1.m_Vertex.SetVertexType<Math::Vector3>(RHIDefine::PS_ATTRIBUTE_POSITION, RHIDefine::DT_FLOAT, RHIDefine::DT_FLOAT);
		mdata1.m_Vertex.SetVertexType<Math::Vector3>(RHIDefine::PS_ATTRIBUTE_NORMAL, RHIDefine::DT_FLOAT, RHIDefine::DT_FLOAT);
		mdata1.m_Vertex.SetVertexType<Math::Vector2>(RHIDefine::PS_ATTRIBUTE_COORDNATE0, RHIDefine::DT_FLOAT, RHIDefine::DT_FLOAT);
		LoadGltfModle("./Models/Sponza.gltf", mdata1);

		GltfMeshStreamData mdata2;
		mdata2.m_Vertex.SetVertexType<Math::Vector3>(RHIDefine::PS_ATTRIBUTE_POSITION, RHIDefine::DT_FLOAT, RHIDefine::DT_FLOAT);
		mdata2.m_Vertex.SetVertexType<Math::Vector3>(RHIDefine::PS_ATTRIBUTE_NORMAL, RHIDefine::DT_FLOAT, RHIDefine::DT_FLOAT);
		mdata2.m_Vertex.SetVertexType<Math::Vector2>(RHIDefine::PS_ATTRIBUTE_COORDNATE0, RHIDefine::DT_FLOAT, RHIDefine::DT_FLOAT);
		LoadGltfModle("./Models/lion.gltf", mdata2);

		mGeometryBuffers["Sponza Vertex"].Create(L"Sponza Vertex Buffer", mdata1.m_Vertex.GetVertexCount(), mdata1.m_Vertex.GetVertexStride(), mdata1.m_Vertex.GetBufferData());
		mGeometryBuffers["lion Vertex"].Create(L"lion Vertex Buffer", mdata2.m_Vertex.GetVertexCount(), mdata2.m_Vertex.GetVertexStride(), mdata2.m_Vertex.GetBufferData());
		mGeometryBuffers["Sponza Index"].Create(L"Sponza Index Buffer", mdata1.m_Indices.GetIndicesCount(), mdata1.m_Indices.GetIndicesStride(), mdata1.m_Indices.GetBuffer());
		mGeometryBuffers["lion Index"].Create(L"lion Index Buffer", mdata2.m_Indices.GetIndicesCount(), mdata2.m_Indices.GetIndicesStride(), mdata2.m_Indices.GetBuffer());
		mVertexViews["Sponza"] = mGeometryBuffers["Sponza Vertex"].VertexBufferView(0, mdata1.m_Vertex.GetByteSize(), mdata1.m_Vertex.GetVertexStride());
		mVertexViews["lion"] = mGeometryBuffers["lion Vertex"].VertexBufferView(0, mdata2.m_Vertex.GetByteSize(), mdata2.m_Vertex.GetVertexStride());
		mIndexViews["Sponza"] = mGeometryBuffers["Sponza Index"].IndexBufferView(0, mdata1.m_Indices.GetByteSize(), mdata1.m_Indices.GetIndicesType() == RHIDefine::IT_UINT32);
		mIndexViews["lion"] = mGeometryBuffers["lion Index"].IndexBufferView(0, mdata2.m_Indices.GetByteSize(), mdata2.m_Indices.GetIndicesType() == RHIDefine::IT_UINT32);

		float skyBoxPoints[] = {
			0.5f,  -0.5f, -0.5f, 1.0f,    // -z
			-0.5f, -0.5f, -0.5f, 1.0f,
			-0.5f, 0.5f,  -0.5f, 1.0f,
			-0.5f, 0.5f,  -0.5f, 1.0f,
			0.5f,  0.5f,  -0.5f, 1.0f,
			0.5f,  -0.5f, -0.5f, 1.0f,

			-0.5f, -0.5f, 0.5f,  1.0f,    //-x
			-0.5f, -0.5f, -0.5f, 1.0f,
			-0.5f, 0.5f,  -0.5f, 1.0f,
			-0.5f, 0.5f,  -0.5f, 1.0f,
			-0.5f, 0.5f,  0.5f,  1.0f,
			-0.5f, -0.5f, 0.5f,  1.0f,

			0.5f,  -0.5f, -0.5f, 1.0f,    //+x
			0.5f,  -0.5f, 0.5f,  1.0f,
			0.5f,  0.5f,  0.5f,  1.0f,
			0.5f,  0.5f,  0.5f,  1.0f,
			0.5f,  0.5f,  -0.5f, 1.0f,
			0.5f,  -0.5f, -0.5f, 1.0f,

			-0.5f, -0.5f, 0.5f,  1.0f,    // +z
			-0.5f, 0.5f,  0.5f,  1.0f,
			0.5f,  0.5f,  0.5f,  1.0f,
			0.5f,  0.5f,  0.5f,  1.0f,
			0.5f,  -0.5f, 0.5f,  1.0f,
			-0.5f, -0.5f, 0.5f,  1.0f,

			-0.5f, 0.5f,  -0.5f, 1.0f,    //+y
			0.5f,  0.5f,  -0.5f, 1.0f,
			0.5f,  0.5f,  0.5f,  1.0f,
			0.5f,  0.5f,  0.5f,  1.0f,
			-0.5f, 0.5f,  0.5f,  1.0f,
			-0.5f, 0.5f,  -0.5f, 1.0f,

			0.5f,  -0.5f, 0.5f,  1.0f,    //-y
			0.5f,  -0.5f, -0.5f, 1.0f,
			-0.5f, -0.5f, -0.5f, 1.0f,
			-0.5f, -0.5f, -0.5f, 1.0f,
			-0.5f, -0.5f, 0.5f,  1.0f,
			0.5f,  -0.5f, 0.5f,  1.0f,
		};
		mGeometryBuffers["Skybox Vertex"].Create(L"Skybox Vertex Buffer", 36, 4 * sizeof(float), skyBoxPoints);
		mVertexViews["Skybox"] = mGeometryBuffers["Skybox Vertex"].VertexBufferView(0, sizeof(skyBoxPoints), 4 * sizeof(float));

		float screenQuadPoints[] = {
			-1.0f, 3.0f, 0.5f, 0.0f, -1.0f,
			-1.0f, -1.0f, 0.5f, 0.0f, 1.0f,
			3.0f, -1.0f, 0.5f, 2.0f, 1.0f,
		};
		mGeometryBuffers["Quad Vertex"].Create(L"Quad Vertex Buffer", 3, 5 * sizeof(float), screenQuadPoints);
		mVertexViews["Quad"] = mGeometryBuffers["Quad Vertex"].VertexBufferView(0, sizeof(screenQuadPoints), 5 * sizeof(float));

		for (int i = 0; i < mdata1.m_subMeshs.size(); ++i)
		{
			SubmeshGeometry Submesh;
			Submesh.IndexCount = mdata1.m_subMeshs[i][1];
			Submesh.StartIndexLocation = mdata1.m_subMeshs[i][0];
			Submesh.BaseVertexLocation = 0;
			std::string name = "Sponza" + std::to_string(i);
			mDrawSubMeshNames.insert({ std::move(std::to_string(i)), Submesh });
		}
		for (int i = 0; i < mdata2.m_subMeshs.size(); ++i)
		{
			SubmeshGeometry Submesh;
			Submesh.IndexCount = mdata2.m_subMeshs[i][1];
			Submesh.StartIndexLocation = 0;
			Submesh.BaseVertexLocation = 0;
			std::string name = "lion" + std::to_string(i);
			mDrawSubMeshNames.insert({ std::move(std::to_string(mdata1.m_subMeshs.size() + i)), Submesh });
		}

		assignSponzaTextures();
		for (int i = 0; i < mDrawSubMeshNames.size(); ++i)
		{
			uint DiffuseMapIndex = 0;
			ObjectConstants obcs;
			obcs.metalness = 0;
			obcs.roughness = 0.5f;
			obcs.pbrMaterials = 1;
			obcs.PreWorld = obcs.World;

			if (i != mDrawSubMeshNames.size() - 1)
			{
				uint materialID = gMaterialIds[i];
				materialID *= 5;    //because it uses 5 basic textures for redering BRDF
				DiffuseMapIndex = ((gSponzaTextureIndexforMaterial[materialID + 0] & 0xFF) << 0) |
					((gSponzaTextureIndexforMaterial[materialID + 1] & 0xFF) << 8) |
					((gSponzaTextureIndexforMaterial[materialID + 2] & 0xFF) << 16) |
					((gSponzaTextureIndexforMaterial[materialID + 3] & 0xFF) << 24);

				obcs.World = Math::Matrix4::MakeTranslation(0.0f, -6.0f, 0.0f) * Math::Matrix4::MakeScale(0.02f);
			}
			else
			{
				DiffuseMapIndex = ((81 & 0xFF) << 0) | ((83 & 0xFF) << 8) | ((6 & 0xFF) << 16) | ((6 & 0xFF) << 24);

				obcs.World = Math::Matrix4::MakeTranslation(0.0f, -6.0f, 1.0f) * Math::Matrix4::MakeRotationY(-1.5708f) * Math::Matrix4::MakeScale(Math::Vector3(0.2f, 0.2f, -0.2f));
			}

			obcs.MaterialIndex = DiffuseMapIndex;
			mObjectCbData.push_back(obcs);
		}
	}

	{	// camera
		Math::Vector3 eye(20.0f, -2.0f, 0.9f);
		Math::Vector3 targetpos(0.0f, -2.0f, 0.9f);
		m_Camera.SetBDefine(true);
		m_Camera.SetFOV(XM_PIDIV4);
		m_Camera.ReverseZ(false);
		m_Camera.SetEyeAtUp(eye, targetpos, Vector3(kYUnitVector));
		m_Camera.SetZRange(0.1f, 1000.0f);
		m_CameraController.reset(new FlyingFPSCamera(m_Camera, Vector3(kYUnitVector)));
		m_CameraController->SetTimeScale(0.1f);
	}

	{	// Pre Draw
		ComputeContext& Context = ComputeContext::Begin(L"Pre Draw");

		{	// BRDFIntegration
			Context.SetRootSignature(mRootSignatures["BRDFIntegration"]);
			Context.TransitionResource(mBRDFIntegration, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			Context.SetDynamicDescriptors(0, 0, 1, &mBRDFIntegration.GetUAV());
			Context.SetPipelineState(mCompPSO["BRDFIntegration"]);
			UINT numGroupsX = (UINT)ceilf(gBRDFIntegrationSize / 16.0f);
			UINT numGroupsY = (UINT)ceilf(gBRDFIntegrationSize / 16.0f);
			Context.Dispatch(numGroupsX, numGroupsY, 1);
		}
		{	// panoToCube
			Context.SetRootSignature(mRootSignatures["panoToCube"]);
			Context.TransitionResource(mSkybox, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			Context.SetDynamicDescriptors(1, 0, 1, &mTextures["LA_Helipad"].GetSRV());
			Context.SetDynamicDescriptors(2, 0, gSkyboxMips, &mSkybox.GetUAV());
			Context.SetPipelineState(mCompPSO["panoToCube"]);

			struct CubeConstants
			{
				uint32_t mip;
				uint32_t maxSize;
			};

			static const UINT gThreadGroupSizeX = 16;
			static const UINT gThreadGroupSizeY = 16;
			for (uint32_t i = 0; i < gSkyboxMips; ++i)
			{
				CubeConstants cc{ i, gSkyboxSize };
				Context.SetConstantArray(0, 2, &cc);

				UINT numGroupsX = (uint32_t)((gSkyboxSize >> i) / gThreadGroupSizeX);
				UINT numGroupsY = (uint32_t)((gSkyboxSize >> i) / gThreadGroupSizeY);
				Context.Dispatch(Math::Max(1u, numGroupsX), Math::Max(1u, numGroupsY), 6);
			}
		}
		{	// IrradianceMap
			Context.SetRootSignature(mRootSignatures["IrradianceMap"]);
			Context.TransitionResource(mIrradianceMap, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			Context.SetDynamicDescriptors(1, 0, 1, &mSkybox.GetSRV());
			Context.SetDynamicDescriptors(2, 0, 1, &mIrradianceMap.GetUAV());
			Context.SetPipelineState(mCompPSO["IrradianceMap"]);
			UINT numGroupsX = (UINT)ceilf(gIrradianceSize / 16.0f);
			UINT numGroupsY = (UINT)ceilf(gIrradianceSize / 16.0f);
			Context.Dispatch(numGroupsX, numGroupsY, 6);
		}
		{	// SpecularMap
			Context.SetRootSignature(mRootSignatures["SpecularMap"]);
			Context.TransitionResource(mSpecularMap, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			Context.SetDynamicDescriptors(1, 0, 1, &mSkybox.GetSRV());
			Context.SetDynamicDescriptors(2, 0, gSpecularMips, &mSpecularMap.GetUAV());
			Context.SetPipelineState(mCompPSO["SpecularMap"]);

			struct CubeConstants
			{
				uint32_t mip;
				float roughness;
			};

			static const UINT gThreadGroupSizeX = 16;
			static const UINT gThreadGroupSizeY = 16;
			for (uint32_t i = 0; i < gSpecularMips; ++i)
			{
				CubeConstants cc{ i, (float)i / (float)(gSpecularMips - 1) };
				Context.SetConstantArray(0, 2, &cc);

				UINT numGroupsX = (uint32_t)((gSpecularSize >> i) / gThreadGroupSizeX);
				UINT numGroupsY = (uint32_t)((gSpecularSize >> i) / gThreadGroupSizeY);
				Context.Dispatch(Math::Max(1u, numGroupsX), Math::Max(1u, numGroupsY), 6);
			}
		}

		Context.Finish();
	}
}

void ScreenSpaceReflections::Cleanup(void)
{
	mBRDFIntegration.Destroy();
}

void ScreenSpaceReflections::Update(float deltaT)
{
	ScopedTimer _prof(L"Update State");

	m_CameraController->Update(deltaT);

	static CpuTimer s_timer;

	mPassCbData.PreViewProj = mPassCbData.ViewProj;
	mPassCbData.View = m_Camera.GetViewMatrix();
	mPassCbData.Proj = m_Camera.GetProjMatrix();
	mPassCbData.ViewProj = m_Camera.GetViewProjMatrix();
	mPassCbData.InvView = Math::Invert(mPassCbData.View);
	mPassCbData.InvProj = Math::Invert(mPassCbData.Proj);
	mPassCbData.InvViewProj = Math::Invert(mPassCbData.ViewProj);;

	mPassCbData.EyePosW.SetXYZ(m_Camera.GetPosition());
	mPassCbData.RenderTargetSize = Math::Vector4(g_DisplayWidth, g_DisplayHeight, 1.0f / g_DisplayWidth, 1.0f / g_DisplayHeight);
	mPassCbData.NearZ = m_Camera.GetNearClip();
	mPassCbData.FarZ = m_Camera.GetFarClip();
	mPassCbData.TotalTime = s_timer.GetTime();
	mPassCbData.DeltaTime = Graphics::GetFrameTime();
	mPassCbData.ViewPortSize = { (float)g_DisplayWidth, (float)g_DisplayHeight, 0.0f, 0.0f };

	mPassCbData.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };

	{	// Point Light
		PointLight light = {};
		light.mCol = Math::Vector4(1.0f, 0.5f, 0.1f, 0.0f);
		light.mPos = Math::Vector4(-12.5f, -3.5f, 4.7f, 0.0f);
		light.mRadius = 10.0f;
		light.mIntensity = 400.0f;

		mPassCbData.PLights[0] = light;

		light.mCol = Math::Vector4(1.0f, 0.5f, 0.1f, 0.0f);
		light.mPos = Math::Vector4(-12.5f, -3.5f, -3.7f, 0.0f);
		light.mRadius = 10.0f;
		light.mIntensity = 400.0f;

		mPassCbData.PLights[1] = light;

		// Add light to scene
		light.mCol = Math::Vector4(1.0f, 0.5f, 0.1f, 0.0f);
		light.mPos = Math::Vector4(9.5f, -3.5f, 4.7f, 0.0f);
		light.mRadius = 10.0f;
		light.mIntensity = 400.0f;

		mPassCbData.PLights[2] = light;

		light.mCol = Math::Vector4(1.0f, 0.5f, 0.1f, 0.0f);
		light.mPos = Math::Vector4(9.5f, -3.5f, -3.7f, 0.0f);
		light.mRadius = 10.0f;
		light.mIntensity = 400.0f;

		mPassCbData.PLights[3] = light;

		mPassCbData.amountOfPLights = 4;
	}
	{
		DirectionalLight light = {};
		light.mCol = Math::Vector4(1.0f, 1.0f, 1.0f, 5.0f);
		light.mPos = Math::Vector4(0.0f, 0.0f, 0.0f, 0.0f);
		light.mDir = Math::Vector4(-1.0f, -1.5f, 1.0f, 0.0f);

		mPassCbData.DLights[0] = light;

		mPassCbData.amountOfDLights = 1;
	}

	// cs uniform
	mCSUniformCbData.g_view = m_Camera.GetViewMatrix();
	mCSUniformCbData.g_proj = m_Camera.GetProjMatrix();
	mCSUniformCbData.g_inv_view = Math::Invert(mCSUniformCbData.g_view);
	mCSUniformCbData.g_inv_proj = Math::Invert(mCSUniformCbData.g_proj);
	mCSUniformCbData.g_prev_view_proj = mPassCbData.PreViewProj;
	mCSUniformCbData.g_inv_view_proj = mPassCbData.InvViewProj;

	mCSUniformCbData.g_frame_index++;
	mCSUniformCbData.g_max_traversal_intersections = gSSSR_MaxTravelsalIntersections;
	mCSUniformCbData.g_min_traversal_occupancy = gSSSR_MinTravelsalOccupancy;
	mCSUniformCbData.g_most_detailed_mip = gSSSR_MostDetailedMip;
	mCSUniformCbData.g_temporal_stability_factor = pSSSR_TemporalStability;
	mCSUniformCbData.g_depth_buffer_thickness = gSSSR_DepthThickness;
	mCSUniformCbData.g_samples_per_quad = gSSSR_SamplesPerQuad;
	mCSUniformCbData.g_temporal_variance_guided_tracing_enabled = gSSSR_TemporalVarianceEnabled;
	mCSUniformCbData.g_roughness_threshold = gSSSR_RougnessThreshold;
	mCSUniformCbData.g_skip_denoiser = gSSSR_SkipDenoiser;

	// Holepatching uniform
	mHolepatchingUniformData.renderMode = SCENE_ONLY;	//SCENE_WITH_REFLECTIONS
	mHolepatchingUniformData.useHolePatching = 0.0f;
	mHolepatchingUniformData.useExpensiveHolePatching = 0.0f;
	mHolepatchingUniformData.useNormalMap = 0.0f;
	mHolepatchingUniformData.intensity = 1.0f;
	mHolepatchingUniformData.useFadeEffect = 0.0f;
}

void ScreenSpaceReflections::_DoRenderScene(void)
{
	GraphicsContext& gfxContext = GraphicsContext::Begin(L"Scene Render");

	// G-Buffer pass
	D3D12_CPU_DESCRIPTOR_HANDLE rts[4] = { mGBufferA.GetRTV(), mGBufferB.GetRTV(), mGBufferC.GetRTV(), mGBufferD.GetRTV() };
	gfxContext.TransitionResource(mGBufferA, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
	gfxContext.TransitionResource(mGBufferB, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
	gfxContext.TransitionResource(mGBufferC, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
	gfxContext.TransitionResource(mGBufferD, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
	gfxContext.TransitionResource(mGBufferDepth, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);
	gfxContext.ClearColor(mGBufferA);
	gfxContext.ClearColor(mGBufferB);
	gfxContext.ClearColor(mGBufferC);
	gfxContext.ClearColor(mGBufferD);
	gfxContext.ClearDepth(mGBufferDepth);
	gfxContext.SetRenderTargets(4, rts, mGBufferDepth.GetDSV());
	gfxContext.SetViewportAndScissor(0, 0, mGBufferA.GetWidth(), mGBufferA.GetHeight());
	gfxContext.SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, mTextureHeap.GetHeapPointer());

	gfxContext.SetRootSignature(mRootSignatures["FillGbuffers"]);
	gfxContext.SetPipelineState(mGraphicsPSO["Skybox"]);
	//gfxContext.SetDynamicConstantBufferView(kMeshCBV, sizeof(SkyboxVSCB), &skyVSCB);
	gfxContext.SetDynamicConstantBufferView(kCommonCBV, sizeof(mPassCbData), &mPassCbData);
	gfxContext.SetDynamicDescriptors(kSkyRTV, 0, 1, &mSkybox.GetSRV());
	gfxContext.SetDynamicDescriptors(kIrradianceRTV, 0, 1, &mIrradianceMap.GetSRV());
	gfxContext.SetDynamicDescriptors(kSpecularRTV, 0, 1, &mSpecularMap.GetSRV());
	gfxContext.SetDescriptorTable(kTextureSRVs, mGameObjectTextures);
	gfxContext.SetVertexBuffer(0, mVertexViews["Skybox"]);
	gfxContext.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	gfxContext.Draw(36);

	gfxContext.SetPipelineState(mGraphicsPSO["FillGbuffers"]);
	for (size_t i = 0; i < mObjectCbData.size(); ++i)
	{
		if (i != mDrawSubMeshNames.size() - 1)
		{
			gfxContext.SetVertexBuffer(0, mVertexViews["Sponza"]);
			gfxContext.SetIndexBuffer(mIndexViews["Sponza"]);
		}
		else
		{
			gfxContext.SetVertexBuffer(0, mVertexViews["lion"]);
			gfxContext.SetIndexBuffer(mIndexViews["lion"]);
		}
		gfxContext.SetDynamicConstantBufferView(kMeshCBV, sizeof(mObjectCbData[i]), &mObjectCbData[i]);
		gfxContext.SetDynamicDescriptor(kMaterialSRV, 0, mMaterialConstants.GetCounterSRV(gfxContext));
		gfxContext.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		auto meshinfo = mDrawSubMeshNames[std::to_string(i)];
		gfxContext.DrawIndexed(meshinfo.IndexCount, meshinfo.StartIndexLocation, meshinfo.BaseVertexLocation);
	}

	gfxContext.TransitionResource(mGBufferA, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, true);
	gfxContext.TransitionResource(mGBufferB, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, true);
	gfxContext.TransitionResource(mGBufferC, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, true);
	gfxContext.TransitionResource(mGBufferD, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, true);
	gfxContext.TransitionResource(mGBufferDepth, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, true);
	gfxContext.TransitionResource(mRenderSceneBrdf, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
	//gfxContext.ClearColor(mRenderSceneBrdf);
	gfxContext.SetRenderTargets(1, &mRenderSceneBrdf.GetRTV());
	gfxContext.SetPipelineState(mGraphicsPSO["renderSceneBRDF"]);
	gfxContext.SetVertexBuffer(0, mVertexViews["Quad"]);
	gfxContext.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	gfxContext.Draw(3);

	gfxContext.TransitionResource(mRenderSceneBrdf, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, true);
	gfxContext.TransitionResource(mRenderOutBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
	//gfxContext.ClearColor(mRenderOutBuffer);
	gfxContext.SetRenderTargets(1, &mRenderOutBuffer.GetRTV());
	gfxContext.SetPipelineState(mGraphicsPSO["renderHolepatching"]);
	gfxContext.SetVertexBuffer(0, mVertexViews["Quad"]);
	gfxContext.SetDynamicConstantBufferView(kMeshCBV, sizeof(mHolepatchingUniformData), &mHolepatchingUniformData);
	gfxContext.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	gfxContext.Draw(3);

	gfxContext.TransitionResource(mRenderOutBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, true);
	gfxContext.TransitionResource(g_SceneColorBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
	//gfxContext.ClearColor(g_SceneColorBuffer);
	gfxContext.SetRenderTargets(1, &g_SceneColorBuffer.GetRTV());
	gfxContext.SetPipelineState(mGraphicsPSO["fxaa"]);
	gfxContext.SetDynamicDescriptors(kSkyRTV, 0, 1, &mRenderOutBuffer.GetSRV());
	gfxContext.SetVertexBuffer(0, mVertexViews["Quad"]);
	gfxContext.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	gfxContext.Draw(3);

	gfxContext.Finish();
}