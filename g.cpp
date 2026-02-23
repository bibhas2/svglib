#include "svglib.h"
#include "g.h"

void SVGGElement::create_presentation_assets(const std::vector<std::shared_ptr<SVGGraphicsElement>>& parent_stack, const SVGDevice& device) {
	//Group element doesn't need to create any brushes.
}
