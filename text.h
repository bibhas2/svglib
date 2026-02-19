#pragma once

struct SVGTextElement : public SVGGraphicsElement {
	std::wstring text_content;
	CComPtr<IDWriteFactory> pDWriteFactory;
	CComPtr<IDWriteTextFormat> text_format;
	CComPtr<IDWriteTextLayout> text_layout;
	float baseline = 0.0f;

	void configure_presentation_style(const std::vector<std::shared_ptr<SVGGraphicsElement>>& parent_stack, ID2D1DeviceContext* pDeviceContext, ID2D1Factory* pD2DFactory) override;
	void render(ID2D1DeviceContext* pContext) override;
};