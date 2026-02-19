#include "SVGUtil.h"
#include "g.h"

void SVGGElement::configure_presentation_style(const std::vector<std::shared_ptr<SVGGraphicsElement>>& parent_stack, ID2D1DeviceContext* pDeviceContext, ID2D1Factory* pD2DFactory) {
	//Group element doesn't need to create any brushes.
}
