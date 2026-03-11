#ifndef EVACAMMATCHRESULT_H_
#define EVACAMMATCHRESULT_H_

struct EvaCAMMatchResult {
	bool hit;
	double searchLatency;
	double searchDynamicEnergy;
	double matchlineDelay;
	double senseMargin;
};

#endif /* EVACAMMATCHRESULT_H_ */
