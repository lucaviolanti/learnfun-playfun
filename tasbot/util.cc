#include "util.h"

string SVGTickmarks(double width, double maxx, double span,
		    double tickheight, double tickfont) {
  string out;
  bool longone = true;
  for (double x = 0.0; x < maxx; x += span) {
    double xf = x / maxx;
    out += 
      StringPrintf("  <polyline fill=\"none\" opacity=\"0.5\""
		   " stroke=\"#000000\""
		   " stroke-width=\"1\" points=\"%f,0 %f,%f\" />\n",
		   width * xf, width * xf, 
		   longone ? tickheight * 2 : tickheight);
    if (longone)
      out += StringPrintf("<text x=\"%f\" y=\"%f\" font-size=\"%f\">"
			  "<tspan fill=\"#000000\">%d</tspan>"
			  "</text>\n",
			  width * xf + 3.0, 2.0 * tickheight + 2.0,
			  tickfont, (int)x);

    longone = !longone;
  }
  return out;
}

string DrawDots(int WIDTH, int HEIGHT,
		const string &color, double xf,
		const vector<double> &values, double maxval, 
		int chosen_idx) {
  // CHECK(colors.size() == values.size());
  vector<double> sorted = values;
  std::sort(sorted.begin(), sorted.end());
  string out;
  for (int i = 0; i < values.size(); i++) {
    int size = values.size();
    int sorted_idx = 
      lower_bound(sorted.begin(), sorted.end(), values[i]) - sorted.begin();
    double opacity = 1.0;
    if (sorted_idx < 0.1 * size || sorted_idx > 0.9 * size) {
      opacity = 0.2;
    } else if (sorted_idx < 0.2 * size || sorted_idx > 0.8 * size) {
      opacity = 0.4;
    } else if (sorted_idx < 0.3 * size || sorted_idx > 0.7 * size) {
      opacity = 0.8;
    } else if (sorted_idx == size / 2) {
      opacity = 1.0;
    }
    out += StringPrintf("<circle cx=\"%s\" cy=\"%s\" r=\"%d\" "
			"opacity=\"%.1f\" "
			"fill=\"%s\" />",
			Coord(xf * WIDTH).c_str(), 
			Coord(HEIGHT * (1.0 - (values[i] / maxval))).c_str(),
			(i == chosen_idx) ? 10 : 4,
			opacity,
			color.c_str());
  }
  return out += "\n";
}

// TODO: to cc-lib.
void HSV(double h, double s, double v,
	 unsigned char *r, unsigned char *g, unsigned char *b) {

  if (s == 0.0f) {
    *r = (unsigned char)(v * 255);
    *g = (unsigned char)(v * 255);
    *b = (unsigned char)(v * 255);
  } else {
    // float chroma = v * s;
    // float hprime = h / 60.0;
    // int ihprime = hprime;
    // float x = c * (1.0 - 

    float hue = h / 60.0f;
    int fh = (int)hue;
    float var_1 = v * (1 - s);
    float var_2 = v * (1 - s * (hue - fh));
    float var_3 = v * (1 - s * (1 - (hue - fh)));

    float red, green, blue;

    switch ((int)hue) {
    case 0:  red = v     ; green = var_3 ; blue = var_1; break;
    case 1:  red = var_2 ; green = v     ; blue = var_1; break;
    case 2:  red = var_1 ; green = v     ; blue = var_3; break;
    case 3:  red = var_1 ; green = var_2 ; blue = v    ; break;
    case 4:  red = var_3 ; green = var_1 ; blue = v    ; break;
    default: red = v     ; green = var_1 ; blue = var_2; break;
    }

    *r = (unsigned char)(red * 255);
    *g = (unsigned char)(green * 255);
    *b = (unsigned char)(blue * 255);
  }
}

InPlaceTerminal::InPlaceTerminal(int lines) 
  : lines(lines), last_was_output(false) {
  CHECK(lines > 0);
}

void InPlaceTerminal::Output(const string &s) {
  if (last_was_output) {
    for (int i = 0; i < lines; i++) {
      fprintf(stdout,
	      // Cursor to beginning of previous line
	      "\x1B[F"
	      // Clear line
	      "\x1B[2K"
	      );
    }
  }

  // Maybe cleaner to pad this to the length of the line
  // with spaces than to clear above.
  fprintf(stderr, "%s", s.c_str());

  last_was_output = true;
}

void InPlaceTerminal::Advance() {
  last_was_output = false;
}
