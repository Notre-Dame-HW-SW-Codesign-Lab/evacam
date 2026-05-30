#ifndef OUTPUT_VARIATIONSAMPLESCSV_H_
#define OUTPUT_VARIATIONSAMPLESCSV_H_

#include <iosfwd>

class Result;

void WriteVariationSamplesCsv(std::ostream &os, const Result &result);

#endif /* OUTPUT_VARIATIONSAMPLESCSV_H_ */
