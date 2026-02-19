#pragma once

struct SVGGElement : public SVGGraphicsElement {
	void configure_presentation_style(const std::vector<std::shared_ptr<SVGGraphicsElement>>& parent_stack, ID2D1DeviceContext* device_context, ID2D1Factory* d2d_factory) override;
};
