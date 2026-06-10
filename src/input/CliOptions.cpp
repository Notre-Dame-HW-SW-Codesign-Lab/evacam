#include "input/CliOptions.h"

#include <omp.h>

#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>

CliOptions CliOptionsParser::Parse(int argc, char *argv[]) {
    CliOptions options;
    options.threads = omp_get_num_procs();

    if (argc <= 1) {
        throw std::invalid_argument("Missing configuration file.");
    }

    for (int i = 1; i < argc; i++) {
        const std::string_view arg(argv[i]);

        if (arg == "-h" || arg == "--help") {
            options.showHelp = true;
            return options;
        }

        if (arg == "-t" || arg == "--threads") {
            if (i + 1 >= argc) {
                throw std::invalid_argument("Missing thread count after " + std::string(arg));
            }
            try {
                options.threads = std::stoi(argv[++i]);
            } catch (const std::exception &) {
                throw std::invalid_argument("Number of threads must be an integer.");
            }
            if (options.threads <= 0) {
                throw std::invalid_argument("Number of threads must be positive.");
            }
            continue;
        }

        if (arg == "-v" || arg == "--verbose") {
            options.verbose = true;
            continue;
        }

        if (arg == "-o" || arg == "--output") {
            if (i + 1 >= argc) {
                throw std::invalid_argument("Missing output YAML file after " + std::string(arg));
            }
            if (!options.outputYamlFileName.empty()) {
                throw std::invalid_argument("Only one output YAML file may be provided.");
            }
            options.outputYamlFileName = argv[++i];
            if (options.outputYamlFileName.empty()) {
                throw std::invalid_argument("Output YAML file must not be empty.");
            }
            continue;
        }

        if (arg == "--no-variation-plots") {
            options.variationPlots = false;
            continue;
        }

        if (!arg.empty() && arg.front() == '-') {
            throw std::invalid_argument("Unknown option: " + std::string(arg));
        }

        if (!options.inputFileName.empty()) {
            throw std::invalid_argument("Only one configuration file may be provided.");
        }

        options.inputFileName = argv[i];
    }

    if (options.inputFileName.empty()) {
        throw std::invalid_argument("Missing configuration file.");
    }

    return options;
}

void CliOptionsParser::PrintUsage(std::ostream &os) {
    os << std::endl << "Usage: ./EvaCAM [OPTIONS] <cfg_file>" << std::endl << std::endl;
    os << "Options:" << std::endl;
    os << "  -t, --threads N           Number of parallel threads (default: all cores)" << std::endl;
    os << "  -o, --output FILE         Write YAML results to FILE" << std::endl;
    os << "  -v, --verbose             Enable verbose output" << std::endl;
    os << "      --no-variation-plots  Skip Monte Carlo variation histogram SVG generation" << std::endl;
    os << "  -h, --help                Show this help and exit" << std::endl;
}
