#pragma once

struct SVGPathElement : public SVGGraphicsElement {
	CComPtr<ID2D1PathGeometry> path_geometry;

	void build_path(ID2D1Factory* d2d_factory, const std::wstring_view& pathData);
	void compute_bbox() override;
	void render(const SVGDevice& device) const override;
};
