/*
 * CAM_Line.h
 *
 *  some line, whatever wl, bl, sl, ml, etc
 */

#ifndef CAM_LINE_H_
#define CAM_LINE_H_

#include "typedef.h"
#include "Wire.h"
#include "Technology.h"
#include "InputParameter.h"

class CAM_Line {
public:
	CAM_Line() {
                initialized = false;
                invalid = false;
        }

	CAM_Line(const CAM_Line&) {}
	virtual ~CAM_Line() {}

	/* Functions */
	void Initialize(bool _isRow, int _index, double _len, long long _numCell, 
                std::shared_ptr<InputParameter> _inputParameter, std::shared_ptr<Wire> _localWire);
	void Initialize(double _len, long long _numCell, double _MuxWidth, 
                std::shared_ptr<InputParameter> _inputParameter, std::shared_ptr<Wire> _localWire); // for mux signal only

	void CalcMaxCurrent();
	void CalcMuxWidth();
	void PrintLine();

	/* Properties */
	bool initialized;
	bool invalid;

	CAMPort CellPort;
	int index;
        int temperature;
	double len;
	double cap;
	double res;
	bool isRow;
	double numCell;
	double maxCurrent;
	double minMuxWidth;

        std::shared_ptr<InputParameter> inputParameter;
        std::shared_ptr<Wire> localWire;
};



#endif /* CAM_LINE_H_ */
