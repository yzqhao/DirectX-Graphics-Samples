
#pragma once

#include "Math/Vector.h"
#include "IndicesStream.h"
#include "VertexStream.h"
#include "Math/BoundingBox.h"

#include <string>
#include <vector>

struct GltfMeshStreamData
{
	IndicesStream m_Indices;
	std::vector<int> m_TrianglesCnt;
	VertexStream m_Vertex;
	RHIDefine::RenderMode m_eMode;
	std::vector<std::vector<uint>> m_subMeshs;
	std::vector<std::shared_ptr<std::vector<Math::Vector3>>> m_pTargets;
	std::vector<int32_t> m_pTargetIds;
	Math::AxisAlignedBox m_BindingBox;
};

struct ShadowData
{
	void* pIndices;
	void* pAttributes[RHIDefine::PS_ATTRIBUTE_MAX];
};

void LoadGltfModle(const std::string& filename, GltfMeshStreamData& meshData, ShadowData* pShadowData = nullptr);