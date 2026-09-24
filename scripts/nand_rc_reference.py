"""Independent linear RC ladder transient reference for NAND model validation.

This solves C dv/dt = -G v through the symmetric tridiagonal matrix
C**(-1/2) G C**(-1/2). It does not call EvaCAM, fit its exponential, or fit
published energy. Units are ohms, farads, seconds, and volts throughout.

Orientation is ground -- R[0] -- node[0] -- R[1] -- node[1] -- ...,
with C[i] from node[i] to ground. A NAND discharge uses
R=[Rsource_select, *Rdata_source_to_drain, Rdrain_select] and
C=[Csource, *Cinternal_source_to_drain, Cbitline]. The observed bitline is
the last node. The usual initial condition is every node at Vprecharge.

Wire parasitics are omitted in the controlled NAND comparison. Additional
linear wire segments can be supplied explicitly; a chosen discretization is
then a separate assumption. This reference is not a transistor simulator.

For ideal bitline-driven precharge with the source select OPEN, reverse the
remaining physical network: R=[Rdrain_select, *reversed(Rdata)] and
C=[*reversed(Cinternal), Csource]. charging_voltages() models a stepped ideal
bitline source and initially discharged internal nodes. Its last node is the
physical source end. Finite precharge-driver impedance, gate transitions,
coupling, nonlinear MOS behavior, and source-select leakage are not included.
"""

from __future__ import annotations

import numpy as np
from scipy.linalg import eigh_tridiagonal, solve_banded


def _positive_vector(values, name):
    result = np.asarray(values, dtype=float)
    if result.ndim != 1 or result.size == 0:
        raise ValueError(f"{name} must be a nonempty one-dimensional array")
    if not np.all(np.isfinite(result)) or not np.all(result > 0):
        raise ValueError(f"{name} must contain finite positive values")
    return result.copy()


def elmore_last_node(resistances, capacitances):
    """Return the exact first moment for a uniform initial-voltage ladder.

    This is a moment identity, not a claim that exp(-t/moment) is its waveform.
    Every capacitance is weighted by its complete path resistance to ground.
    """
    resistances = _positive_vector(resistances, "resistances")
    capacitances = _positive_vector(capacitances, "capacitances")
    if resistances.shape != capacitances.shape:
        raise ValueError("resistances and capacitances must have equal lengths")
    moment = float(np.dot(np.cumsum(resistances), capacitances))
    if not np.isfinite(moment) or moment <= 0:
        raise ValueError("first moment is outside the finite positive range")
    return moment


class RcLadderReference:
    """Exact linear-network modal solution with positive shunt capacitances.

    Zero-capacitance nodes must first be eliminated by combining adjacent
    series resistors. No artificial capacitance or hidden regularization is
    inserted. Input arrays are copied to keep the solved network immutable.
    """

    def __init__(self, resistances, capacitances):
        resistance = _positive_vector(resistances, "resistances")
        capacitance = _positive_vector(capacitances, "capacitances")
        if resistance.shape != capacitance.shape:
            raise ValueError("resistances and capacitances must have equal lengths")
        self.resistances = resistance
        self.capacitances = capacitance
        self.resistances.flags.writeable = False
        self.capacitances.flags.writeable = False
        self._sqrt_capacitance = np.sqrt(capacitance)
        conductance = 1 / resistance
        diagonal = conductance.copy()
        diagonal[:-1] += conductance[1:]
        off_diagonal = -conductance[1:]
        scaled_diagonal = diagonal / capacitance
        scaled_off = off_diagonal / self._sqrt_capacitance[:-1] / self._sqrt_capacitance[1:]
        if not np.all(np.isfinite(scaled_diagonal)) or not np.all(np.isfinite(scaled_off)):
            raise ValueError("network coefficients exceed floating-point range")
        if resistance.size == 1:
            self._rates = scaled_diagonal.copy()
            self._modes = np.ones((1, 1))
        else:
            self._rates, self._modes = eigh_tridiagonal(
                scaled_diagonal, scaled_off, lapack_driver="stev"
            )
        if not np.all(np.isfinite(self._rates)) or not np.all(self._rates > 0):
            raise ValueError("network modal rates must be finite and positive")
        # Solve the static nodal equations independently of the eigensolver.
        banded = np.zeros((3, resistance.size))
        banded[0, 1:] = off_diagonal
        banded[1] = diagonal
        banded[2, :-1] = off_diagonal
        self._moments = solve_banded((1, 1), banded, capacitance)

    @property
    def first_moments(self):
        """Integral of each normalized voltage, obtained from G^-1 C 1."""
        return self._moments.copy()

    def _initial(self, initial_voltages, default):
        initial = (np.full(self.resistances.size, default) if initial_voltages is None
                   else np.asarray(initial_voltages, dtype=float))
        if initial.shape != self.resistances.shape or not np.all(np.isfinite(initial)):
            raise ValueError("initial_voltages must have one finite value per node")
        return initial

    def voltages(self, times, initial_voltages=None):
        """Return [time, node] free-decay voltages; defaults to uniform 1 V."""
        times = np.atleast_1d(np.asarray(times, dtype=float))
        if times.ndim != 1 or not np.all(np.isfinite(times)) or not np.all(times >= 0):
            raise ValueError("times must be a one-dimensional finite nonnegative array")
        initial = self._initial(initial_voltages, 1.0)
        weights = self._modes.T @ (self._sqrt_capacitance * initial)
        values = (np.exp(-np.outer(times, self._rates)) * weights) @ self._modes.T
        values /= self._sqrt_capacitance
        # Preserve the prescribed initial state exactly at t=0 instead of its
        # roundoff-level reconstruction through the orthonormal modal basis.
        values[times == 0] = initial
        return values

    def charging_voltages(self, times, rail_voltage, initial_voltages=None):
        """Voltages after stepping the grounded boundary to an ideal rail.

        Initial internal voltages default to zero. This is the complement of
        free decay because the DC equilibrium is uniform at rail_voltage.
        """
        if not np.isfinite(rail_voltage):
            raise ValueError("rail_voltage must be finite")
        initial = self._initial(initial_voltages, 0.0)
        return rail_voltage - self.voltages(times, rail_voltage - initial)
