#pragma once
struct SVGDefsElement : public SVGGraphicsElement {
	void render_tree(const SVGDevice& device) const override;
};