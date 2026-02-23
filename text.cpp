#include "svglib.h"
#include "text.h"
#include "utils.h"
#include <sstream>

SVGTextElement::SVGTextElement(const SVGTextElement& that) :
	SVGGraphicsElement(that),
	text_content(that.text_content),
	text_format(that.text_format),
	text_layout(that.text_layout),
	baseline(that.baseline) {
}

std::shared_ptr<SVGGraphicsElement> SVGTextElement::clone() const {
	return std::make_shared<SVGTextElement>(*this);
}

CComPtr<IDWriteTextFormat> build_text_format(IDWriteFactory* dwrite_factory, std::wstring_view family, std::wstring_view weight, std::wstring_view style, float size) {
	CComPtr<IDWriteTextFormat> tfmt;
	//Split the family string by commas and try to find the first installed font
	auto families = split_string(family, L",");
	DWRITE_FONT_WEIGHT fontWeight = DWRITE_FONT_WEIGHT_NORMAL;
	DWRITE_FONT_STYLE fontStyle = DWRITE_FONT_STYLE_NORMAL;

	if (weight == L"bold") {
		fontWeight = DWRITE_FONT_WEIGHT_BOLD;
	}
	else if (weight == L"normal") {
		fontWeight = DWRITE_FONT_WEIGHT_NORMAL;
	}
	else if (weight == L"light") {
		fontWeight = DWRITE_FONT_WEIGHT_LIGHT;
	}
	else if (weight == L"semibold") {
		fontWeight = DWRITE_FONT_WEIGHT_SEMI_BOLD;
	}
	else if (weight == L"medium") {
		fontWeight = DWRITE_FONT_WEIGHT_MEDIUM;
	}
	else if (weight == L"black") {
		fontWeight = DWRITE_FONT_WEIGHT_BLACK;
	}
	else if (weight == L"thin") {
		fontWeight = DWRITE_FONT_WEIGHT_THIN;
	}
	else {
		std::wstringstream ws;

		ws << weight;

		float wValue = 0.0f;

		if (ws >> wValue) {
			if (wValue >= 1.0f && wValue <= 1000.0f) {
				fontWeight = static_cast<DWRITE_FONT_WEIGHT>(static_cast<UINT32>(wValue));
			}
		}
	}

	if (style == L"italic") {
		fontStyle = DWRITE_FONT_STYLE_ITALIC;
	}
	else if (style == L"normal") {
		fontStyle = DWRITE_FONT_STYLE_NORMAL;
	}
	else if (style == L"oblique") {
		fontStyle = DWRITE_FONT_STYLE_OBLIQUE;
	}

	for (auto& fam : families) {
		ltrim_str(fam);

		std::wstring trimmedFamily(fam);

		HRESULT hr = dwrite_factory->CreateTextFormat(
			trimmedFamily.c_str(),
			nullptr,
			fontWeight,
			fontStyle,
			DWRITE_FONT_STRETCH_NORMAL,
			size,
			L"",
			&tfmt);

		if (SUCCEEDED(hr)) {
			return tfmt;
		}
	}

	return nullptr;
}

void SVGTextElement::render(const SVGDevice& device) const {
	if (fill_brush && text_format && text_layout) {
		//SVG spec requires x and y to specify the position of the text baseline
		D2D1_POINT_2F  origin = D2D1::Point2F(
			points[0],
			points[1] - baseline);

		device.device_context->DrawTextLayout(origin, text_layout, fill_brush);
	}
}

void SVGTextElement::create_presentation_assets(const std::vector<std::shared_ptr<SVGGraphicsElement>>& parent_stack, const SVGDevice& device) {
	SVGGraphicsElement::create_presentation_assets(parent_stack, device);

	std::wstring font_family;
	std::wstring font_weight;
	std::wstring font_style;
	std::wstring font_size_str;
	float fontSize = 12.0f;

	get_style_computed(parent_stack, L"font-family", font_family, L"Arial, sans-serif, Verdana");
	get_style_computed(parent_stack, L"font-weight", font_weight, L"normal");
	get_style_computed(parent_stack, L"font-style", font_style, L"normal");
	get_style_computed(parent_stack, L"font-size", font_size_str, L"12");

	get_size_value(device.device_context, font_size_str, fontSize);

	this->text_format = build_text_format(
		dwrite_factory,
		font_family,
		font_weight,
		font_style,
		fontSize
	);

	HRESULT hr = device.dwrite_factory->CreateTextLayout(
		text_content.c_str(),           // The string to be laid out
		text_content.size(),     // The length of the string
		text_format,    // The initial format (font, size, etc.)
		device.device_context->GetSize().width,       // Maximum width of the layout box
		device.device_context->GetSize().height,      // Maximum height of the layout box
		&text_layout    // Output: the resulting IDWriteTextLayout
	);

	if (!SUCCEEDED(hr)) {
		return;
	}

	// To prevent wrapping and force it to stay on one line:
	text_layout->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

	//Get the font baseline
	UINT32 lineCount = 0;

	//First get the line count
	hr = text_layout->GetLineMetrics(nullptr, 0, &lineCount);

	if (!SUCCEEDED(hr) && hr != HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER)) {
		return;
	}

	if (lineCount == 0) {
		//Nothing there
		return;
	}

	//Allocate memory for metrics
	std::vector<DWRITE_LINE_METRICS> lineMetrics(lineCount);

	hr = text_layout->GetLineMetrics(lineMetrics.data(), lineMetrics.size(), &lineCount);

	if (!SUCCEEDED(hr)) {
		return;
	}

	baseline = lineMetrics[0].baseline;
}

void SVGTextElement::compute_bbox() {
	bbox.left = points[0];
	bbox.top = points[1];
	//TBD Fix the width and height
	bbox.right = bbox.left + 600;
	bbox.bottom = bbox.top + 200;
}