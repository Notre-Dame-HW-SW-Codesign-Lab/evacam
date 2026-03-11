#ifndef EVACAM_MATCH_H_
#define EVACAM_MATCH_H_

#include <memory>
#include <string>
#include <vector>

#include "Bank.h"
#include "EvaCAMMatchResult.h"
#include "InputParameter.h"
#include "Wire.h"

class EvaCAM_Match {
public:
	explicit EvaCAM_Match(const std::string &configPath);

	bool match(const std::vector<int> &stored, const std::vector<int> &query) const;
	EvaCAMMatchResult evaluate(const std::vector<int> &stored, const std::vector<int> &query) const;
	size_t word_width() const;

private:
	void InitializeConfiguredBank();
	int SelectConfiguredValue(int minValue, int maxValue) const;
	void ValidateBinaryVector(const std::vector<int> &value, const char *name) const;
	std::shared_ptr<Wire> CreateLocalWire() const;
	std::shared_ptr<Wire> CreateGlobalWire() const;

	std::shared_ptr<InputParameter> inputParameter;
	std::shared_ptr<Bank> bank;
	std::shared_ptr<Wire> localWire;
	std::shared_ptr<Wire> globalWire;
	std::shared_ptr<CAM_Opt> camOpt;
};

#endif /* EVACAM_MATCH_H_ */
