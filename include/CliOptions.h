#ifndef CLI_OPTIONS_H_
#define CLI_OPTIONS_H_

#include <iosfwd>
#include <string>

struct CliOptions {
    std::string inputFileName;
    std::string outputYamlFileName;
    int threads = 0;
    bool verbose = false;
    bool deepExploration = false;
    bool showHelp = false;
};

class CliOptionsParser {
    public:
        static CliOptions Parse(int argc, char *argv[]);
        static void PrintUsage(std::ostream &os);
};

#endif /* CLI_OPTIONS_H_ */
