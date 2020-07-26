//
// UploadContextD3D11.cpp
//

#include "UploadContextD3D11.hpp"
#include "TextureD3D11.hpp"
#include "DeviceD3D11.hpp"

#include "../common/TextureStore.hpp"
#include <vector>

namespace MrQ2
{

void UploadContextD3D11::Init(const DeviceD3D11 & device)
{
    MRQ2_ASSERT(m_device == nullptr);
    m_device = &device;
}

void UploadContextD3D11::Shutdown()
{
    m_device = nullptr;
}

void UploadContextD3D11::UploadTextureImmediate(const TextureUploadD3D11 & upload_info)
{
    MRQ2_ASSERT(m_device != nullptr);
    MRQ2_ASSERT(upload_info.pixels != nullptr);
    MRQ2_ASSERT(upload_info.width != 0);
    MRQ2_ASSERT(upload_info.texture->m_resource != nullptr);

//    const UINT sub_rsrc  = 0; // no mips/slices
//    const UINT row_pitch = upload_info.width * 4; // RGBA
//    m_device->context->UpdateSubresource(upload_info.texture->m_resource.Get(), sub_rsrc, nullptr, upload_info.pixels, row_pitch, 0);

	//TODO
	std::vector<ColorRGBA32> mip1;
	mip1.resize((upload_info.width * upload_info.height) / 2);
	for (ColorRGBA32& c : mip1)
	{
		c = BytesToColor(255,0,0,255);
	}

	auto* texture_resource = upload_info.texture->m_resource.Get();
	const uint32_t mipmap_count = 2;
	const uint32_t mipmap_row_sizes[] = { upload_info.width * 4, (upload_info.width * 4)/2 }; // RGBA
	const void* mipmap_pixels[] = { upload_info.pixels, mip1.data() };

	for (uint32_t mip_index = 0; mip_index < mipmap_count; ++mip_index)
	{
		const uint32_t row_pitch = mipmap_row_sizes[mip_index];
		const void* pixels = mipmap_pixels[mip_index];
		m_device->context->UpdateSubresource(texture_resource, mip_index, nullptr, pixels, row_pitch, 0);
	}
}

} // MrQ2
