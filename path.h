#pragma once

struct SVGPathElement : public SVGGraphicsElement {
	CComPtr<ID2D1PathGeometry> path_geometry;

	void build_path(ID2D1Factory* pD2DFactory, const std::wstring_view& pathData);
	void compute_bbox() override;
	void render(ID2D1DeviceContext* pContext) override;
};
