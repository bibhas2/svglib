#pragma once

struct SVGTextElement : public SVGGraphicsElement {
	std::wstring text_content;
	CComPtr<IDWriteFactory> dwrite_factory;
	CComPtr<IDWriteTextFormat> text_format;
	CComPtr<IDWriteTextLayout> text_layout;
	float baseline = 0.0f;

	void create_presentation_assets(const std::vector<std::shared_ptr<SVGGraphicsElement>>& parent_stack, const SVGDevice& device) override;
	void render(const SVGDevice& device) const override;
};