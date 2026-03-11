#include "svglib.h"
#include "path.h"
#include <cwchar>
#include <cerrno>
#include <climits>

SVGPathElement::SVGPathElement(const SVGPathElement& that) :
	SVGGraphicsElement(that),
	path_geometry(that.path_geometry) {
}

std::shared_ptr<SVGGraphicsElement> SVGPathElement::clone() const {
	return std::make_shared<SVGPathElement>(*this);
}

//Gets the path command letter at the current position. 
// If there is no command letter, returns false and cmd is not modified. 
// Advances pos to the next character after the command letter if found.
//path_data must be null-terminated.
static bool get_command(const std::wstring_view& path_data, size_t& pos, wchar_t& cmd) {
	std::wstring_view spaces(L" \t\r\n");

	while (pos < path_data.length()) {
		if (spaces.find_first_of(path_data[pos]) != std::wstring_view::npos) {
			++pos;

			continue;
		}

		cmd = path_data[pos];
		++pos;

		return true;
	}

	return false;
}

//Gets a floating point number at the current position. 
//Returns false if float conversion fails. 
//Advances pos to the next character after the number if successful.
//path_data must be null-terminated, else std::wcstof will fail.
static bool get_float_in_path(const std::wstring_view& path_data, size_t& pos, float& value) {
	std::wstring_view spaces(L" \t\r\n");

	while (pos < path_data.length()) {
		//Skip white spaces
		if (spaces.find_first_of(path_data[pos]) != std::wstring_view::npos) {
			++pos;

			continue;
		}

		//Skip comma
		if (path_data[pos] == L',') {
			++pos;

			continue;
		}

		wchar_t* end = 0;

		errno = 0; //clear

		value = std::wcstof(path_data.data() + pos, &end);

		if (end == path_data.data() + pos || errno == ERANGE) {
			//No number found, over or underflow
			return false;
		}

		pos = end - path_data.data();

		return true;
	}

	return false;
}

//Gets an int number at the current position. 
//Returns false if an int conversion fails. 
//Advances pos to the next character after the number if successful.
//path_data must be null-terminated, else std::wcstol will fail.
static bool get_int_in_path(const std::wstring_view& path_data, size_t& pos, int& value) {
	std::wstring_view spaces(L" \t\r\n");

	while (pos < path_data.length()) {
		//Skip white spaces
		if (spaces.find_first_of(path_data[pos]) != std::wstring_view::npos) {
			++pos;

			continue;
		}

		//Skip comma
		if (path_data[pos] == L',') {
			++pos;

			continue;
		}

		wchar_t* end = 0;

		errno = 0; //clear
		long result_long = std::wcstol(path_data.data() + pos, &end, 10);

		if (end == path_data.data() + pos || errno == ERANGE || result_long > INT_MAX || result_long < INT_MIN) {
			//No number found, over or underflow
			return false;
		}

		value = static_cast<int>(result_long);
		pos = end - path_data.data();

		return true;
	}

	return false;
}


void SVGPathElement::build_path(ID2D1Factory* d2d_factory, const std::wstring_view& path_data) {
	d2d_factory->CreatePathGeometry(&path_geometry);

	CComPtr<ID2D1GeometrySink> pSink;

	path_geometry->Open(&pSink);

	//SVG spec is very leinent on path syntax. White spaces are
	//entirely optional. Numbers can either be separated by comma or spaces.
	std::wstring_view spaces(L" \t\r\n");
	wchar_t cmd = 0, last_cmd = 0;
	bool is_in_figure = false;
	std::wstring_view supported_cmds(L"MmLlHhVvQqTtCcSsAaZz");
	float current_x = 0.0, current_y = 0.0;
	float last_ctrl_x = 0.0, last_ctrl_y = 0.0;
	size_t pos = 0;

	while (pos < path_data.length()) {
		//Read command letter
		if (!get_command(path_data, pos, cmd)) {
			//End of stream
			break;
		}

		if (supported_cmds.find_first_of(cmd) == std::wstring_view::npos) {
			//We did not find a command. Put it back.
			--pos;

			//As per the SVG spec deduce the command from the last command
			if (last_cmd == L'M') {
				//Subsequent moveto pairs are treated as lineto commands
				cmd = L'L';
			}
			else if (last_cmd == L'm') {
				//Subsequent moveto pairs are treated as lineto commands
				cmd = L'l';
			}
			else {
				//Continue with the last command
				cmd = last_cmd;
			}
		}

		if (cmd == L'M' || cmd == L'm') {
			float x = 0.0, y = 0.0;

			//If we are already in a figure, end it first
			if (is_in_figure) {
				pSink->EndFigure(D2D1_FIGURE_END_OPEN);
			}

			if (!get_float_in_path(path_data, pos, x) || !get_float_in_path(path_data, pos, y)) {
				//Invalid path data
				return;
			}

			if (cmd == L'm') {
				x += current_x;
				y += current_y;
			}

			pSink->BeginFigure(D2D1::Point2F(x, y), D2D1_FIGURE_BEGIN_FILLED);
			is_in_figure = true;
			//Update current point
			current_x = x;
			current_y = y;
		}
		else if (cmd == L'L' || cmd == L'l') {
			float x = 0.0, y = 0.0;

			if (!get_float_in_path(path_data, pos, x) || !get_float_in_path(path_data, pos, y)) {
				//Invalid path data
				return;
			}

			if (cmd == L'l') {
				x += current_x;
				y += current_y;
			}

			pSink->AddLine(D2D1::Point2F(x, y));

			//Update current point
			current_x = x;
			current_y = y;
		}
		else if (cmd == L'H' || cmd == L'h') {
			float x = 0.0;

			if (!get_float_in_path(path_data, pos, x)) {
				//Invalid path data
				return;
			}

			if (cmd == L'h') {
				x += current_x;
			}

			pSink->AddLine(D2D1::Point2F(x, current_y));

			//Update current point
			current_x = x;
		}
		else if (cmd == L'V' || cmd == L'v') {
			float y = 0.0;

			if (!get_float_in_path(path_data, pos, y)) {
				//Invalid path data
				return;
			}

			if (cmd == L'v') {
				y += current_y;
			}

			pSink->AddLine(D2D1::Point2F(current_x, y));

			//Update current point
			current_y = y;
		}
		else if (cmd == L'Q' || cmd == L'q') {
			float x1 = 0.0, y1 = 0.0, x2 = 0.0, y2 = 0.0;

			if (!get_float_in_path(path_data, pos, x1) || 
				!get_float_in_path(path_data, pos, y1) ||
				!get_float_in_path(path_data, pos, x2) ||
				!get_float_in_path(path_data, pos, y2)) {
				//Invalid path data
				return;
			}

			if (cmd == L'q') {
				x1 += current_x;
				y1 += current_y;
				x2 += current_x;
				y2 += current_y;
			}

			pSink->AddQuadraticBezier(D2D1::QuadraticBezierSegment(D2D1::Point2F(x1, y1), D2D1::Point2F(x2, y2)));

			//Update current point
			current_x = x2;
			current_y = y2;
			last_ctrl_x = x1;
			last_ctrl_y = y1;
		}
		else if (cmd == L'T' || cmd == L't') {
			float x1 = 0.0, y1 = 0.0, x2 = 0.0, y2 = 0.0;

			if (!get_float_in_path(path_data, pos, x2) ||
				!get_float_in_path(path_data, pos, y2)) {
				//Invalid path data
				return;
			}

			if (cmd == L't') {
				x2 += current_x;
				y2 += current_y;
			}

			//Calculate the control point by reflecting the last control point
			if (last_cmd == L'Q' || last_cmd == L'T' || last_cmd == L'q' || last_cmd == L't') {
				x1 = 2 * current_x - last_ctrl_x;
				y1 = 2 * current_y - last_ctrl_y;
			}
			else {
				x1 = current_x;
				y1 = current_y;
			}

			pSink->AddQuadraticBezier(D2D1::QuadraticBezierSegment(D2D1::Point2F(x1, y1), D2D1::Point2F(x2, y2)));

			//Update current point
			current_x = x2;
			current_y = y2;
			last_ctrl_x = x1;
			last_ctrl_y = y1;
		}
		else if (cmd == L'C' || cmd == L'c') {
			float x1 = 0.0, y1 = 0.0, x2 = 0.0, y2 = 0.0, x3 = 0.0, y3 = 0.0;

			if (!get_float_in_path(path_data, pos, x1) ||
				!get_float_in_path(path_data, pos, y1) ||
				!get_float_in_path(path_data, pos, x2) ||
				!get_float_in_path(path_data, pos, y2) ||
				!get_float_in_path(path_data, pos, x3) ||
				!get_float_in_path(path_data, pos, y3)) {
				//Invalid path data
				return;
			}

			if (cmd == L'c') {
				x1 += current_x;
				y1 += current_y;
				x2 += current_x;
				y2 += current_y;
				x3 += current_x;
				y3 += current_y;
			}

			pSink->AddBezier(D2D1::BezierSegment(D2D1::Point2F(x1, y1), D2D1::Point2F(x2, y2), D2D1::Point2F(x3, y3)));

			//Update current point
			current_x = x3;
			current_y = y3;
			last_ctrl_x = x2;
			last_ctrl_y = y2;
		}
		else if (cmd == L'S' || cmd == L's') {
			float x1 = 0.0, y1 = 0.0, x2 = 0.0, y2 = 0.0, x3 = 0.0, y3 = 0.0;

			if (!get_float_in_path(path_data, pos, x2) ||
				!get_float_in_path(path_data, pos, y2) ||
				!get_float_in_path(path_data, pos, x3) ||
				!get_float_in_path(path_data, pos, y3)) {
				//Invalid path data
				return;
			}

			if (cmd == L's') {
				x2 += current_x;
				y2 += current_y;
				x3 += current_x;
				y3 += current_y;
			}

			//Calculate the first control point by reflecting the last control point
			if (last_cmd == L'C' || last_cmd == L'S' || last_cmd == L'c' || last_cmd == L's') {
				x1 = 2 * current_x - last_ctrl_x;
				y1 = 2 * current_y - last_ctrl_y;
			}
			else {
				x1 = current_x;
				y1 = current_y;
			}

			pSink->AddBezier(D2D1::BezierSegment(D2D1::Point2F(x1, y1), D2D1::Point2F(x2, y2), D2D1::Point2F(x3, y3)));

			//Update current point
			current_x = x3;
			current_y = y3;
			last_ctrl_x = x2;
			last_ctrl_y = y2;
		}
		else if (cmd == L'A' || cmd == L'a') {
			// TODO: Handle elliptical arc commands
			float rx = 0.0, ry = 0.0, x_axis_rotation = 0.0, x = 0.0, y = 0.0;
			int large_arc_flag = 0, sweep_flag = 0;

			if (!get_float_in_path(path_data, pos, rx) ||
				!get_float_in_path(path_data, pos, ry) ||
				!get_float_in_path(path_data, pos, x_axis_rotation) ||
				!get_int_in_path(path_data, pos, large_arc_flag) ||
				!get_int_in_path(path_data, pos, sweep_flag) ||
				!get_float_in_path(path_data, pos, x) ||
				!get_float_in_path(path_data, pos, y)) {
				//Invalid path data
				return;
			}

			if (cmd == L'a') {
				x += current_x;
				y += current_y;
			}

			pSink->AddArc(D2D1::ArcSegment(
				D2D1::Point2F(x, y),
				D2D1::SizeF(rx, ry),
				x_axis_rotation,
				(sweep_flag != 0) ? D2D1_SWEEP_DIRECTION_CLOCKWISE : D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE,
				(large_arc_flag != 0) ? D2D1_ARC_SIZE_LARGE : D2D1_ARC_SIZE_SMALL
			));

			//Update current point
			current_x = x;
			current_y = y;
		}
		else if (cmd == L'Z' || cmd == L'z') {
			//Close the current figure
			if (is_in_figure) {
				pSink->EndFigure(D2D1_FIGURE_END_CLOSED);
				is_in_figure = false;
			}
		}

		last_cmd = cmd;
	}

	//End of path
	if (is_in_figure) {
		pSink->EndFigure(D2D1_FIGURE_END_OPEN);

		is_in_figure = false;
	}

	pSink->Close();
}

void SVGPathElement::compute_bbox() {
	if (path_geometry) {
		D2D1_RECT_F r;
		HRESULT hr = path_geometry->GetBounds(
			D2D1::IdentityMatrix(),
			&r);

		if (SUCCEEDED(hr)) {
			bbox = r;
		}
	}
}

void SVGPathElement::render(const SVGDevice& device) const {
	if (fill_brush) {
		device.device_context->FillGeometry(path_geometry, fill_brush);
	}
	if (stroke_brush) {
		device.device_context->DrawGeometry(path_geometry, stroke_brush, stroke_width, stroke_style);
	}
}