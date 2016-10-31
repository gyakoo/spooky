#include "pch.h"
#include "UIRenderer.h"

#include "Common/DirectXHelper.h"

using namespace SpookyAdulthood;
using namespace Microsoft::WRL;

// Initializes D2D resources used for text rendering.
UIRenderer::UIRenderer(const std::shared_ptr<DX::DeviceResources>& deviceResources) : 
	m_text(L""),
	m_deviceResources(deviceResources),
    m_time(.0f)
{
	ZeroMemory(&m_textMetrics, sizeof(DWRITE_TEXT_METRICS));

	// Create device independent resources
	ComPtr<IDWriteTextFormat> textFormat;
	DX::ThrowIfFailed(
		m_deviceResources->GetDWriteFactory()->CreateTextFormat(
			L"Segoe UI",
			nullptr,
			DWRITE_FONT_WEIGHT_LIGHT,
			DWRITE_FONT_STYLE_NORMAL,
			DWRITE_FONT_STRETCH_NORMAL,
			32.0f,
			L"en-US",
			&textFormat
			)
		);

	DX::ThrowIfFailed(
		textFormat.As(&m_textFormat)
		);

	DX::ThrowIfFailed(
		m_textFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR)
		);

	DX::ThrowIfFailed(
		m_deviceResources->GetD2DFactory()->CreateDrawingStateBlock(&m_stateBlock)
		);

	CreateDeviceDependentResources();
}

// Updates the text to be displayed.
void UIRenderer::Update(DX::StepTimer const& timer)
{
	// Update display text.
	uint32 fps = timer.GetFramesPerSecond();
    auto gameRes = DX::GameResources::instance;
    int ri = gameRes->m_curRoomIndex;
	//m_text = (fps > 0) ? std::to_wstring(fps) + L" / " + std::to_wstring(ri) : L" - FPS";
    m_text = std::to_wstring(gameRes->m_camera.m_bullets);

	ComPtr<IDWriteTextLayout> textLayout;
	DX::ThrowIfFailed(
		m_deviceResources->GetDWriteFactory()->CreateTextLayout(
			m_text.c_str(),
			(uint32) m_text.length(),
			m_textFormat.Get(),
			240.0f, // Max width of the input text.
			50.0f, // Max height of the input text.
			&textLayout
			)
		);

	DX::ThrowIfFailed(
		textLayout.As(&m_textLayout)
		);

	DX::ThrowIfFailed(
		m_textLayout->GetMetrics(&m_textMetrics)
		);

    m_time += (float)timer.GetElapsedSeconds();
}

// Renders a frame to the screen.
void UIRenderer::Render()
{
	ID2D1DeviceContext* context = m_deviceResources->GetD2DDeviceContext();
	Windows::Foundation::Size logicalSize = m_deviceResources->GetLogicalSize();
    auto gameRes = DX::GameResources::instance;

	context->SaveDrawingState(m_stateBlock.Get());
	context->BeginDraw();

	// Position on the bottom right corner
	/*D2D1::Matrix3x2F screenTranslation = D2D1::Matrix3x2F::Translation(
		logicalSize.Width - m_textMetrics.layoutWidth,
		logicalSize.Height - m_textMetrics.height
		);*/

    if (!gameRes->m_inMenu)
    {
        D2D1::Matrix3x2F screenTranslation = D2D1::Matrix3x2F::Translation(50.0f, logicalSize.Height - m_textMetrics.height);
        context->SetTransform(screenTranslation * m_deviceResources->GetOrientationTransform2D());
        DX::ThrowIfFailed(m_textFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING));
        context->DrawTextLayout(D2D1::Point2F(0.f, 0.f),m_textLayout.Get(),m_whiteBrush.Get());
    } 
    else
    {
        /*
        D2D1::Matrix3x2F screenTranslation = D2D1::Matrix3x2F::Translation(0.0f, 0);
        context->SetTransform(screenTranslation * m_deviceResources->GetOrientationTransform2D());
        DX::ThrowIfFailed(m_textFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER));
        context->DrawTextLayout( D2D1::Point2F(0.f, 0.f), m_textLayout.Get(), m_whiteBrush.Get() );
        */

        auto& spr = gameRes->m_sprite;
        spr.Begin2D(gameRes->m_camera);
        spr.Draw2D(37, XMFLOAT2(0, 0), XMFLOAT2(2.0f, 1.0f), 0); // splash
        spr.Draw2D(38, XMFLOAT2(0, -fabs(cos(m_time))*sin(m_time*0.05f)*0.05f), XMFLOAT2(2.0f, 1.0f), 0); // splashspooky
        spr.Draw2D(39, XMFLOAT2(sin(m_time*0.05f)*0.05f, 0), XMFLOAT2(2.0f, 1.0f), 0); // micro 
        spr.Draw2D(40, XMFLOAT2(0, cos(m_time*0.05f)*0.05f), XMFLOAT2(2.0f, 1.0f), 0); // scarejam
        XMFLOAT2 s(0.6f, 0.1f);
        float _f = fabs(sin(m_time*4.0f)*0.3f);
        s.x += s.x*_f;
        s.y += s.y*_f;
        spr.Draw2D(41, XMFLOAT2(0,-0.8f), s, 0); // left-click
        spr.End2D();

        // MENU        
        {
            static wchar_t buff[256];
            auto s = gameRes->m_sprites.get();
            auto f = gameRes->m_fontConsole.get();
            const wchar_t* text = L"(F1) Help / (ESC) Exit";
            auto measure = f->MeasureString(text);
            const float padY = XMVectorGetY(measure);
            const float padX = XMVectorGetX(measure);

            s->Begin();
            XMFLOAT2 p(10, logicalSize.Height - padY*4);
            f->DrawString(s, L"A game by Manu Marin", p, DirectX::Colors::White);
            p.y += padY;
            f->DrawString(s, L"mmrom@microsoft.com", p, DirectX::Colors::White);
            p.y += padY;
            f->DrawString(s, L"(F1) Help / (ESC) Exit", p, DirectX::Colors::White);
            s->End();
        }
        
    }

	// Ignore D2DERR_RECREATE_TARGET here. This error indicates that the device
	// is lost. It will be handled during the next call to Present.
	HRESULT hr = context->EndDraw();
	if (hr != D2DERR_RECREATE_TARGET)
	{
		DX::ThrowIfFailed(hr);
	}

	context->RestoreDrawingState(m_stateBlock.Get());
}

void UIRenderer::CreateDeviceDependentResources()
{
	DX::ThrowIfFailed(
		m_deviceResources->GetD2DDeviceContext()->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &m_whiteBrush)
		);
}
void UIRenderer::ReleaseDeviceDependentResources()
{
	m_whiteBrush.Reset();
}