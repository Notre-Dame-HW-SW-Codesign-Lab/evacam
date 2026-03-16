#ifndef EVACAM_CONFIG_INTVALUEDOMAIN_H_
#define EVACAM_CONFIG_INTVALUEDOMAIN_H_

#include <vector>

enum class ValueDomainKind {
    PowersOfTwo,
    Sequential,
    FixedSet,
};

class IntValueDomain {
    public:
        static IntValueDomain PowersOfTwo(int minValue, int maxValue);
        static IntValueDomain Sequential(int minValue, int maxValue);
        static IntValueDomain FixedSet(std::vector<int> values);

        std::vector<int> Values() const;
        bool Contains(int value) const;
        bool IsFixed() const;

        int Min() const;
        int Max() const;
        ValueDomainKind Kind() const;

    private:
        IntValueDomain(ValueDomainKind kind, int minValue, int maxValue, std::vector<int> fixedValues);

        ValueDomainKind kind_;
        int min_ = 0;
        int max_ = 0;
        std::vector<int> fixedValues_;
};

#endif  // EVACAM_CONFIG_INTVALUEDOMAIN_H_
