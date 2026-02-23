#pragma once

#include <xmllite.h>

struct TransformFunction {
	std::wstring name;
	std::vector<float> values;
};

void ltrim_str(std::wstring_view& source);
void rtrim_str(std::wstring_view& source);
void collapse_whitespace(std::wstring_view& source, std::wstring& result);
std::vector<std::wstring_view> split_string(std::wstring_view source, std::wstring_view separator);
bool get_css_color(std::wstring_view source, float& r, float& g, float& b, float& a);
bool get_attribute(IXmlReader* xml_reader, const wchar_t* attr_name, std::wstring_view& attr_value);
bool get_href_id(IXmlReader* xml_reader, std::wstring_view& ref_id);
bool get_href_id(std::wstring_view source, std::wstring_view& ref_id);
bool get_size_value(ID2D1DeviceContext* pContext, const std::wstring_view& source, float& size);
bool get_size_attribute(IXmlReader* xml_reader, ID2D1DeviceContext* device_context, const wchar_t* attr_name, float& size);
bool get_transform_functions(const std::wstring_view& source, std::vector<TransformFunction>& functions);
bool build_transform_matrix(const std::wstring_view& transform_str, D2D1_MATRIX_3X2_F& matrix);
bool char_is_number(wchar_t ch);