#ifndef MEMCELL_H_
#define MEMCELL_H_

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sstream>
#include <bits/stdc++.h>
#include <yaml.h>

#include "typedef.h"
#include "constant.h"


// Helper YAML parser functions from ChatGPT 5.2, altered for functionality

static inline std::string kind(const YAML::Node& n) {
    if (!n) return "missing";
    if (n.IsNull()) return "null";
    if (n.IsScalar()) return "scalar";
    if (n.IsSequence()) return "sequence";
    if (n.IsMap()) return "map";
    return "other";
}

// Returns parent[key], with strong checks.
// - Doesn't mutate the tree (forces const operator[])
// - Errors if parent is missing or not a map
inline YAML::Node child_required(const YAML::Node& parent, const char* key) {
    if (!parent) {
        throw std::runtime_error(std::string("Parent node missing; cannot read key: ") + key);
    }
    if (!parent.IsMap()) {
        throw std::runtime_error(
            std::string("Cannot read key '") + key + "' from parent of type " + kind(parent) + " (expected map)"
        );
    }

    const YAML::Node cparent = parent; // force const operator[]
    YAML::Node child = cparent[key];

    if (!child) {
        // For missing keys, there is no exact location; parent mark is the best hint.
        std::ostringstream oss;
        oss << "Missing key: " << key;
        if (parent.Mark().line != -1) {
            oss << " (near line " << (parent.Mark().line + 1)
                << ", col " << (parent.Mark().column + 1) << ")";
        }
        throw std::runtime_error(oss.str());
    }
    return child;
}

// Convert a node to T, with good error text including line/col if available.
template <typename T>
T read_required(const YAML::Node& node, const char* what = "value") {
    if (!node) {
        throw std::runtime_error(std::string("Missing node for ") + what);
    }
    try {
        return node[what].as<T>();
    } catch (const YAML::Exception& e) {
        std::ostringstream oss;
        oss << "Bad conversion for " << what << ": " << e.what();
        if (e.mark.line != -1) {
            oss << " (line " << (e.mark.line + 1)
                << ", col " << (e.mark.column + 1) << ")";
        }
        throw std::runtime_error(oss.str());
    }
}

/*
static inline std::string join_path(const char* const* parts, size_t n) {
    std::ostringstream oss;
    for (size_t i = 0; i < n; ++i) {
        if (i) oss << '.';
        oss << parts[i];
    }
    return oss.str();
}

static inline std::string kind(const YAML::Node& n) {
    if (!n) return "missing";
    if (n.IsNull()) return "null";
    if (n.IsScalar()) return "scalar";
    if (n.IsSequence()) return "sequence";
    if (n.IsMap()) return "map";
    return "other";
}

// Returns the node at path relative to `root`.
// Does NOT assume root is the document root.
// Does NOT mutate (const operator[] at each step).
template <typename... Keys>
YAML::Node read_node_required(const YAML::Node& root, Keys... keys) {
    const char* path[] = { keys... };
    constexpr size_t N = sizeof...(keys);

    if (!root) {
        throw std::runtime_error("Root node is missing/null");
    }
    if constexpr (N == 0) {
        throw std::runtime_error("read_node_required needs at least one key");
    }

    YAML::Node n = root;

    for (size_t i = 0; i < N; ++i) {
        const char* k = path[i];

        if (!n) {
            throw std::runtime_error("Missing key: " + join_path(path, i));
        }
        if (!n.IsMap()) {
            throw std::runtime_error(
                "Key path " + join_path(path, i) +
                " failed: parent is " + kind(n) + " (expected map)"
            );
        }

        const YAML::Node cn = n;   // force const operator[]
        n = cn[k];

        if (!n) {
            throw std::runtime_error("Missing key: " + join_path(path, i + 1));
        }
    }

    return n;
}

template <typename T, typename... Keys>
T read_required(const YAML::Node& root, Keys... keys) {
    YAML::Node n = read_node_required(root, keys...);

    try {
        return n.as<T>();
    } catch (const YAML::Exception& e) {
        // Keep YAML's own message/mark, but add the path we were reading.
        const char* path[] = { keys... };
        std::ostringstream oss;
        oss << "While reading " << join_path(path, sizeof...(keys)) << ": " << e.what();
        if (e.mark.line != -1) {
            oss << " (line " << (e.mark.line + 1) << ", col " << (e.mark.column + 1) << ")";
        }
        throw std::runtime_error(oss.str());
    }
}

template <typename... Keys>
inline static std::string join_path(Keys... keys) {
    std::string out;
    ((out += (out.empty() ? "" : ".") + std::string(keys)), ...);
    return out;
}

inline const char* kind_of(const YAML::Node& n) {
    if (!n) return "missing";
    if (n.IsNull()) return "null";
    if (n.IsScalar()) return "scalar";
    if (n.IsSequence()) return "sequence";
    if (n.IsMap()) return "map";
    return "other";
}

template <typename T, typename... Keys>
inline T read_required_dbg(const YAML::Node& root, Keys... keys) {
    const char* path[] = { keys... };
    YAML::Node n = root;
    std::string full;

    for (const char* k : path) {
        if (!full.empty()) full += '.';
        full += k;

        const YAML::Node cn = n; // force const operator[]
        n = cn[k];

        if (!n) throw std::runtime_error("Missing key: " + full);
    }
    return n.as<T>();
}

template <typename T, typename... Keys>
inline T read_required(const YAML::Node& root, Keys... keys) {
    const char* path[] = { keys... };

    YAML::Node n = root;
    std::string full;

    for (const char* k : path) {
        if (!full.empty()) full += '.';
        full += k;

        if (!n) {
            throw std::runtime_error("Missing key: " + full);
        }
        if (!n.IsMap()) {
            throw std::runtime_error("Key path " + full +
                                     " failed because parent is " + kind_of(n) +
                                     " (expected map)");
        }

        n = n[k];
        if (!n) {
            throw std::runtime_error("Missing key: " + full);
        }
    }

    return n.as<T>();
    const char* path[] = { keys... };

    YAML::Node n = root;
    std::string full;

    // Walk the path safely, stopping immediately on a missing key.
    for (const char* k : path) {
        if (!full.empty()) full += '.';
        full += k;

        n = n[k];
        if (!n) {
            throw std::runtime_error("Missing key: " + full);
        }
    }

    // Conversion happens only after we know the node exists.
    try {
        return n.as<T>();
    } catch (const YAML::Exception& e) {
        // Keep YAML's mark/line info by rethrowing, but add context for you.
        std::cerr << "While reading " << full << ": " << e.what() << "\n";
        throw;
    }
    
    YAML::Node n = root;
    YAML::Mark last_mark;

    ((n = n[keys], last_mark = n ? n.Mark() : last_mark), ...);

    if (!n) {
        if (last_mark.line != -1) {
            throw YAML::BadConversion(last_mark);
        }
        throw std::runtime_error("Missing required key: " + join_path(keys...));
    }

    return n.as<T>();
}
*/

// Helper string functions from ChatGPT 5.2, altered so they actually work
inline std::size_t value_start_after_colon(const std::string& line)
{
    const auto pos = line.find(':');
    if (pos == std::string::npos)
        throw std::runtime_error("Missing ':'");

    std::size_t i = pos + 1;
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t'))
        ++i;

    if (i >= line.size())
        throw std::runtime_error("Missing value after ':'");

    return i;
}

inline bool starts_with(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() &&
           s.compare(0, prefix.size(), prefix) == 0;
}

inline std::string parse_string_after_colon(const std::string& line) {
    const std::size_t i = value_start_after_colon(line);
    auto tmp = line.substr(i);
    if (tmp.back() == '\n' || tmp.back() == '\r' || tmp.back() == ' ' || tmp.back() == '\t')
        tmp.pop_back();
    return tmp; // keep everything (including spaces in the value)
}

inline int parse_int_after_colon(const std::string& line) {
    const std::size_t i = value_start_after_colon(line);

    const char* begin = line.c_str() + i;
    char* end = nullptr;

    errno = 0;
    long v = std::strtol(begin, &end, 10);

    if (begin == end || errno != 0)
        throw std::runtime_error("Invalid integer");

    // Allow trailing whitespace only
    while (*end == ' ' || *end == '\t') ++end;
    //if (*end != '\0')
        //throw std::runtime_error("Trailing junk after integer");

    return static_cast<int>(v);
}

inline int parse_long_after_colon(const std::string& line) {
    const long i = value_start_after_colon(line);

    const char* begin = line.c_str() + i;
    char* end = nullptr;

    errno = 0;
    long v = std::strtol(begin, &end, 10);

    if (begin == end || errno != 0)
        throw std::runtime_error("Invalid long");

    // Allow trailing whitespace only
    while (*end == ' ' || *end == '\t') ++end;
    //if (*end != '\0')
        //throw std::runtime_error("Trailing junk after long");

    return static_cast<int64_t>(v);
}

inline double parse_double_after_colon(const std::string& line) {
    const std::size_t i = value_start_after_colon(line);

    const char* begin = line.c_str() + i;
    char* end = nullptr;

    errno = 0;
    double v = std::strtod(begin, &end);

    if (begin == end || errno != 0)
        throw std::runtime_error("Invalid double");

    // Allow trailing whitespace only
    while (*end == ' ' || *end == '\t') ++end;
    //if (*end != '\0')
        //throw std::runtime_error("Trailing junk after double");

    return v;
}

inline bool parse_bool_after_colon(const std::string& line) {
    std::string s = parse_string_after_colon(line);

    // trim trailing whitespace (common in config files)
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t'))
        s.pop_back();

    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return static_cast<char>(std::tolower(c)); });

    if (s == "true"  || s == "True" || s == "yes" || s == "Yes")  return true;
    if (s == "false" || s == "False" || s == "no"  || s == "No") return false;

    throw std::runtime_error("Invalid boolean");
}

class MemCell {
public:
	MemCell();
	MemCell(const MemCell&) {}
	virtual ~MemCell() {}

	/* Functions */
	void ReadCellFromFile(const std::string & inputFile, DesignTarget _designTarget, double _vdd);
	void PrintCell();
	// void CellScaling(int _targetProcessNode);
	double GetMemristance(double _relativeReadVoltage);  /* Get the LRS resistance of memristor at log-linear region of I-V curve */
	void CalculateWriteEnergy();
	double CalculateReadPower();

	/* Properties */
	MemCellType memCellType;	/* Memory cell type (like MRAM, PCRAM, etc.) */
	int processNode;        /* Cell original process technology node, Unit: nm*/
	double area;			/* Cell area, Unit: F^2 */
	double aspectRatio;		/* Cell aspect ratio, H/W */
	double widthInFeatureSize;	/* Cell width, Unit: F */
	double heightInFeatureSize;	/* Cell height, Unit: F */
	double resistanceOn;	/* Turn-on resistance */
	double resistanceOff;	/* Turn-off resistance */
	double capacitanceOn;   /* Cell capacitance when memristor is on */
	double capacitanceOff;  /* Cell capacitance when memristor is off */
	bool   readMode;		/* true = voltage-mode, false = current-mode */
	double readVoltage;		/* Read voltage */
	double readCurrent;		/* Read current */
	double minSenseVoltage; /* Minimum sense voltage */
        double wordlineBoostRatio; /*TODO: function not realized: ratio of boost wordline voltage to vdd */
	double readPower;       /* Read power per cell (uW)*/
	double readEnergy;      /* Read Energy per cell (fJ) */
	bool   resetMode;		/* true = voltage-mode, false = current-mode */
	double resetVoltage;	/* Reset voltage */
	double resetCurrent;	/* Reset current */
	double resetPulse;		/* Reset pulse duration (ns) */
	double resetEnergy;     /* Reset energy per cell (pJ) */
	bool   setMode;			/* true = voltage-mode, false = current-mode */
	double setVoltage;		/* Set voltage */
	double setCurrent;		/* Set current */
	double setPulse;		/* Set pulse duration (ns) */
	double setEnergy;       /* Set energy per cell (pJ) */
	CellAccessType accessType;	/* Cell access type: CMOS, BJT, or diode */

	int camNumRow;
	int camNumCol;
	CAMPort camPort[2][MAX_PORT];

        // No longer global
        DesignTarget designTarget;
        double vdd;


	double camWidthMatchTran;		/* The gate width of CMOS access transistor, Unit: F */
	CAMType camType; /* Ternary CAM, Multi-bit CAM, or Analog CAM */
	bool isNVMdischarge;
	int numResistanceState; // # of state of multi-bit CAM
	// double ResistanceValues[64]; // corresponding resistance values
	double ResistanceState[64];
	    
	bool withVariation;
	double resistanceOnVariation;
	double resistanceOffVariation;
	double resStateVariation[64];

	/* Optional properties */
	int stitching;			/* If non-zero, add stitching overhead for every x cells */
	double gateOxThicknessFactor; /* The oxide thickness of FBRAM could be larger than the traditional SOI MOS */
	double widthSOIDevice; /* The gate width of SOI device as FBRAM element, Unit: F*/
	double widthAccessCMOS;	/* The gate width of CMOS access transistor, Unit: F */
	double voltageDropAccessDevice;  /* The voltage drop on the access device, Unit: V */
	double leakageCurrentAccessDevice;  /* Reverse current of access device, Unit: uA */
	double capDRAMCell;		/* The DRAM cell capacitance if the memory cell is DRAM, Unit: F */
	double widthSRAMCellNMOS;	/* The gate width of NMOS in SRAM cells, Unit: F */
	double widthSRAMCellPMOS;	/* The gate width of PMOS in SRAM cells, Unit: F */

	/* For memristor */
	bool readFloating;      /* If unselected wordlines/bitlines are floating to reduce total leakage */
	double resistanceOnAtSetVoltage; /* Low resistance state when set voltage is applied */
	double resistanceOffAtSetVoltage; /* High resistance state when set voltage is applied */
	double resistanceOnAtResetVoltage; /* Low resistance state when reset voltage is applied */
	double resistanceOffAtResetVoltage; /* High resistance state when reset voltage is applied */
	double resistanceOnAtReadVoltage; /* Low resistance state when read voltage is applied */
	double resistanceOffAtReadVoltage; /* High resistance state when read voltage is applied */
	double resistanceOnAtHalfReadVoltage; /* Low resistance state when 1/2 read voltage is applied */
	double resistanceOffAtHalfReadVoltage; /* High resistance state when 1/2 read voltage is applied */
	double resistanceOnAtHalfResetVoltage; /* Low resistance state when 1/2 reset voltage is applied */

	/* For NAND flash */
	double flashEraseVoltage;		/* The erase voltage, Unit: V, highest W/E voltage in ITRS sheet */
	double flashPassVoltage;		/* The voltage applied on the unselected wordline within the same block during programming, Unit: V */
	double flashProgramVoltage;		/* The program voltage, Unit: V */
	double flashEraseTime;			/* The flash erase time, Unit: s */
	double flashProgramTime;		/* The SLC flash program time, Unit: s */
	double gateCouplingRatio;		/* The ratio of control gate to total floating gate capacitance */
};

#endif /* MEMCELL_H_ */
