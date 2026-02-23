#include "svglib.h"
#include "path.h"
#include <sstream>


void SVGPathElement::build_path(ID2D1Factory* d2d_factory, const std::wstring_view& pathData) {
	d2d_factory->CreatePathGeometry(&path_geometry);

	CComPtr<ID2D1GeometrySink> pSink;

	path_geometry->Open(&pSink);

	//SVG spec is very leinent on path syntax. White spaces are
	//entirely optional. Numbers can either be separated by comma or spaces.
	//Here we normalize the path by properly separating commands and numbers by spaces.
	std::wstringstream ws;
	std::wstring_view spaces(L", \t\r\n");

	for (wchar_t ch : pathData) {
		if (spaces.find_first_of(ch) != std::wstring_view::npos) {
			//Normalize all spaces to single space
			ws << L' ';
		}
		else if (ch == L'-') {
			//Insert space before negative sign
			ws << L' ';
			ws << ch;
		}
		else if (ch == L',') {
			ws << L' ';
		}
		else {
			ws << ch;
		}
	}

	wchar_t cmd = 0, last_cmd = 0;
	bool is_in_figure = false;
	std::wstring_view supported_cmds(L"MmLlHhVvQqTtCcSsAaZz");
	float current_x = 0.0, current_y = 0.0;
	float last_ctrl_x = 0.0, last_ctrl_y = 0.0;

	while (!ws.eof()) {
		//Read command letter
		if (!(ws >> cmd)) {
			//End of stream
			break;
		}

		if (supported_cmds.find_first_of(cmd) == std::wstring_view::npos) {
			//We did not find a command. Put it back.
			ws.unget();

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

			ws >> x >> y;

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

			ws >> x >> y;

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

			ws >> x;

			if (cmd == L'h') {
				x += current_x;
			}

			pSink->AddLine(D2D1::Point2F(x, current_y));

			//Update current point
			current_x = x;
		}
		else if (cmd == L'V' || cmd == L'v') {
			float y = 0.0;

			ws >> y;

			if (cmd == L'v') {
				y += current_y;
			}

			pSink->AddLine(D2D1::Point2F(current_x, y));

			//Update current point
			current_y = y;
		}
		else if (cmd == L'Q' || cmd == L'q') {
			float x1 = 0.0, y1 = 0.0, x2 = 0.0, y2 = 0.0;

			ws >> x1 >> y1 >> x2 >> y2;

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

			ws >> x2 >> y2;

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

			ws >> x1 >> y1 >> x2 >> y2 >> x3 >> y3;

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

			ws >> x2 >> y2 >> x3 >> y3;

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

			ws >> rx >> ry >> x_axis_rotation >> large_arc_flag >> sweep_flag >> x >> y;

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