#pragma once

struct SVGTextElement : public SVGGraphicsElement {
	std::wstring text_content;
	CComPtr<IDWriteFactory> dwrite_factory;
	CComPtr<IDWriteTextFormat> text_format;
	CComPtr<IDWriteTextLayout> text_layout;
	float baseline = 0.0f;

	void configure_presentation_style(const std::vector<std::shared_ptr<SVGGraphicsElement>>& parent_stack, ID2D1DeviceContext* device_context, ID2D1Factory* d2d_factory) override;
	void render(const SVGDevice& device) const override;
};