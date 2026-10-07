#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>
#include <vector>

#include "model/NandNonlinearString.h"

NandNonlinearDcResult NandNonlinearString::Solve(
        const std::vector<NandNonlinearDevice> &devices, double sourceVoltage,
        double drainVoltage, const NandNonlinearDcOptions &options) {
    if (devices.empty() || devices.size() > 512 || !std::isfinite(sourceVoltage) ||
            !std::isfinite(drainVoltage) || !std::isfinite(options.absoluteCurrentTolerance) ||
            options.absoluteCurrentTolerance <= 0 || !std::isfinite(options.relativeCurrentTolerance) ||
            options.relativeCurrentTolerance < 0 || options.relativeCurrentTolerance >= 1 ||
            !std::isfinite(options.voltageTolerance) || options.voltageTolerance <= 0 ||
            options.maxIterations <= 0 || options.maxBacktracks < 0) {
        throw std::invalid_argument("Invalid NAND nonlinear DC inputs/options");
    }
    const size_t count = devices.size();
    const size_t unknowns = count - 1;
    NandNonlinearDcResult result;
    result.voltages.resize(count + 1);
    for (size_t i = 0; i <= count; ++i) {
        result.voltages[i] = sourceVoltage + (drainVoltage - sourceVoltage) * (double(i) / count);
    }
    result.voltages.front() = sourceVoltage;
    result.voltages.back() = drainVoltage;
    // Each cell must support the full passive interval traversed by the line search.
    for (const auto &device : devices) {
        device.model.Evaluate(device.gateVoltage, sourceVoltage, drainVoltage, device.threshold);
    }
    for (int iteration = 0; iteration < options.maxIterations; ++iteration) {
        std::vector<NandCellCurrentResult> cells;
        for (size_t i = 0; i < count; ++i) {
            const auto &device = devices[i];
            cells.push_back(device.model.Evaluate(device.gateVoltage, result.voltages[i],
                    result.voltages[i + 1], device.threshold));
        }
        result.current = cells.back().current;
        double currentScale = 0;
        for (const auto &cell : cells) currentScale = std::max(currentScale, std::abs(cell.current));
        std::vector<std::vector<double>> matrix(unknowns, std::vector<double>(unknowns, 0));
        std::vector<double> correction(unknowns);
        result.maximumKclResidual = 0;
        for (size_t i = 0; i < unknowns; ++i) {
            const double residual = cells[i].current - cells[i + 1].current;
            result.maximumKclResidual = std::max(result.maximumKclResidual, std::abs(residual));
            correction[i] = -residual;
            matrix[i][i] = cells[i].drainDerivative - cells[i + 1].sourceDerivative;
            if (i > 0) matrix[i][i - 1] = cells[i].sourceDerivative;
            if (i + 1 < unknowns) matrix[i][i + 1] = -cells[i + 1].drainDerivative;
        }
        // Partial pivoting; deliberately independent of the linear RC solver.
        for (size_t col = 0; col < unknowns; ++col) {
            size_t pivot = col;
            for (size_t row = col + 1; row < unknowns; ++row) {
                if (std::abs(matrix[row][col]) > std::abs(matrix[pivot][col])) pivot = row;
            }
            if (!std::isfinite(matrix[pivot][col]) || matrix[pivot][col] == 0) {
                throw std::runtime_error("NAND nonlinear DC singular Jacobian");
            }
            std::swap(matrix[col], matrix[pivot]);
            std::swap(correction[col], correction[pivot]);
            for (size_t row = col + 1; row < unknowns; ++row) {
                const double factor = matrix[row][col] / matrix[col][col];
                if (factor == 0) continue;
                for (size_t k = col + 1; k < unknowns; ++k) matrix[row][k] -= factor * matrix[col][k];
                matrix[row][col] = 0;
                correction[row] -= factor * correction[col];
            }
        }
        result.maximumVoltageCorrection = 0;
        for (size_t reverse = unknowns; reverse > 0; --reverse) {
            const size_t row = reverse - 1;
            for (size_t col = row + 1; col < unknowns; ++col) correction[row] -= matrix[row][col] * correction[col];
            correction[row] /= matrix[row][row];
            if (!std::isfinite(correction[row])) throw std::runtime_error("NAND nonlinear DC nonfinite correction");
            result.maximumVoltageCorrection = std::max(result.maximumVoltageCorrection, std::abs(correction[row]));
        }
        result.iterations = iteration + 1;
        if (result.maximumKclResidual <= options.absoluteCurrentTolerance +
                    options.relativeCurrentTolerance * currentScale &&
                result.maximumVoltageCorrection <= options.voltageTolerance) return result;

        bool accepted = false;
        double damping = 1;
        for (int trial = 0; trial <= options.maxBacktracks; ++trial) {
            std::vector<double> candidate = result.voltages;
            bool inDomain = true;
            for (size_t i = 0; i < unknowns; ++i) {
                candidate[i + 1] += damping * correction[i];
                inDomain = inDomain && std::isfinite(candidate[i + 1]) &&
                        candidate[i + 1] >= std::min(sourceVoltage, drainVoltage) &&
                        candidate[i + 1] <= std::max(sourceVoltage, drainVoltage);
            }
            if (inDomain) {
                double residual = 0;
                double previousCurrent = 0;
                for (size_t i = 0; i < count; ++i) {
                    const auto &device = devices[i];
                    const double current = device.model.Evaluate(device.gateVoltage, candidate[i],
                            candidate[i + 1], device.threshold).current;
                    if (i > 0) residual = std::max(residual, std::abs(previousCurrent - current));
                    previousCurrent = current;
                }
                if (residual <= (1 - 1e-4 * damping) * result.maximumKclResidual) {
                    result.voltages = std::move(candidate);
                    accepted = true;
                    break;
                }
            }
            if (trial < options.maxBacktracks) ++result.backtracks;
            damping *= 0.5;
        }
        if (!accepted) throw std::runtime_error("NAND nonlinear DC line search failed");
    }
    throw std::runtime_error("NAND nonlinear DC iteration limit reached");
}
