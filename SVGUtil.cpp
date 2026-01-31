#include "SVGUtil.h"
#include <sstream>
#include <string_view>
#include <xmllite.h>

struct TransformFunction {
	std::wstring name;
	std::vector<float> values;
};

static void ltrim_str(std::wstring_view& source) {
	size_t pos = source.find_first_not_of(L" \t\r\n");

	if (pos == std::wstring_view::npos) {
		source.remove_prefix(source.length());
	}
	else {
		source.remove_prefix(pos);
	}
}

static bool get_transform_functions(std::wstring_view& source, std::vector<TransformFunction>& functions) {
	size_t start = 0;

	while (true) {
		TransformFunction f;
		size_t pos = source.find(L'(', start);

		if (pos == std::wstring::npos) {
			return false;
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

		std::wstring value_text(values);
		std::wstringstream ws(value_text);
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

void SVGGraphicsElement::render_tree(ID2D1DeviceContext* pContext) {
	//Save the old transform
	D2D1_MATRIX_3X2_F oldTransform;

	pContext->GetTransform(&oldTransform);

	//Combine all transforms
	D2D1_MATRIX_3X2_F combinedTransform = D2D1::Matrix3x2F::Identity();

	for (const auto& t : transforms) {
		combinedTransform = combinedTransform * t;
	}

	pContext->SetTransform(combinedTransform);

	render(pContext);

	//Render all child elements
	for (const auto& child : children) {
		child->render_tree(pContext);
	}

	pContext->SetTransform(oldTransform);
}

void SVGPathElement::buildPath(ID2D1Factory* pFactory, const std::wstring_view& pathData) {
	pFactory->CreatePathGeometry(&pathGeometry);

	CComPtr<ID2D1GeometrySink> pSink;

	pathGeometry->Open(&pSink);

	std::wstring pathStr(pathData);
	//Direct creation of string stream from string view only from C++ 26
	std::wstringstream ws(pathStr);
	wchar_t cmd = 0, last_cmd = 0;

	while (true) {
		ws >> cmd;

		if (cmd != L'L' && cmd != L'Z' && cmd != L'M') {
			ws.unget();

			if (last_cmd == L'M') {
				cmd = L'L';
			}
			else {
				cmd = last_cmd;
			}
		}

		if (cmd == L'Z') {
			//End of path
			break;
		}

		float x = 0.0, y = 0.0;

		if (cmd == L'M') {
			ws >> x >> y;

			pSink->BeginFigure(D2D1::Point2F(x, y), D2D1_FIGURE_BEGIN_FILLED);
		}
		else if (cmd == L'L') {
			ws >> x >> y;

			pSink->AddLine(D2D1::Point2F(x, y));
		}

		//std::wcout << cmd << " " << x << " " << y << std::endl;

		last_cmd = cmd;
	}

	pSink->Close();
}

void SVGPathElement::render(ID2D1DeviceContext* pContext) {
	pContext->FillGeometry(pathGeometry, fillBrush);
}

void SVGRectElement::render(ID2D1DeviceContext* pContext) {
	pContext->FillRectangle(
		D2D1::RectF(points[0], points[1], points[0] + points[2], points[1] + points[3]),
		fillBrush
	);
	pContext->DrawRectangle(
		D2D1::RectF(points[0], points[1], points[0] + points[2], points[1] + points[3]),
		strokeBrush
	);
}

bool SVGUtil::init(HWND _wnd)
{
	wnd = _wnd;

	HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &pFactory);
	
	if (!SUCCEEDED(hr)) {
		return false;
	}

	// Create Render Target
	RECT rc;

	GetClientRect(_wnd, &rc);

	hr = pFactory->CreateHwndRenderTarget(
		D2D1::RenderTargetProperties(),
		D2D1::HwndRenderTargetProperties(
			_wnd,
			D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top)
		),
		&pRenderTarget
	);

	if (!SUCCEEDED(hr)) {
		return false;
	}

	//Get the device context from the render target
	hr = pRenderTarget->QueryInterface(IID_PPV_ARGS(&pDeviceContext));

	if (!SUCCEEDED(hr)) {
		return false;
	}

	hr = pDeviceContext->CreateSolidColorBrush(
		D2D1::ColorF(D2D1::ColorF::Black),
		&defaultStrokeBrush
	);

	if (!SUCCEEDED(hr)) {
		return false;
	}

	hr = pDeviceContext->CreateSolidColorBrush(
		D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f),
		&defaultFillBrush
	);

	if (!SUCCEEDED(hr)) {
		return false;
	}

	return true;
}

// Resize the render target when the window size changes
void SVGUtil::resize()
{
	RECT rc;

	GetClientRect(wnd, &rc);
	pRenderTarget->Resize(D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top));
}

// Render the loaded bitmap onto the window
void SVGUtil::render()
{
	pDeviceContext->BeginDraw();
	pDeviceContext->Clear(D2D1::ColorF(D2D1::ColorF::Beige));

	

	pDeviceContext->EndDraw();
}

bool get_element_name(IXmlReader *pReader, std::wstring_view& name) {
	const wchar_t* pwszLocalName = NULL;
	UINT len;

	HRESULT hr = pReader->GetLocalName(&pwszLocalName, &len);

	if (!SUCCEEDED(hr)) {
		return false;
	}

	name = std::wstring_view(pwszLocalName, len);

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

bool SVGUtil::parse(const wchar_t* fileName) {
	CComPtr<IXmlReader> pReader;

	HRESULT hr = ::CreateXmlReader(__uuidof(IXmlReader), (void**)&pReader, NULL);

	if (!SUCCEEDED(hr)) {
		return false;
	}

	CComPtr<IStream> pFileStream;

	hr = SHCreateStreamOnFileEx(fileName, STGM_READ | STGM_SHARE_DENY_WRITE, FILE_ATTRIBUTE_NORMAL, FALSE, NULL, &pFileStream);
	
	if (!SUCCEEDED(hr)) {
		return false;
	}

	hr = pReader->SetInput(pFileStream);

	if (!SUCCEEDED(hr)) {
		return false;
	}

	while (true) {
		XmlNodeType nodeType;

		hr = pReader->Read(&nodeType);

		if (hr == S_FALSE) {
			break; //End of file
		}

		if (!SUCCEEDED(hr)) {
			return false;
		}

		if (nodeType == XmlNodeType_Element) {
			std::wstring_view element_name, attr_value;

			if (!get_element_name(pReader, element_name)) {
				return false;
			}

			if (get_attribute(pReader, L"transform", attr_value)) {
				std::vector<TransformFunction> functions;
				if (get_transform_functions(attr_value, functions)) {
					for (auto& f : functions) {
						OutputDebugStringW(f.name.c_str());
						OutputDebugStringW(L"\n\t");	

						for (float v : f.values) {
							OutputDebugStringW(std::to_wstring(v).c_str());
							OutputDebugStringW(L", ");
						}

						OutputDebugStringW(L"\n");
					}
				}
			}
		}
		else if (nodeType == XmlNodeType_Text) {
			const wchar_t* pwszValue = NULL;

			hr = pReader->GetValue(&pwszValue, NULL);
			if (!SUCCEEDED(hr)) {
				return false;
			}

			OutputDebugStringW(L"\tValue: ");
			OutputDebugStringW(pwszValue);
			OutputDebugStringW(L"\n");
		}
	}

	return true;
}
