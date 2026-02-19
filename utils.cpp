#include "SVGUtil.h"
#include "utils.h"
#include <sstream>

void ltrim_str(std::wstring_view& source) {
	size_t pos = source.find_first_not_of(L" \t\r\n");

	if (pos == std::wstring_view::npos) {
		source.remove_prefix(source.length());
	}
	else {
		source.remove_prefix(pos);
	}
}

void rtrim_str(std::wstring_view& source) {
	size_t pos = source.find_last_not_of(L" \t\r\n");

	if (pos == std::wstring_view::npos) {
		//Empty out the string
		source.remove_suffix(source.length());
	}
	else {
		source.remove_suffix(source.length() - pos - 1);
	}
}

//Collapse white spaces as per CSS and HTML spec
void collapse_whitespace(std::wstring_view& source, std::wstring& result) {
	result.clear();

	ltrim_str(source);

	wchar_t last_ch = 0;
	std::wstring_view white_spaces(L" \t\r\n");

	for (wchar_t ch : source) {
		if (white_spaces.find(ch) != std::wstring_view::npos) {
			//Normalize all white spaces
			ch = L' ';
		}

		if (ch == last_ch) {
			//Skip consecutive what spaces
			continue;
		}

		result.push_back(ch);
		last_ch = ch;
	}
}

std::vector<std::wstring_view>
split_string(std::wstring_view source, std::wstring_view separator) {
	std::vector<std::wstring_view> list;
	size_t pos, start = 0;

	while ((pos = source.find(separator, start)) != std::string_view::npos) {
		list.push_back(source.substr(start, (pos - start)));

		start = pos + separator.length();
	}

	list.push_back(source.substr(start));

	return list;
}

bool get_rgba(std::wstring_view source, float& r, float& g, float& b, float& a) {
	//Initialize named colors
	static std::map<std::wstring, UINT32> namedColors = {
		{L"black", D2D1::ColorF::Black},
		{L"white", D2D1::ColorF::White},
		{L"red", D2D1::ColorF::Red},
		{L"green", D2D1::ColorF::Green},
		{L"blue", D2D1::ColorF::Blue},
		{L"orange", D2D1::ColorF::Orange},
		{L"pink", D2D1::ColorF::Pink},
		{L"yellow", D2D1::ColorF::Yellow},
		{L"brown", D2D1::ColorF::Brown},
		{L"grey", D2D1::ColorF::Gray},
		{L"gray", D2D1::ColorF::Gray},
		{L"teal", D2D1::ColorF::Teal},
	};

	if (source.empty()) {
		return false;
	}

	// Trim leading whitespace
	ltrim_str(source);

	if (source == L"none") {
		return false;
	}

	//See if it is a named color
	if (source[0] != L'#') {
		auto iter = namedColors.find(std::wstring(source));
		if (iter != namedColors.end()) {
			UINT32 color = iter->second;

			r = ((color >> 16) & 0xFF) / 255.0f;
			g = ((color >> 8) & 0xFF) / 255.0f;
			b = (color & 0xFF) / 255.0f;
			a = 1.0f;

			return true;
		}
	}

	//Parse #RRGGBBAA color from the source
	if (source.length() < 7 || source[0] != L'#') {
		return false;
	}

	std::wstring rStr(source.substr(1, 2));
	std::wstring gStr(source.substr(3, 2));
	std::wstring bStr(source.substr(5, 2));

	r = static_cast<float>(std::stoul(rStr, nullptr, 16)) / 255.0f;
	g = static_cast<float>(std::stoul(gStr, nullptr, 16)) / 255.0f;
	b = static_cast<float>(std::stoul(bStr, nullptr, 16)) / 255.0f;

	if (source.length() == 9) {
		std::wstring aStr(source.substr(7, 2));

		a = static_cast<float>(std::stoul(aStr, nullptr, 16)) / 255.0f;
	}
	else {
		a = 1.0f;
	}

	return true;
}

bool get_attribute(IXmlReader* pReader, const wchar_t* attr_name, std::wstring_view& attr_value) {
	const wchar_t* pwszValue = NULL;
	UINT len;

	HRESULT hr = pReader->MoveToAttributeByName(attr_name, NULL);

	if (hr == S_FALSE) {
		return false; //Attribute not found
	}

	if (!SUCCEEDED(hr)) {
		return false;
	}

	hr = pReader->GetValue(&pwszValue, &len);

	if (!SUCCEEDED(hr)) {
		return false;
	}

	attr_value = std::wstring_view(pwszValue, len);

	return true;
}

//Gets the id reference from the href or xlink:href attribute.
//Only reference by ID values like href="#someId" or href="url(#someId)"
//are supported
bool get_href_id(IXmlReader* pReader, std::wstring_view& ref_id) {
	std::wstring_view source;

	if (!get_attribute(pReader, L"href", source)) {
		return false;
	}

	if (source.find(L"url") != std::wstring_view::npos) {
		size_t start = source.find(L"(");
		size_t end = source.rfind(L")");

		if (start == std::wstring_view::npos || end == std::wstring_view::npos) {
			return false;
		}

		source = source.substr(start + 1, end - start - 1);
	}

	ltrim_str(source);
	rtrim_str(source);

	if (source.empty()) {
		return false;
	}

	if (source[0] == L'#') {
		source = source.substr(1);

		if (source.empty()) {
			return false;
		}

		ref_id = source;

		return true;
	}

	return false;
}

bool get_size_value(ID2D1DeviceContext* pContext, const std::wstring_view& source, float& size) {

	try {
		size_t len;
		std::wstring_view unit;

		size = std::stof(std::wstring(source), &len);

		if (len < source.length()) {
			unit = std::wstring_view(source).substr(len);
		}
		else {
			return true; //No unit specified, assume pixels
		}

		float dpiX, dpiY;

		pContext->GetDpi(&dpiX, &dpiY);

		//Take an average of the horizontal and vertical DPI for unit conversion
		float dpi = (dpiX + dpiY) / 2.0f;

		if (unit == L"px") {
			//Pixels, do nothing
		}
		else if (unit == L"in") {
			size *= dpi; //Inches to pixels
		}
		else if (unit == L"cm") {
			size *= dpi / 2.54f; //Centimeters to pixels
		}
		else if (unit == L"mm") {
			size *= dpi / 25.4f; //Millimeters to pixels
		}
		else if (unit == L"pt") {
			size *= dpi / 72.0f; //Points to pixels
		}
		else if (unit == L"pc") {
			size *= dpi / 6.0f; //Picas to pixels
		}

		return true;
	}
	catch (const std::exception& e) {
		return false;
	}
}

bool get_size_attribute(IXmlReader* pReader, ID2D1DeviceContext* pContext, const wchar_t* attr_name, float& size) {
	std::wstring_view attr_value;

	if (!get_attribute(pReader, attr_name, attr_value)) {
		return false;
	}

	return get_size_value(pContext, attr_value, size);
}

bool get_transform_functions(const std::wstring_view& source, std::vector<TransformFunction>& functions) {
	size_t start = 0;

	while (true) {
		TransformFunction f;
		size_t pos = source.find(L'(', start);

		if (pos == std::wstring::npos) {
			//No more functions left
			return true;
		}

		std::wstring_view function_name = source.substr(start, pos - start);

		ltrim_str(function_name);

		if (function_name.empty()) {
			return false;
		}

		f.name = function_name;

		start = pos + 1;

		pos = source.find(L')', start);

		if (pos == std::wstring::npos) {
			return false;
		}

		std::wstring_view values = source.substr(start, pos - start);
		std::wstringstream ws;

		//Replace comma with spaces
		for (wchar_t ch : values) {
			if (ch == L',') {
				ws << L' ';
			}
			else {
				ws << ch;
			}
		}

		float v;

		while (ws >> v) {
			f.values.push_back(v);
		}

		functions.push_back(f);

		start = pos + 1;

		if (start >= source.length()) {
			break;
		}
	}

	return true;
}

bool build_transform_matrix(const std::wstring_view& transformStr, D2D1_MATRIX_3X2_F& outMatrix) {
	std::vector<TransformFunction> functions;

	if (!get_transform_functions(transformStr, functions)) {
		return false;
	}

	//Transformation functions are applied in reverse order
	for (auto it = functions.rbegin(); it != functions.rend(); ++it) {
		const auto& f = *it;

		if (f.name == L"translate") {
			if (f.values.size() == 1) {
				outMatrix = outMatrix * D2D1::Matrix3x2F::Translation(f.values[0], 0.0f);
			}
			else if (f.values.size() == 2) {
				outMatrix = outMatrix * D2D1::Matrix3x2F::Translation(f.values[0], f.values[1]);
			}
		}
		else if (f.name == L"scale") {
			if (f.values.size() == 1) {
				outMatrix = outMatrix * D2D1::Matrix3x2F::Scale(f.values[0], f.values[0]);
			}
			else if (f.values.size() == 2) {
				outMatrix = outMatrix * D2D1::Matrix3x2F::Scale(f.values[0], f.values[1]);
			}
		}
		else if (f.name == L"rotate") {
			if (f.values.size() == 1) {
				outMatrix = outMatrix * D2D1::Matrix3x2F::Rotation(f.values[0]);
			}
			else if (f.values.size() == 3) {
				outMatrix = outMatrix * D2D1::Matrix3x2F::Rotation(f.values[0], D2D1::Point2F(f.values[1], f.values[2]));
			}
		}
		else if (f.name == L"matrix") {
			if (f.values.size() == 6) {
				D2D1_MATRIX_3X2_F m = D2D1::Matrix3x2F(
					f.values[0], f.values[1],
					f.values[2], f.values[3],
					f.values[4], f.values[5]
				);

				outMatrix = outMatrix * m;
			}
		}
		else if (f.name == L"skew") {
			//Apply skew transform
			if (f.values.size() == 2) {
				outMatrix = outMatrix * D2D1::Matrix3x2F::Skew(f.values[0], f.values[1]);
			}
		}
	}

	return true;
}

bool char_is_number(wchar_t ch) {
	return (ch >= 48 && ch <= 57) || (ch == L'.') || (ch == L'-');
}

