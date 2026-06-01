#include "output/VariationHistogramSvg.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <ostream>
#include <string>
#include <vector>

#include "Result.h"

namespace {

struct Metric {
    std::string title;
    std::string unit;
    std::vector<double> values;
};

std::vector<int> Histogram(const std::vector<double> &values, double &lo, double &hi, int bins) {
    lo = *std::min_element(values.begin(), values.end());
    hi = *std::max_element(values.begin(), values.end());
    if (std::abs(lo - hi) <= std::max(std::abs(lo), std::abs(hi)) * 1e-12) {
        const double pad = std::abs(lo) * 0.05 > 0 ? std::abs(lo) * 0.05 : 1.0;
        lo -= pad;
        hi += pad;
    }

    std::vector<int> counts(bins, 0);
    const double width = (hi - lo) / bins;
    for (double value : values) {
        int index = static_cast<int>((value - lo) / width);
        if (index < 0) index = 0;
        if (index >= bins) index = bins - 1;
        counts[index]++;
    }
    return counts;
}

std::string Format(double value) {
    std::ostringstream oss;
    if (value == 0) {
        return "0";
    }
    if (std::abs(value) >= 1000 || std::abs(value) < 0.01) {
        oss << std::scientific << std::setprecision(2) << value;
    } else {
        oss << std::defaultfloat << std::setprecision(3) << value;
    }
    return oss.str();
}

void Text(std::ostream &os, double x, double y, const std::string &text,
        int size = 13, const std::string &anchor = "start", const std::string &weight = "normal") {
    os << "<text x=\"" << x << "\" y=\"" << y << "\" font-size=\"" << size
       << "\" text-anchor=\"" << anchor
       << "\" font-family=\"Arial, sans-serif\" font-weight=\"" << weight
       << "\">" << text << "</text>\n";
}

void Panel(std::ostream &os, double x, double y, double width, double height,
        const Metric &metric, int bins) {
    const double marginLeft = 74;
    const double marginRight = 54;
    const double marginTop = 52;
    const double marginBottom = 58;
    const double plotX = x + marginLeft;
    const double plotY = y + marginTop;
    const double plotW = width - marginLeft - marginRight;
    const double plotH = height - marginTop - marginBottom;

    double lo = 0;
    double hi = 0;
    const std::vector<int> counts = Histogram(metric.values, lo, hi, bins);
    const int maxCount = *std::max_element(counts.begin(), counts.end());
    const double barW = plotW / bins;

    Text(os, x + 18, y + 30, metric.title, 15, "start", "bold");

    os << std::fixed << std::setprecision(2);
    for (int i = 0; i < bins; i++) {
        const double barH = maxCount > 0 ? std::max(plotH * counts[i] / maxCount - 1, 0.0) : 0;
        const double bx = plotX + i * barW + 1;
        const double by = plotY + plotH - 1 - barH;
        os << "<rect x=\"" << bx << "\" y=\"" << by
           << "\" width=\"" << std::max(barW - 2, 1.0)
           << "\" height=\"" << barH << "\" fill=\"#4f7cac\"/>\n";
    }
    os << std::defaultfloat;

    os << "<line x1=\"" << plotX << "\" y1=\"" << plotY + plotH
       << "\" x2=\"" << plotX + plotW << "\" y2=\"" << plotY + plotH
       << "\" stroke=\"#24292f\"/>\n";
    os << "<line x1=\"" << plotX << "\" y1=\"" << plotY
       << "\" x2=\"" << plotX << "\" y2=\"" << plotY + plotH
       << "\" stroke=\"#24292f\"/>\n";

    Text(os, plotX, plotY + plotH + 22, Format(lo), 11, "middle");
    Text(os, plotX + plotW, plotY + plotH + 22, Format(hi), 11, "middle");
    Text(os, plotX + plotW / 2, y + height - 16, metric.unit, 12, "middle");
    Text(os, plotX - 10, plotY + 4, std::to_string(maxCount), 11, "end");
    Text(os, plotX - 10, plotY + plotH + 4, "0", 11, "end");
}

}  // namespace

void WriteVariationHistogramSvg(std::ostream &os, const Result &result, int bins) {
    if (!result.bank || !result.bank->mat || !result.bank->mat->subarray) {
        return;
    }

    const auto &samples = result.bank->mat->subarray->monteCarloSamples;
    if (samples.empty()) {
        return;
    }

    std::vector<Metric> metrics = {
        {"Matchline delay", "ps", {}},
        {"Search latency", "ps", {}},
        {"Search dynamic energy", "pJ", {}},
        {"Sense margin", "V", {}},
    };
    for (const auto &sample : samples) {
        metrics[0].values.push_back(sample.matchlineDelay * 1e12);
        metrics[1].values.push_back(sample.searchLatency * 1e12);
        metrics[2].values.push_back(sample.searchDynamicEnergy * 1e12);
        metrics[3].values.push_back(sample.senseMargin);
    }

    const double panelW = 500;
    const double panelH = 320;
    const double gap = 28;
    const double cardPadding = 38;
    const double headerH = 72;
    const int cols = 2;
    const int rows = static_cast<int>((metrics.size() + cols - 1) / cols);
    const double gridW = cols * panelW + (cols - 1) * gap;
    const double gridH = rows * panelH + (rows - 1) * gap;
    const double width = gridW + 2 * cardPadding;
    const double height = headerH + gridH + 2 * cardPadding;
    const double gridX = cardPadding;
    const double gridY = cardPadding + headerH;

    os << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << width
       << "\" height=\"" << height << "\" viewBox=\"0 0 " << width << " " << height << "\">\n";
    os << "<rect x=\"0\" y=\"0\" width=\"" << width << "\" height=\"" << height
       << "\" fill=\"#ffffff\"/>\n";
    Text(os, width / 2, 34, "EvaCAM Monte Carlo Variation Histograms", 20, "middle", "bold");
    Text(os, width / 2, 58, std::to_string(samples.size()) + " samples", 13, "middle");

    for (size_t i = 0; i < metrics.size(); i++) {
        const int col = static_cast<int>(i) % cols;
        const int row = static_cast<int>(i) / cols;
        Panel(os, gridX + col * (panelW + gap), gridY + row * (panelH + gap),
                panelW, panelH, metrics[i], bins);
    }
    os << "</svg>\n";
}
