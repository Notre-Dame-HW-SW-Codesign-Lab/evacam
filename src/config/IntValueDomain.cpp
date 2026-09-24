#include "config/IntValueDomain.h"

#include <algorithm>
#include <cstdint>
#include <stdexcept>

namespace {
std::vector<int> NormalizeFixedValues(std::vector<int> values) {
    if (values.empty()) {
        throw std::invalid_argument("FixedSet domain requires at least one value.");
    }

    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end()), values.end());
    return values;
}

}  // namespace

IntValueDomain IntValueDomain::PowersOfTwo(int minValue, int maxValue) {
    if (minValue <= 0 || maxValue <= 0) {
        throw std::invalid_argument("PowersOfTwo domain requires positive bounds.");
    }
    if (minValue > maxValue) {
        throw std::invalid_argument("PowersOfTwo domain requires min <= max.");
    }

    return IntValueDomain(ValueDomainKind::PowersOfTwo, minValue, maxValue, {});
}

IntValueDomain IntValueDomain::Sequential(int minValue, int maxValue) {
    if (minValue > maxValue) {
        throw std::invalid_argument("Sequential domain requires min <= max.");
    }

    return IntValueDomain(ValueDomainKind::Sequential, minValue, maxValue, {});
}

IntValueDomain IntValueDomain::FixedSet(std::vector<int> values) {
    std::vector<int> normalized = NormalizeFixedValues(std::move(values));
    const int minValue = normalized.front();
    const int maxValue = normalized.back();
    return IntValueDomain(ValueDomainKind::FixedSet, minValue, maxValue, std::move(normalized));
}

std::vector<int> IntValueDomain::Values() const {
    switch (kind_) {
        case ValueDomainKind::PowersOfTwo: {
            std::vector<int> values;
            for (int value = min_; value <= max_; value *= 2) {
                values.push_back(value);
                // Stop before multiplication can exceed either the upper
                // domain bound or the representable range of int.
                if (value > max_ / 2) {
                    break;
                }
            }
            return values;
        }
        case ValueDomainKind::Sequential: {
            std::vector<int> values;
            const auto count = static_cast<std::uint64_t>(
                    static_cast<std::int64_t>(max_) - min_) + 1;
            values.reserve(count);
            for (int value = min_; ; ++value) {
                values.push_back(value);
                if (value == max_) {
                    break;
                }
            }
            return values;
        }
        case ValueDomainKind::FixedSet:
            return fixedValues_;
    }

    throw std::logic_error("Unknown ValueDomainKind.");
}

bool IntValueDomain::Contains(int value) const {
    const std::vector<int> values = Values();
    return std::find(values.begin(), values.end(), value) != values.end();
}

bool IntValueDomain::IsFixed() const {
    return Values().size() == 1;
}

int IntValueDomain::Min() const {
    return min_;
}

int IntValueDomain::Max() const {
    return max_;
}

ValueDomainKind IntValueDomain::Kind() const {
    return kind_;
}

IntValueDomain::IntValueDomain(ValueDomainKind kind, int minValue, int maxValue,
        std::vector<int> fixedValues)
    : kind_(kind), min_(minValue), max_(maxValue), fixedValues_(std::move(fixedValues)) {
}
