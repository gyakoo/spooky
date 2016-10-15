#pragma once

#include <ppltasks.h>	// For create_task

namespace DX
{
	inline void ThrowIfFailed(HRESULT hr)
	{
		if (FAILED(hr))
		{
			// Set a breakpoint on this line to catch Win32 API errors.
			throw Platform::Exception::CreateException(hr);
		}
	}

    inline void ThrowIfFalse(bool expr)
    {
        ThrowIfFailed(expr ? (HRESULT)(S_OK) : (HRESULT)(E_FAIL));
    }

	// Function that reads from a binary file asynchronously.
	inline Concurrency::task<std::vector<byte>> ReadDataAsync(const std::wstring& filename)
	{
		using namespace Windows::Storage;
		using namespace Concurrency;

		auto folder = Windows::ApplicationModel::Package::Current->InstalledLocation;

		return create_task(folder->GetFileAsync(Platform::StringReference(filename.c_str()))).then([] (StorageFile^ file) 
		{
			return FileIO::ReadBufferAsync(file);
		}).then([] (Streams::IBuffer^ fileBuffer) -> std::vector<byte> 
		{
			std::vector<byte> returnBuffer;
			returnBuffer.resize(fileBuffer->Length);
			Streams::DataReader::FromBuffer(fileBuffer)->ReadBytes(Platform::ArrayReference<byte>(returnBuffer.data(), fileBuffer->Length));
			return returnBuffer;
		});
	}

	// Converts a length in device-independent pixels (DIPs) to a length in physical pixels.
	inline float ConvertDipsToPixels(float dips, float dpi)
	{
		static const float dipsPerInch = 96.0f;
		return floorf(dips * dpi / dipsPerInch + 0.5f); // Round to nearest integer.
	}

#if defined(_DEBUG)
	// Check for SDK Layer support.
	inline bool SdkLayersAvailable()
	{
		HRESULT hr = D3D11CreateDevice(
			nullptr,
			D3D_DRIVER_TYPE_NULL,       // There is no need to create a real hardware device.
			0,
			D3D11_CREATE_DEVICE_DEBUG,  // Check for the SDK layers.
			nullptr,                    // Any feature level will do.
			0,
			D3D11_SDK_VERSION,          // Always set this to D3D11_SDK_VERSION for Windows Store apps.
			nullptr,                    // No need to keep the D3D device reference.
			nullptr,                    // No need to know the feature level.
			nullptr                     // No need to keep the D3D device context reference.
			);

		return SUCCEEDED(hr);
	}
#endif

    inline DirectX::XMFLOAT2 WindowCoordsToScreenSpace(const DirectX::XMFLOAT2& wcoodrs, const DirectX::XMFLOAT2& winSize)
    {
        return DirectX::XMFLOAT2(0, 0);
    }

    inline uint32_t VectorColorToRGBA(DirectX::XMFLOAT4 fcolor)
    {
        uint32_t c = 0;
        for (int i = 0; i < 4; ++i)
        {
            const int shift = 8 * (3 - i);
            c |= uint32_t( (&fcolor.x)[i] * 255.0f) << shift;
        }
        return c;
    }

    inline uint32_t VectorColorToARGB(DirectX::XMFLOAT4 fcolor)
    {
        const float t = fcolor.x;
        fcolor.x = fcolor.w;
        fcolor.w = t;
        return VectorColorToRGBA(fcolor);
    }
    inline DirectX::XMFLOAT4 ColorConversion(uint32_t argb)
    {
        DirectX::XMFLOAT4 fargb;
        const float inv = 1.0f / 255.0f;
        fargb.x = (argb >> 24)*inv;
        fargb.y = ((argb >> 16)&0xff)*inv;
        fargb.z = ((argb >> 8) & 0xff)*inv;
        fargb.w = ((argb) & 0xff)*inv;
        return fargb;
    }
    inline DirectX::XMFLOAT4 GetColorAt(int ndx)
    {
        using namespace DirectX::Colors;
        static const DirectX::XMVECTORF32* colors[] = {
            &AliceBlue,
            &AntiqueWhite,
            &Aqua,
            &Aquamarine,
            &Azure,
            &Beige,
            &Bisque,
            &Black,
            &BlanchedAlmond,
            &Blue,
            &BlueViolet,
            &Brown,
            &BurlyWood,
            &CadetBlue,
            &Chartreuse,
            &Chocolate,
            &Coral,
            &CornflowerBlue,
            &Cornsilk,
            &Crimson,
            &Cyan,
            &DarkBlue,
            &DarkCyan,
            &DarkGoldenrod,
            &DarkGray,
            &DarkGreen,
            &DarkKhaki,
            &DarkMagenta,
            &DarkOliveGreen,
            &DarkOrange,
            &DarkOrchid,
            &DarkRed,
            &DarkSalmon,
            &DarkSeaGreen,
            &DarkSlateBlue,
            &DarkSlateGray,
            &DarkTurquoise,
            &DarkViolet,
            &DeepPink,
            &DeepSkyBlue,
            &DimGray,
            &DodgerBlue,
            &Firebrick,
            &FloralWhite,
            &ForestGreen,
            &Fuchsia,
            &Gainsboro,
            &GhostWhite,
            &Gold,
            &Goldenrod,
            &Gray,
            &Green,
            &GreenYellow,
            &Honeydew,
            &HotPink,
            &IndianRed,
            &Indigo,
            &Ivory,
            &Khaki,
            &Lavender,
            &LavenderBlush,
            &LawnGreen,
            &LemonChiffon,
            &LightBlue,
            &LightCoral,
            &LightCyan,
            &LightGreen,
            &LightGray,
            &LightPink,
            &LightSalmon,
            &LightSeaGreen,
            &LightSkyBlue,
            &LightSlateGray,
            &LightSteelBlue,
            &LightYellow,
            &Lime,
            &LimeGreen,
            &Linen,
            &Magenta,
            &Maroon,
            &MediumAquamarine,
            &MediumBlue,
            &MediumOrchid,
            &MediumPurple,
            &MediumSeaGreen,
            &MediumSlateBlue,
            &MediumSpringGreen,
            &MediumTurquoise,
            &MediumVioletRed,
            &MidnightBlue,
            &MintCream,
            &MistyRose,
            &Moccasin,
            &NavajoWhite,
            &Navy,
            &OldLace,
            &Olive,
            &OliveDrab,
            &Orange,
            &OrangeRed,
            &Orchid,
            &PaleGoldenrod,
            &PaleGreen,
            &PaleTurquoise,
            &PaleVioletRed,
            &PapayaWhip,
            &PeachPuff,
            &Peru,
            &Pink,
            &Plum,
            &PowderBlue,
            &Purple,
            &Red,
            &RosyBrown,
            &RoyalBlue,
            &SaddleBrown,
            &Salmon,
            &SandyBrown,
            &SeaGreen,
            &SeaShell,
            &Sienna,
            &Silver,
            &SkyBlue,
            &SlateBlue,
            &SlateGray,
            &Snow,
            &SpringGreen,
            &SteelBlue,
            &Tan,
            &Teal,
            &Thistle,
            &Tomato,
            &Turquoise,
            &Violet,
            &Wheat,
            &White,
            &WhiteSmoke,
            &Yellow,
            &YellowGreen
        };

        DirectX::XMFLOAT4 ret(colors[ndx]->f);
        return ret;
    }

    inline int GetColorCount()
    {
        return 139;
    }
}
