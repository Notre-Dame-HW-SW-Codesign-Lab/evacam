#ifndef OUTPUT_UNIT_FORMATTER_H_
#define OUTPUT_UNIT_FORMATTER_H_

#include <ostream>

struct FormattedUnit {
    double value;
    const char *suffix;
};

inline std::ostream &operator<<(std::ostream &os, const FormattedUnit &unit) {
    return os << unit.value << unit.suffix;
}

inline FormattedUnit ToSecond(double value) {
    if (value < 1e-9) {
        return {value * 1e12, "ps"};
    }
    if (value < 1e-6) {
        return {value * 1e9, "ns"};
    }
    if (value < 1e-3) {
        return {value * 1e6, "us"};
    }
    if (value < 1) {
        return {value * 1e3, "ms"};
    }
    return {value, "s"};
}

inline FormattedUnit ToBps(double value) {
    if (value < 1e3) {
        return {value, "B/s"};
    }
    if (value < 1e6) {
        return {value / 1e3, "KB/s"};
    }
    if (value < 1e9) {
        return {value / 1e6, "MB/s"};
    }
    if (value < 1e12) {
        return {value / 1e9, "GB/s"};
    }
    return {value / 1e12, "TB/s"};
}

inline FormattedUnit ToJoule(double value) {
    if (value < 1e-9) {
        return {value * 1e12, "pJ"};
    }
    if (value < 1e-6) {
        return {value * 1e9, "nJ"};
    }
    if (value < 1e-3) {
        return {value * 1e6, "uJ"};
    }
    if (value < 1) {
        return {value * 1e3, "mJ"};
    }
    return {value, "J"};
}

inline FormattedUnit ToWatt(double value) {
    if (value < 1e-9) {
        return {value * 1e12, "pW"};
    }
    if (value < 1e-6) {
        return {value * 1e9, "nW"};
    }
    if (value < 1e-3) {
        return {value * 1e6, "uW"};
    }
    if (value < 1) {
        return {value * 1e3, "mW"};
    }
    return {value, "W"};
}

inline FormattedUnit ToMeter(double value) {
    if (value < 1e-9) {
        return {value * 1e12, "pm"};
    }
    if (value < 1e-6) {
        return {value * 1e9, "nm"};
    }
    if (value < 1e-3) {
        return {value * 1e6, "um"};
    }
    if (value < 1) {
        return {value * 1e3, "mm"};
    }
    return {value, "m"};
}

inline FormattedUnit ToSquareMeter(double value) {
    if (value < 1e-12) {
        return {value * 1e18, "nm^2"};
    }
    if (value < 1e-6) {
        return {value * 1e12, "um^2"};
    }
    if (value < 1) {
        return {value * 1e6, "mm^2"};
    }
    return {value, "m^2"};
}

#endif // OUTPUT_UNIT_FORMATTER_H_
