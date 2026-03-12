#ifndef MACROS_H_
#define MACROS_H_

#define TO_SECOND(x) \
    ((x) < 1e-9 ? (x) * 1e12 : (x) < 1e-6 ? (x) * 1e9 : (x) < 1e-3 ? (x) * 1e6 : (x) < 1 ? (x) * 1e3 : (x)) \
    << \
    ((x) < 1e-9 ? "ps" : (x) < 1e-6 ? "ns" : (x) < 1e-3 ? "us" : (x) < 1 ? "ms" : "s")

#define TO_BPS(x) \
    ((x) < 1e3 ? (x) : (x) < 1e6 ? (x) / 1e3 : (x) < 1e9 ? (x) / 1e6 : (x) < 1e12 ? (x) / 1e9 : (x) / 1e12) \
    << \
    ((x) < 1e3 ? "B/s" : (x) < 1e6 ? "KB/s" : (x) < 1e9 ? "MB/s" : (x) < 1e12 ? "GB/s" : "TB/s")

#define TO_JOULE(x) \
    ((x) < 1e-9 ? (x) * 1e12 : (x) < 1e-6 ? (x) * 1e9 : (x) < 1e-3 ? (x) * 1e6 : (x) < 1 ? (x) * 1e3 : (x)) \
    << \
    ((x) < 1e-9 ? "pJ" : (x) < 1e-6 ? "nJ" : (x) < 1e-3 ? "uJ" : (x) < 1 ? "mJ" : "J")

#define TO_WATT(x) \
    ((x) < 1e-9 ? (x) * 1e12 : (x) < 1e-6 ? (x) * 1e9 : (x) < 1e-3 ? (x) * 1e6 : (x) < 1 ? (x) * 1e3 : (x)) \
    << \
    ((x) < 1e-9 ? "pW" : (x) < 1e-6 ? "nW" : (x) < 1e-3 ? "uW" : (x) < 1 ? "mW" : "W")

#define TO_METER(x) \
    ((x) < 1e-9 ? (x) * 1e12 : (x) < 1e-6 ? (x) * 1e9 : (x) < 1e-3 ? (x) * 1e6 : (x) < 1 ? (x) * 1e3 : (x)) \
    << \
    ((x) < 1e-9 ? "pm" : (x) < 1e-6 ? "nm" : (x) < 1e-3 ? "um" : (x) < 1 ? "mm" : "m")

#define TO_SQM(x) \
    ((x) < 1e-12 ? (x) * 1e18 : (x) < 1e-6 ? (x) * 1e12 : (x) < 1 ? (x) * 1e6 : (x)) \
    << \
    ((x) < 1e-12 ? "nm^2" : (x) < 1e-6 ? "um^2" : (x) < 1 ? "mm^2" : "m^2")

#endif /* MACROS_H_ */
