#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <vector>

#include "model/NandResistanceTable.h"
#include "TestSupport.h"

namespace {
using TestSupport::Require;
using TestSupport::AssertNear;
using TestSupport::AssertThrows;

NandDistanceContext Context() {
    NandDistanceContext c;
    c.table.resistance = {{10, 8, 6, 5}, {80, 11, 8, 6}, {160, 90, 12, 8}, {240, 170, 100, 13}};
    c.table.queryVoltages = {0.5, 1, 1.8, 2.5};
    c.symbols = 2;
    c.firstKey = 1;
    c.weights = {6, 5, 4, 3, 2};
    c.passResistance = 5;
    c.baseResistance = 40;
    c.baseTimeConstant = 150;
    c.prechargeVoltage = 0.8;
    c.invalidTimeIncrement = 1000;
    c.senseOffset = 0.01;
    c.requiredMargin = 0.02;
    return c;
}

// Independently expand complementary physical cells, without the table Pair helper.
std::pair<double, double> Physical(const NandDistanceContext &c,
        const std::vector<int> &s, const std::vector<int> &q) {
    double tau = c.baseTimeConstant, resistance = c.baseResistance;
    for (int i = 0; i < c.symbols; ++i) {
        const double a = c.table.resistance[s[i]][q[i]] - c.passResistance;
        const double b = c.table.resistance[3-s[i]][3-q[i]] - c.passResistance;
        tau += a * c.weights[c.firstKey+2*i] + b * c.weights[c.firstKey+2*i+1];
        resistance += a + b;
    }
    return {tau, resistance};
}

void TestExhaustiveDistanceBounds() {
    const auto c = Context();
    for (const std::vector<int> &query : {std::vector<int>{}, {0, 0}, {1, 2}}) {
        struct Range { double lo=1e99, hi=0, rlo=1e99, rhi=0; };
        std::map<int, Range> expected;
        for (int a=0; a<4; ++a) for (int b=0; b<4; ++b)
        for (int x=0; x<4; ++x) for (int y=0; y<4; ++y) {
            if (!query.empty() && query != std::vector<int>{x,y}) continue;
            const auto physical = Physical(c, {a,b}, {x,y});
            auto &e = expected[(a-x)*(a-x)+(b-y)*(b-y)];
            e.lo=std::min(e.lo, physical.first); e.hi=std::max(e.hi, physical.first);
            e.rlo=std::min(e.rlo, physical.second); e.rhi=std::max(e.rhi, physical.second);
        }
        const auto bounds=NandTableDistanceBounds(c, query, 300, 400);
        Require(bounds.size()==expected.size(), "exact reachable distance set");
        for (const auto &bound:bounds) {
            const auto e=expected.at(static_cast<int>(bound.squaredEuclideanDistance));
            AssertNear(bound.minimumTimeConstant,e.lo);
            AssertNear(bound.maximumTimeConstant,e.hi);
            AssertNear(bound.minimumVoltage,0.8*std::exp(-300/e.lo));
            AssertNear(bound.maximumVoltage,0.8*std::exp(-300/e.hi));
            AssertNear(bound.minimumConductance,1/e.rhi);
            AssertNear(bound.maximumConductance,1/e.rlo);
            AssertNear(bound.minimumSearchLatency,400);
            AssertNear(NandSquaredDistance(c,bound.minimumVoltageStored,bound.minimumVoltageQuery),bound.squaredEuclideanDistance);
            AssertNear(NandSquaredDistance(c,bound.maximumVoltageStored,bound.maximumVoltageQuery),bound.squaredEuclideanDistance);
            AssertNear(Physical(c,bound.minimumVoltageStored,bound.minimumVoltageQuery).first,e.lo);
            AssertNear(Physical(c,bound.maximumVoltageStored,bound.maximumVoltageQuery).first,e.hi);
        }
    }
    const auto left=Physical(c,{1,0},{0,0}), right=Physical(c,{0,1},{0,0});
    AssertNear(left.second,right.second);
    Require(left.first>right.first,"permuting equal series resistance changes distributed RC delay");
    auto lumped=c;
    lumped.weights.assign(5,2);
    AssertNear(Physical(lumped,{1,0},{0,0}).first,Physical(lumped,{0,1},{0,0}).first);
}

void TestThresholdsAndValidation() {
    auto c=Context();
    const auto bounds=NandTableDistanceBounds(c,{0,0},300,400);
    for (double threshold : {0.,1.,2.,4.,18.}) {
        double accepted=0,rejected=0.8*std::exp(-300/(bounds.front().minimumTimeConstant+c.invalidTimeIncrement));
        for (const auto &b:bounds) {
            if(b.squaredEuclideanDistance<=threshold) accepted=std::max(accepted,b.maximumVoltage);
            else rejected=std::min(rejected,b.minimumVoltage);
        }
        EvaCAMMatchResult result;
        result.squaredEuclideanDistance=2;
        SetNandThresholdDiagnostics(result,c,bounds,threshold,300);
        Require(result.hit==(threshold>=2),"logical threshold decision");
        AssertNear(result.acceptedMaximumVoltage,accepted);
        AssertNear(result.rejectedMinimumVoltage,rejected);
        AssertNear(result.senseMargin,(rejected-accepted)/2-c.senseOffset);
        AssertNear(result.senseMarginSlack,result.senseMargin-c.requiredMargin);
        Require(result.senseMarginPass==(result.senseMarginSlack>=0),"signed margin classification");
    }
    c.configuredReference=0.2;
    EvaCAMMatchResult result;
    SetNandThresholdDiagnostics(result,c,bounds,18,300);
    AssertNear(result.senseReferenceVoltage,0.2);
    Require(result.senseMargin<0 && !result.senseMarginPass,"overlap is preserved");
    AssertThrows<std::invalid_argument>([&]{NandSquaredDistance(c,{-1,0},{0,0});},"ordinary");
    AssertThrows<std::invalid_argument>([&]{NandSquaredDistance(c,{0},{0,0});},"symbols_per_key");
    AssertThrows<std::invalid_argument>([&]{NandTableDistanceBounds(c,{4,0},300,400);},"ordinary");
    AssertThrows<std::invalid_argument>([&]{NandTableDistanceBounds(c,{0,0},0,400);},"RC weights");
    AssertThrows<std::invalid_argument>([&]{SetNandThresholdDiagnostics(result,c,bounds,-1,300);},"threshold");
    AssertThrows<std::invalid_argument>([&]{SetNandThresholdDiagnostics(result,c,bounds,std::numeric_limits<double>::quiet_NaN(),300);},"threshold");
    c.symbols=1000; c.weights.assign(2001,1);
    AssertThrows<std::invalid_argument>([&]{NandTableDistanceBounds(c,{},300,400);},"budget");
}
}
int main() { TestExhaustiveDistanceBounds(); TestThresholdsAndValidation(); }
