#ifndef OUTPUT_VARIATIONHISTOGRAMSVG_H_
#define OUTPUT_VARIATIONHISTOGRAMSVG_H_

#include <iosfwd>

class Result;

void WriteVariationHistogramSvg(std::ostream &os, const Result &result, int bins = 40);

#endif /* OUTPUT_VARIATIONHISTOGRAMSVG_H_ */
