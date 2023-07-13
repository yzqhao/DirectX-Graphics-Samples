#include "GltfLoader.h"
#include "Macros.h"
#include <fstream>
#include "BundleReader.h"

#define CGLTF_IMPLEMENTATION
#include "cgltf/cgltf.h"

#define READ_IF_RETURN(cmp) if (cmp) { return ;}

static inline constexpr RHIDefine::ShaderAttribute util_cgltf_attrib_type_to_semantic(cgltf_attribute_type type, uint32_t index)
{
	switch (type)
	{
	case cgltf_attribute_type_position: return RHIDefine::ShaderAttribute::PS_ATTRIBUTE_POSITION;
	case cgltf_attribute_type_normal: return RHIDefine::ShaderAttribute::PS_ATTRIBUTE_NORMAL;
	case cgltf_attribute_type_tangent: return RHIDefine::ShaderAttribute::PS_ATTRIBUTE_TANGENT;
	case cgltf_attribute_type_color: return RHIDefine::ShaderAttribute::PS_ATTRIBUTE_COLOR0;
	case cgltf_attribute_type_joints: return RHIDefine::ShaderAttribute::PS_ATTRIBUTE_BONE_INEX;
	case cgltf_attribute_type_weights: return RHIDefine::ShaderAttribute::PS_ATTRIBUTE_BONE_WEIGHT;
	case cgltf_attribute_type_texcoord: return (RHIDefine::ShaderAttribute)(RHIDefine::ShaderAttribute::PS_ATTRIBUTE_COORDNATE0 + index);
	default: return RHIDefine::ShaderAttribute::PS_ATTRIBUTE_COORDNATE0;
	}
}

static inline constexpr RHIDefine::PixelFormat util_cgltf_type_to_image_format(cgltf_type type, cgltf_component_type compType)
{
	switch (type)
	{
	case cgltf_type_scalar:
		if (cgltf_component_type_r_8 == compType)
			return RHIDefine::PixelFormat::PF_L8;
		else if (cgltf_component_type_r_16 == compType)
			return RHIDefine::PixelFormat::PF_R16_FLOAT;
		else if (cgltf_component_type_r_16u == compType)
			return RHIDefine::PixelFormat::PF_R16_UINT;
		else if (cgltf_component_type_r_32f == compType)
			return RHIDefine::PixelFormat::PF_R32_FLOAT;
		else if (cgltf_component_type_r_32u == compType)
			return RHIDefine::PixelFormat::PF_R32_UINT;
	case cgltf_type_vec2:
		if (cgltf_component_type_r_8 == compType)
			return RHIDefine::PixelFormat::PF_R8G8;
		else if (cgltf_component_type_r_16 == compType)
			return RHIDefine::PixelFormat::PF_R16G16;
		else if (cgltf_component_type_r_16u == compType)
			return RHIDefine::PixelFormat::PF_R16G16;
		else if (cgltf_component_type_r_32f == compType)
			return RHIDefine::PixelFormat::PF_R32G32;
		else if (cgltf_component_type_r_32u == compType)
			return RHIDefine::PixelFormat::PF_R32G32;
	case cgltf_type_vec3:
		if (cgltf_component_type_r_8 == compType)
			return RHIDefine::PixelFormat::PF_R8G8B8;
		else if (cgltf_component_type_r_16 == compType)
			return RHIDefine::PixelFormat::PF_R16G16B16;
		else if (cgltf_component_type_r_16u == compType)
			return RHIDefine::PixelFormat::PF_R16G16B16;
		else if (cgltf_component_type_r_32f == compType)
			return RHIDefine::PixelFormat::PF_R32G32B32;
		else if (cgltf_component_type_r_32u == compType)
			return RHIDefine::PixelFormat::PF_R32G32B32;
	case cgltf_type_vec4:
		if (cgltf_component_type_r_8 == compType)
			return RHIDefine::PixelFormat::PF_R8G8B8A8;
		else if (cgltf_component_type_r_8u == compType)
			return RHIDefine::PixelFormat::PF_R8G8B8A8;
		else if (cgltf_component_type_r_16 == compType)
			return RHIDefine::PixelFormat::PF_RGBAHALF;
		else if (cgltf_component_type_r_16u == compType)
			return RHIDefine::PixelFormat::PF_RGBAHALF;
		else if (cgltf_component_type_r_32f == compType)
			return RHIDefine::PixelFormat::PF_RGBAFLOAT;
		else if (cgltf_component_type_r_32u == compType)
			return RHIDefine::PixelFormat::PF_RGBAFLOAT;
		// #NOTE: Not applicable to vertex formats
	case cgltf_type_mat2:
	case cgltf_type_mat3:
	case cgltf_type_mat4:
	default: return RHIDefine::PixelFormat::PF_AUTO;
	}
}


static uint32_t gltfAttributeStride(const cgltf_attribute* const* attributes, RHIDefine::ShaderAttribute semantic)
{
	const cgltf_size stride = attributes[semantic] ? attributes[semantic]->data->stride : 0;
	ASSERT(stride < UINT32_MAX);
	if (stride > UINT32_MAX)
	{
		printf("ShaderSemantic stride of this gltf_attribute is too big to store in uint32, value will be truncated");
		return UINT32_MAX;
	}
	return (uint32_t)stride;
}

static void s_readFileData(const std::string& filename, std::string& readTexts)
{
	FILE* file = NULL;
	fopen_s(&file, filename.c_str(), "rb");
	if (file == NULL)
		return;

	fseek(file, 0, SEEK_END);
	int rawLength = ftell(file);
	fseek(file, 0, SEEK_SET);
	if (rawLength < 0)
	{
		fclose(file);
		return;
	}

	readTexts.resize(rawLength);
	int readLength = fread(&*readTexts.begin(), 1, rawLength, file);
	fclose(file);
}

void LoadGltfModle(const std::string& filename, GltfMeshStreamData& meshData, ShadowData* pShadowData)
{
	std::string readTexts;
	s_readFileData(filename, readTexts);

	BundleReader binaryReader;
	binaryReader.init((char*)readTexts.data(), readTexts.size());

	// load
	cgltf_options options = {};
	cgltf_data* data = NULL;
	options.memory_alloc = [](void* user, cgltf_size size) { return malloc(size); };
	options.memory_free = [](void* user, void* ptr) { free(ptr); };
	cgltf_result result = cgltf_parse(&options, readTexts.data(), readTexts.size(), &data);

	if (cgltf_result_success != result)
	{
		ASSERT(0);
		return;
	}


	// Load buffers located in separate files (.bin) using our file system
	for (uint32_t i = 0; i < data->buffers_count; ++i)
	{
		const char* uri = data->buffers[i].uri;

		if (!uri || data->buffers[i].data)
		{
			continue;
		}

		if (strncmp(uri, "data:", 5) != 0 && !strstr(uri, "://"))
		{
			int t = filename.rfind('/');
			std::string urifilename = filename.substr(0, t+1) + uri;
			std::string readBinTexts;
			s_readFileData(urifilename, readBinTexts);
			data->buffers[i].data = malloc(readBinTexts.size());
			memcpy(data->buffers[i].data, readBinTexts.data(), readBinTexts.size());
		}
	}

	result = cgltf_load_buffers(&options, data, filename.c_str());
	if (cgltf_result_success != result)
	{
		ASSERT(false);
		return;
	}

	typedef void (*PackingFunction)(uint32_t count, uint32_t srcStride, uint32_t dstStride, uint32_t offset, const uint8_t* src, uint8_t* dst);

	uint32_t         vertexStrides[RHIDefine::ShaderAttribute::PS_ATTRIBUTE_MAX + 1] = {};
	uint32_t         vertexAttribCount[RHIDefine::ShaderAttribute::PS_ATTRIBUTE_MAX + 1] = {};
	uint32_t         vertexOffsets[RHIDefine::ShaderAttribute::PS_ATTRIBUTE_MAX + 1] = {};
	uint32_t         vertexBindings[RHIDefine::ShaderAttribute::PS_ATTRIBUTE_MAX + 1] = {};
	cgltf_attribute* vertexAttribs[RHIDefine::ShaderAttribute::PS_ATTRIBUTE_MAX + 1] = {};
	PackingFunction  vertexPacking[RHIDefine::ShaderAttribute::PS_ATTRIBUTE_MAX + 1] = {};
	for (uint32_t i = 0; i < RHIDefine::ShaderAttribute::PS_ATTRIBUTE_MAX + 1; ++i)
		vertexOffsets[i] = UINT_MAX;

	uint32_t indexCount = 0;
	uint32_t vertexCount = 0;
	uint32_t drawCount = 0;
	uint32_t jointCount = 0;
	uint32_t vertexBufferCount = 0;

	// Find number of traditional draw calls required to draw this piece of geometry
	// Find total index count, total vertex count
	for (uint32_t i = 0; i < data->meshes_count; ++i)
	{
		for (uint32_t p = 0; p < data->meshes[i].primitives_count; ++p)
		{
			const cgltf_primitive* prim = &data->meshes[i].primitives[p];
			indexCount += (uint32_t)prim->indices->count;
			vertexCount += (uint32_t)prim->attributes->data->count;
			++drawCount;

			for (uint32_t j = 0; j < prim->attributes_count; ++j)
			{
				if (prim->attributes[j].data->count != prim->attributes[0].data->count)
				{
					//LOGF(eERROR, "Mismatched vertex attribute count for %s, attribute index %d", pDesc->pFileName, j);
				}
				vertexAttribs[util_cgltf_attrib_type_to_semantic(prim->attributes[j].type, prim->attributes[j].index)] = &prim->attributes[j];
			}
		}
	}

	uint32_t defaultTexcoordSemantic = RHIDefine::ShaderAttribute::PS_ATTRIBUTE_MAX;
	uint32_t defaultTexcoordStride = 0;

	// Determine vertex stride for each binding
	for (uint32_t i = 0; i < meshData.m_Vertex.GetLayout().GetVertexLayouts().size(); ++i)
	{
		const VertexStreamLayout::Layout* attr = meshData.m_Vertex.GetLayout().GetVertexLayouts()[i];
		const cgltf_attribute* cgltfAttr = vertexAttribs[attr->GetAttributes()];

		bool defaultTexcoords = !cgltfAttr && (attr->GetAttributes() >= 0 && attr->GetAttributes() <= RHIDefine::ShaderAttribute::PS_ATTRIBUTE_MAX);
		ASSERT(cgltfAttr || defaultTexcoords);

		const uint32_t dstFormatSize = attr->Stride();

		vertexStrides[i] += dstFormatSize;
		vertexOffsets[attr->GetAttributes()] = attr->Offset();
		vertexBindings[attr->GetAttributes()] = i;
		++vertexAttribCount[i];
	}

	for (uint32_t i = 0; i < RHIDefine::ShaderAttribute::PS_ATTRIBUTE_MAX; ++i)
		if (vertexStrides[i])
			++vertexBufferCount;
	for (uint32_t i = 0; i < data->skins_count; ++i)
		jointCount += (uint32_t)data->skins[i].joints_count;

	const uint32_t indexStride = vertexCount > UINT16_MAX ? sizeof(uint32_t) : sizeof(uint16_t);
	RHIDefine::IndicesType it = (sizeof(uint16_t) == indexStride) ? RHIDefine::IT_UINT16 : RHIDefine::IT_UINT32;

	meshData.m_Vertex.ReserveBuffer(vertexCount); 
	meshData.m_Vertex.ForceVertexCount(vertexCount);

	meshData.m_Indices.SetIndicesType(it);
	meshData.m_Indices.ReserveBuffer(indexCount);
	//meshData.m_Indices.ForceIndicesCount(indexCount);

	if (pShadowData)
	{
		uint32_t shadowSize = 0;

		shadowSize += gltfAttributeStride(vertexAttribs, RHIDefine::PS_ATTRIBUTE_POSITION) * vertexCount;
		shadowSize += gltfAttributeStride(vertexAttribs, RHIDefine::PS_ATTRIBUTE_NORMAL) * vertexCount;
		shadowSize += gltfAttributeStride(vertexAttribs, RHIDefine::PS_ATTRIBUTE_COORDNATE0) * vertexCount;
		shadowSize += gltfAttributeStride(vertexAttribs, RHIDefine::PS_ATTRIBUTE_BONE_INEX) * vertexCount;
		shadowSize += gltfAttributeStride(vertexAttribs, RHIDefine::PS_ATTRIBUTE_BONE_WEIGHT) * vertexCount;
		shadowSize += indexCount * indexStride;

		void* temp = malloc(shadowSize);	// TODO: 没有处理释放
		pShadowData->pIndices = temp;
		pShadowData->pAttributes[RHIDefine::PS_ATTRIBUTE_POSITION] = (uint8_t*)pShadowData->pIndices + (indexCount * indexStride);
		pShadowData->pAttributes[RHIDefine::PS_ATTRIBUTE_NORMAL] = (uint8_t*)pShadowData->pAttributes[RHIDefine::PS_ATTRIBUTE_POSITION] + 
			gltfAttributeStride(vertexAttribs, RHIDefine::PS_ATTRIBUTE_POSITION) * vertexCount;
		pShadowData->pAttributes[RHIDefine::PS_ATTRIBUTE_COORDNATE0] = (uint8_t*)pShadowData->pAttributes[RHIDefine::PS_ATTRIBUTE_NORMAL] + 
			gltfAttributeStride(vertexAttribs, RHIDefine::PS_ATTRIBUTE_NORMAL) * vertexCount;
		pShadowData->pAttributes[RHIDefine::PS_ATTRIBUTE_BONE_INEX] = (uint8_t*)pShadowData->pAttributes[RHIDefine::PS_ATTRIBUTE_COORDNATE0] + 
			gltfAttributeStride(vertexAttribs, RHIDefine::PS_ATTRIBUTE_COORDNATE0) * vertexCount;
		pShadowData->pAttributes[RHIDefine::PS_ATTRIBUTE_BONE_WEIGHT] = (uint8_t*)pShadowData->pAttributes[RHIDefine::PS_ATTRIBUTE_BONE_INEX] + 
			gltfAttributeStride(vertexAttribs, RHIDefine::PS_ATTRIBUTE_BONE_INEX) * vertexCount;
		//ASSERT(((const char*)pShadowData) + shadowSize == ((char*)pShadowData->pAttributes[RHIDefine::PS_ATTRIBUTE_BONE_WEIGHT] + gltfAttributeStride(vertexAttribs, RHIDefine::PS_ATTRIBUTE_BONE_WEIGHT) * vertexCount));
	}
	
	indexCount = 0;
	vertexCount = 0;
	drawCount = 0;
	ASSERT(data->meshes_count == 1);
	for (uint32_t i = 0; i < data->meshes_count; ++i)
	{
		for (uint32_t p = 0; p < data->meshes[i].primitives_count; ++p)
		{
			const cgltf_primitive* prim = &data->meshes[i].primitives[p];
			meshData.m_subMeshs.push_back({ indexCount , (uint)prim->indices->count });

			/************************************************************************/
			// Fill index buffer for this primitive
			/************************************************************************/
			if (sizeof(uint16_t) == indexStride)
			{
				for (uint32_t idx = 0; idx < prim->indices->count; ++idx)
					meshData.m_Indices.PushIndicesDataFast(vertexCount + (uint32_t)cgltf_accessor_read_index(prim->indices, idx));
			}
			else
			{
				for (uint32_t idx = 0; idx < prim->indices->count; ++idx)
					meshData.m_Indices.PushIndicesDataFast(vertexCount + (uint32_t)cgltf_accessor_read_index(prim->indices, idx));
			}
			/************************************************************************/
			// Fill vertex buffers for this primitive
			/************************************************************************/
			for (uint32_t a = 0; a < prim->attributes_count; ++a)
			{
				cgltf_attribute* attr = &prim->attributes[a];
				uint32_t         index = util_cgltf_attrib_type_to_semantic(attr->type, attr->index);

				if (vertexOffsets[index] != UINT_MAX)
				{
					const uint32_t binding = vertexBindings[index];
					const uint32_t offset = vertexOffsets[index];
					const uint32_t stride = vertexStrides[binding];
					const uint8_t* src =
						(uint8_t*)attr->data->buffer_view->buffer->data + attr->data->offset + attr->data->buffer_view->offset;

					// If this vertex attribute is not interleaved with any other attribute use fast path instead of copying one by one
					// In this case a simple memcpy will be enough to transfer the data to the buffer
					if (1 == vertexAttribCount[binding])
					{
						for (int ii = 0; ii < attr->data->count; ++ii)
						{
							uint8_t* dst = (uint8_t*)meshData.m_Vertex.GetBufferData(vertexCount+ii);
							memcpy(dst + offset, src, attr->data->stride);
							src += attr->data->stride;
						}
					}
					else
					{
						uint8_t* dst = (uint8_t*)meshData.m_Vertex.GetBufferData();
						// Loop through all vertices copying into the correct place in the vertex buffer
						// Example:
						// [ POSITION | NORMAL | TEXCOORD ] => [ 0 | 12 | 24 ], [ 32 | 44 | 52 ], ... (vertex stride of 32 => 12 + 12 + 8)
						if (vertexPacking[index])
							vertexPacking[index]((uint32_t)attr->data->count, (uint32_t)attr->data->stride, stride, offset, src, dst);
						else
							for (uint32_t e = 0; e < attr->data->count; ++e)
								memcpy(dst + e * stride + offset, src + e * attr->data->stride, attr->data->stride);
					}
				}
			}

			indexCount += (uint32_t)prim->indices->count;
			vertexCount += (uint32_t)prim->attributes->data->count;
			++drawCount;
		}
	}

	if (pShadowData)
	{
		indexCount = 0;
		vertexCount = 0;

		for (uint32_t i = 0; i < data->meshes_count; ++i)
		{
			for (uint32_t p = 0; p < data->meshes[i].primitives_count; ++p)
			{
				const cgltf_primitive* prim = &data->meshes[i].primitives[p];
				/************************************************************************/
				// Fill index buffer for this primitive
				/************************************************************************/
				if (sizeof(uint16_t) == indexStride)
				{
					uint16_t* dst = (uint16_t*)pShadowData->pIndices;
					for (uint32_t idx = 0; idx < prim->indices->count; ++idx)
						dst[indexCount + idx] = vertexCount + (uint16_t)cgltf_accessor_read_index(prim->indices, idx);
				}
				else
				{
					uint32_t* dst = (uint32_t*)pShadowData->pIndices;
					for (uint32_t idx = 0; idx < prim->indices->count; ++idx)
						dst[indexCount + idx] = vertexCount + (uint32_t)cgltf_accessor_read_index(prim->indices, idx);
				}

				for (uint32_t a = 0; a < prim->attributes_count; ++a)
				{
					cgltf_attribute* attr = &prim->attributes[a];
					if (cgltf_attribute_type_position == attr->type)
					{
						const uint8_t* src =
							(uint8_t*)attr->data->buffer_view->buffer->data + attr->data->offset + attr->data->buffer_view->offset;
						uint8_t* dst = (uint8_t*)pShadowData->pAttributes[RHIDefine::PS_ATTRIBUTE_POSITION] + vertexCount * attr->data->stride;
						memcpy(dst, src, attr->data->count * attr->data->stride);
					}
					else if (cgltf_attribute_type_normal == attr->type)
					{
						const uint8_t* src =
							(uint8_t*)attr->data->buffer_view->buffer->data + attr->data->offset + attr->data->buffer_view->offset;
						uint8_t* dst = (uint8_t*)pShadowData->pAttributes[RHIDefine::PS_ATTRIBUTE_NORMAL] + vertexCount * attr->data->stride;
						memcpy(dst, src, attr->data->count * attr->data->stride);
					}
					else if (cgltf_attribute_type_texcoord == attr->type)
					{
						const uint8_t* src =
							(uint8_t*)attr->data->buffer_view->buffer->data + attr->data->offset + attr->data->buffer_view->offset;
						uint8_t* dst = (uint8_t*)pShadowData->pAttributes[RHIDefine::PS_ATTRIBUTE_COORDNATE0] + vertexCount * attr->data->stride;
						memcpy(dst, src, attr->data->count * attr->data->stride);
					}
					else if (cgltf_attribute_type_joints == attr->type)
					{
						const uint8_t* src =
							(uint8_t*)attr->data->buffer_view->buffer->data + attr->data->offset + attr->data->buffer_view->offset;
						uint8_t* dst = (uint8_t*)pShadowData->pAttributes[RHIDefine::PS_ATTRIBUTE_BONE_INEX] + vertexCount * attr->data->stride;
						memcpy(dst, src, attr->data->count * attr->data->stride);
					}
					else if (cgltf_attribute_type_weights == attr->type)
					{
						const uint8_t* src =
							(uint8_t*)attr->data->buffer_view->buffer->data + attr->data->offset + attr->data->buffer_view->offset;
						uint8_t* dst = (uint8_t*)pShadowData->pAttributes[RHIDefine::PS_ATTRIBUTE_BONE_WEIGHT] + vertexCount * attr->data->stride;
						memcpy(dst, src, attr->data->count * attr->data->stride);
					}
				}

				indexCount += (uint32_t)prim->indices->count;
				vertexCount += (uint32_t)prim->attributes->data->count;
			}
		}
	}
}
