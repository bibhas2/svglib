#pragma once

struct SVGGElement : public SVGGraphicsElement {
	void configure_presentation_style(const std::vector<std::shared_ptr<SVGGraphicsElement>>& parent_stack, ID2D1DeviceContext* pDeviceContext, ID2D1Factory* pD2DFactory) override;
};
