#pragma once

struct SVGEllipseElement : public SVGGraphicsElement {
	void compute_bbox() override;
	void render(const SVGDevice& device) const override;
};
