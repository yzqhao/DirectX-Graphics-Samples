#include "IndicesStream.h"

#include "../Core/Math/Common.h"

IndicesStream::IndicesStream(void)
	:m_eType(RHIDefine::IT_UINT16)
	, m_ReserveIndicesCount(0)
	, m_CopyBytes(0)
	, m_IndicesCount(0)
{
}
	
IndicesStream::IndicesStream(const IndicesStream& rhs)
{
	_Copy(rhs);
}
	
IndicesStream::~IndicesStream(void)
{
	_Reset();
}
	
void IndicesStream::_Reset()
{
	m_eType = RHIDefine::IT_UINT16;
	m_CopyBytes = 0;
	m_IndicesCount = 0;
	m_ReserveIndicesCount = 0;
	m_DataVector.clear();
}
	
void IndicesStream::ReserveBuffer(int size)
{
	if (m_ReserveIndicesCount < size)
	{
		m_ReserveIndicesCount = size;
		m_DataVector.resize(size * m_CopyBytes);
	}
}
	
void IndicesStream::FullMerge(IndicesStream& is, int startIdx, int mergeCount)
{
	if (is.GetIndicesStride() != m_CopyBytes)
	{
		printf("indices-merge fails: unequal stride");
	}
	int srcIdxCount = is.GetIndicesCount();
	if (srcIdxCount > 0)
	{
		int dstCount = Math::Min(mergeCount, m_IndicesCount);
		ReserveBuffer(dstCount + srcIdxCount);
		const int copysize = srcIdxCount * m_CopyBytes;
		std::vector<unsigned int> indices(srcIdxCount);
		is.GetIndicesData(indices);

		if (m_eType == RHIDefine::IT_UINT16)
		{
			std::vector<unsigned short> s_indices(srcIdxCount);
			for (int i = 0; i < srcIdxCount; ++i)
			{
				s_indices[i] = indices[i] + startIdx;
			}
			memmove(m_DataVector.data() + dstCount * m_CopyBytes, (const void*)s_indices.data(), copysize);
		}
		else
		{
			for(auto& idx : indices)
			{
				idx += startIdx;
			}
			memmove(m_DataVector.data() + dstCount * m_CopyBytes, (const void*)indices.data(), copysize);
		}
		m_IndicesCount = Math::Max(dstCount + srcIdxCount, m_IndicesCount);
	}
}
	
void IndicesStream::SetIndicesType(RHIDefine::IndicesType type)
{
	assert(0 == m_ReserveIndicesCount);//先设置顶点格式
	m_eType = type;
	switch (type)
	{
	case RHIDefine::IT_UINT16: m_CopyBytes = 2; break;
	case RHIDefine::IT_UINT32: m_CopyBytes = 4; break;
	default:printf("invalid indices data type");
	}
	//chunk stride变更，重新设置buffer
	if (0 != m_ReserveIndicesCount)
	{
		m_DataVector.resize(m_ReserveIndicesCount * m_CopyBytes);
	}
}
	
void IndicesStream::GetIndicesData(std::vector<unsigned int>& indices) const
{
	indices.resize(m_IndicesCount);

	if (m_eType == RHIDefine::IT_UINT16)
	{
		unsigned short* pBuffer = (unsigned short*)m_DataVector.data();
		for (int i = 0; i < m_IndicesCount; i++)
		{
			indices[i] = (unsigned short)(*pBuffer);
			pBuffer++;
		}
	}
	else
	{
		unsigned int* pBuffer = (unsigned int*)m_DataVector.data();
		for (int i = 0; i < m_IndicesCount; i++)
		{
			indices[i] = *pBuffer;
			pBuffer++;
		}
	}
}
	
void IndicesStream::CopyIndicesBuffer(int indicecount, int indicesize, const byte* buffer)
{
	if (indicesize == m_CopyBytes)
	{
		ReserveBuffer(indicecount);
		m_IndicesCount = indicecount;
		memcpy(m_DataVector.data(), buffer, indicecount * indicesize);
	}
	else
	{
		printf("vertex copy error: size of indices is miss match");
	}
}
	
const IndicesStream& IndicesStream::_Copy(const IndicesStream& rhs)
{
	if (this != &rhs)
	{
		m_DataVector = rhs.m_DataVector;
		m_eType = rhs.m_eType;
		m_ReserveIndicesCount = rhs.m_ReserveIndicesCount;
		m_IndicesCount = rhs.m_IndicesCount;
		m_CopyBytes = rhs.m_CopyBytes;
	}
	return *this;
}
	
const IndicesStream& IndicesStream::operator=(const IndicesStream& rhs)
{
	return _Copy(rhs);
}
