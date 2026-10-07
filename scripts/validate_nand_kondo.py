#!/usr/bin/env python3
"""Offline Kondo/Tanzawa SLM benchmark. No fitted circuit parameters.

Publication observations live separately under docs/validation/data/kondo-2022.
The C++ production solver is compared with an independent dense spectral
matrix exponential assembled from connectivity. Ambiguity is never a pass.
"""
from __future__ import annotations

import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import os
from functools import lru_cache
from pathlib import Path
import subprocess
import sys

import numpy as np
from scipy.linalg import eigh
import yaml

ROOT = Path(__file__).resolve().parents[1]
REFERENCE = ROOT / 'docs/validation/nand-kondo-2022.reference.yaml'
DATA = ROOT / 'docs/validation/data/kondo-2022'
PROBE = ROOT / 'test-bin/NandRcLadderProbe'
POSITIONS = (.25, .33, .5, .66, .75, 1.)


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def load_reference():
    manifest = yaml.safe_load(REFERENCE.read_text())
    for name, digest in manifest['data_hashes'].items():
        if sha256(DATA / name) != digest:
            raise ValueError(f'reference hash mismatch: {name}')
    curves = json.loads((DATA / 'curves.json').read_text())['curves']
    extraction = json.loads((DATA / 'extraction.json').read_text())
    ids = set()
    for curve in curves:
        if curve['id'] in ids or not curve['points']:
            raise ValueError('duplicate or empty curve')
        ids.add(curve['id'])
        axis = extraction['axes'][curve['axis']]
        x0, y0, x1, y1 = axis['box']
        xmin, ymin, xmax, ymax = axis['limits']
        for point in curve['points']:
            x, y = point['pixel']
            expected = [(x-x0)/(x1-x0)*(xmax-xmin)+xmin,
                        (y-y0)/(y1-y0)*(ymax-ymin)+ymin]
            if not np.all(np.isfinite([*expected, point['x'], point['y']])):
                raise ValueError('nonfinite reference coordinate')
            if not np.allclose(expected, [point['x'], point['y']], rtol=1e-12, atol=1e-20):
                raise ValueError('axis conversion mismatch')
    return manifest, curves, extraction


def build_line(segments, position, data):
    """Conservative finite volumes; keep the open line beyond an interior tap."""
    if not isinstance(segments, int) or segments < 2 or segments > 2048:
        raise ValueError('segments must be an integer between 2 and 2048')
    if not np.isfinite(position) or not 0 < position <= 1 or data not in (0, 1):
        raise ValueError('invalid cell position or data')
    # Rounding merges coincident mesh/tap nodes; never rounds the tap to a grid index.
    x = np.unique(np.round(np.r_[np.linspace(0, 1, segments+1), position], 14))
    dx = np.diff(x)
    cap = 3e-12 * (dx + np.r_[dx[1:], 0]) / 2
    tap = int(np.argmin(abs(x[1:] - position)))
    resistance = 50e6 if data == 0 else 5e6
    shunts = np.zeros(len(cap)); shunts[tap] = 1/resistance
    return dict(capacitances_f=cap.tolist(), series_resistances_ohm=(1e6*dx[1:]).tolist(),
                shunt_conductances_s=shunts.tolist(), initial_voltages_v=[0.]*len(cap),
                positions=x[1:].tolist(), source_capacitance_f=float(3e-12*dx[0]/2),
                left_resistance_ohm=float(1e6*dx[0]), tap=tap,
                cell_resistance_ohm=resistance, position=position, data=data,
                solver=dict(max_step_s=20e-9, tolerance_v=1e-10, max_steps=1000000))


class LineReference:
    """Dense connectivity assembly + symmetric matrix exponential, no time steps."""
    def __init__(self, circuit):
        self.circuit = circuit
        self.cap = np.asarray(circuit['capacitances_f'])
        n = len(self.cap)
        incidence = np.zeros((n-1, n))
        for edge in range(n-1):
            incidence[edge, edge:edge+2] = [1, -1]
        g = incidence.T @ np.diag(1/np.asarray(circuit['series_resistances_ohm'])) @ incidence
        g += np.diag(circuit['shunt_conductances_s'])
        self.drive = np.zeros(n); self.drive[0] = 1/circuit['left_resistance_ohm']
        g[0, 0] += self.drive[0]
        self.g = g
        self.steady = np.linalg.solve(g, self.drive)
        root = np.sqrt(self.cap)
        self.rates, modes = eigh(g / root[:, None] / root[None, :])
        self.basis = modes/root[:, None]
        self.coefficients = modes.T @ (root*self.steady)
        self.tap = circuit['tap']

    def state(self, times, pulse):
        times = np.atleast_1d(times).astype(float)
        if np.any(~np.isfinite(times)) or np.any(times < 0) or not np.isfinite(pulse) or pulse < 0:
            raise ValueError('times and pulse must be finite and nonnegative')
        amplitude = .5 if pulse == 0 else .6
        exponential = np.exp(-np.outer(times, self.rates))
        state = amplitude * (self.steady - (exponential*self.coefficients) @ self.basis.T)
        if pulse > 0:
            after = times >= pulse
            state[after] -= .1 * (self.steady -
                (np.exp(-np.outer(times[after]-pulse, self.rates))*self.coefficients) @ self.basis.T)
        return state

    def observe(self, times, pulse, side='right'):
        times = np.atleast_1d(times)
        if np.any(~np.isfinite(times)) or np.any(times < 0) or not np.isfinite(pulse) or pulse < 0:
            raise ValueError('times and pulse must be finite and nonnegative')
        weights = self.basis[[0, self.tap], :].T * self.coefficients[:, None]
        amplitude = .5 if pulse == 0 else .6
        state = amplitude * (self.steady[[0,self.tap]] - np.exp(-np.outer(times,self.rates)) @ weights)
        if pulse > 0:
            after = times >= pulse
            state[after] -= .1 * (self.steady[[0,self.tap]] - np.exp(-np.outer(times[after]-pulse,self.rates)) @ weights)
        source = np.where((times < pulse) | ((times == pulse) & (side == 'left')), .6, .5)
        if pulse == 0: source[:] = .5
        return dict(voltage=state[:, 1],
                    cell_current=state[:, 1]/self.circuit['cell_resistance_ohm'],
                    sense_current=(source-state[:, 0])/self.circuit['left_resistance_ohm'])

    def targets(self):
        current = .5/(1e6*self.circuit['position']+self.circuit['cell_resistance_ohm'])
        return dict(voltage=current*self.circuit['cell_resistance_ohm'], sense_current=current)


def windows(circuit, convention='loaded'):
    current = .5/(1e6*circuit['position']+circuit['cell_resistance_ohm'])
    voltage = current*circuit['cell_resistance_ohm']
    if convention == 'nominal':
        voltage = .5; current = .5/circuit['cell_resistance_ohm']
    elif convention not in ('loaded', 'data0_upper_only'):
        raise ValueError('unknown measurement convention')
    return dict(voltage=(.9*voltage, 1.1*voltage),
                sense_current=((float('-inf'), .5/circuit['cell_resistance_ohm'])
                    if convention == 'data0_upper_only' and circuit['data']==0
                    else (.9*current, 1.1*current)))


def settling(times, values, window, tail_certified):
    """Last entry with a retained bracket; duplicates are left/right transition limits."""
    t = np.asarray(times); v = np.asarray(values)
    if (len(t) != len(v) or len(t) < 2 or np.any(~np.isfinite(t)) or
            np.any(~np.isfinite(v)) or np.any(np.diff(t) < 0) or window[0] > window[1]):
        raise ValueError('invalid settling observations')
    outside = np.flatnonzero((v < window[0]) | (v > window[1]))
    if not tail_certified or (len(outside) and outside[-1] == len(t)-1):
        return dict(status='unresolved', time_s=None, bracket_s=None)
    if not len(outside):
        return dict(status='settled', time_s=float(t[0]), bracket_s=[float(t[0])]*2)
    i = int(outside[-1]); lo, hi = float(t[i]), float(t[i+1])
    boundary = window[0] if v[i] < window[0] else window[1]
    fraction = (boundary-v[i])/(v[i+1]-v[i])
    return dict(status='settled', time_s=lo+(hi-lo)*float(fraction), bracket_s=[lo, hi])


def observation_times(pulse, spacing=20e-9, horizon=30e-6, extras=()):
    if spacing <= 0 or horizon <= pulse or pulse < 0:
        raise ValueError('invalid observation schedule')
    # Dense through the observable transient, then a sparse certified-tail check.
    dense_end = min(horizon, max(15e-6, pulse+8e-6))
    t = np.unique(np.round(np.r_[np.arange(0, dense_end, spacing), dense_end, horizon, pulse, extras],18))
    return t[(t >= 0) & (t <= horizon)]


def tail_is_safe(state, circuit, observable, window):
    x = np.asarray(circuit['positions'])
    dc_current = .5/(1e6*circuit['position']+circuit['cell_resistance_ohm'])
    dc = .5-dc_current*1e6*np.minimum(x, circuit['position'])
    # Maximum principle for the passive error system bounds every future node
    # error by the current maximum. No assumption of monotonic observed current.
    bound = float(np.max(abs(np.asarray(state)-dc)))
    target = dc[circuit['tap']]
    if observable == 'sense_current':
        bound /= circuit['left_resistance_ohm']; target = dc_current
    return bool(window[0] <= target-bound and target+bound <= window[1])


def reference_trace(reference, pulse, times):
    t = np.asarray(times)
    # Keep both sides of the source transition, even when a sample is at pulse.
    if pulse > 0:
        t = np.sort(np.r_[t, pulse])
    obs = reference.observe(t, pulse)
    if pulse > 0:
        index = int(np.flatnonzero(t == pulse)[0])
        obs['sense_current'][index] = reference.observe([pulse], pulse, 'left')['sense_current'][0]
    return t, obs, reference.state([t[-1]], pulse)[0]


def run_probe(circuit, pulse, times, directory, name='circuit'):
    payload = {k: circuit[k] for k in ('capacitances_f','series_resistances_ohm',
               'shunt_conductances_s','initial_voltages_v','solver')}
    payload['observed_nodes'] = [0, circuit['tap'], len(circuit['capacitances_f'])-1]
    phases = []
    for start, end, voltage in ([(0, pulse, .6), (pulse, float(times[-1]), .5)] if pulse > 0
                                 else [(0, float(times[-1]), .5)]):
        selected = np.unique(np.r_[0, np.asarray(times)[(times>start)&(times<end)]-start, end-start])
        phases.append(dict(duration_s=end-start, observation_times_s=selected.tolist(),
                           left=dict(connected=True, resistance_ohm=circuit['left_resistance_ohm'], voltage_v=voltage)))
    payload['phases'] = phases
    path = Path(directory)/f'{name}.yaml'; path.write_text(yaml.safe_dump(payload))
    process = subprocess.run([str(PROBE), str(path)], text=True, capture_output=True, timeout=120)
    if process.returncode:
        raise RuntimeError('unresolved production trajectory: '+process.stderr.strip())
    result = yaml.load(process.stdout, Loader=getattr(yaml,'CSafeLoader',yaml.SafeLoader))
    # yaml-cpp emits valid YAML 1.2 numbers such as 2e-08. PyYAML's default
    # YAML 1.1 resolver leaves those as strings without a decimal point.
    result['final_voltages_v'] = list(map(float,result['final_voltages_v']))
    for sample in result['samples']:
        for key,value in sample.items():
            sample[key] = list(map(float,value)) if isinstance(value,list) else float(value)
    samples = result['samples']; t = np.array([s['time_s'] for s in samples])
    v = np.array([s['voltages_v'][1] for s in samples])
    obs = dict(voltage=v, cell_current=v/circuit['cell_resistance_ohm'],
               sense_current=np.array([s['left_current_a'] for s in samples]))
    result['source_endpoint_switch_charge_c'] = circuit['source_capacitance_f']*.5
    result['source_endpoint_capacitor_energy_change_j'] = .5*circuit['source_capacitance_f']*.5**2
    result['source_endpoint_switches'] = [dict(time_s=0.,charge_c=circuit['source_capacitance_f']*(.5 if pulse==0 else .6))]
    if pulse>0:
        result['source_endpoint_switches'].append(dict(time_s=pulse,charge_c=-.1*circuit['source_capacitance_f']))
    # Only capacitor charge and stored energy are defined at an ideal imposed
    # step. Supply energy needs a specified switching path; do not invent one.
    result['source_endpoint_supply_energy_j'] = None
    (Path(directory)/f'{name}.result.json').write_text(json.dumps(result, separators=(',',':'))+'\n')
    return t, obs, result


def measure_trace(times, observations, state, circuit, convention='loaded'):
    return {key:settling(times, observations[key], bounds,
                         tail_is_safe(state,circuit,key,bounds))
            for key,bounds in windows(circuit,convention).items()}


def evaluate_case(circuit, pulse, directory, name, spacing=20e-9, horizon=30e-6):
    reference = LineReference(circuit)
    times = observation_times(pulse, spacing, horizon)
    # Explicitly extend the horizon until a passive-network tail bound certifies
    # the primary windows; hard limit returns unresolved instead of a fake delay.
    while horizon < 240e-6:
        state = reference.state([horizon],pulse)[0]
        if all(tail_is_safe(state,circuit,k,b) for k,b in windows(circuit).items()): break
        horizon *= 2; times = observation_times(pulse,spacing,horizon)
    t, obs, raw = run_probe(circuit,pulse,times,directory,name)
    extra = []
    # Refine every reported convention. A one-sided near-DC target can settle
    # well after the primary 10% window; a sparse tail interval is not a timing
    # estimate for that alternative.
    for convention in ('loaded','nominal','data0_upper_only'):
        measurements = measure_trace(t,obs,raw['final_voltages_v'],circuit,convention)
        for result in measurements.values():
            if result['bracket_s']:
                a,b = result['bracket_s']; extra.extend(np.linspace(a,b,max(2,int(np.ceil((b-a)/1e-9))+1)))
    if extra:
        times = np.unique(np.round(np.r_[times,extra],18))
        t,obs,raw = run_probe(circuit,pulse,times,directory,name)
    exact = reference.observe(t,pulse)
    # Probe includes a pre-transition sample followed by the post-transition.
    if pulse > 0:
        index = np.flatnonzero(np.isclose(t,pulse,rtol=0,atol=1e-20))
        if len(index): exact['sense_current'][index[0]] = reference.observe([pulse],pulse,'left')['sense_current'][0]
    measurements = {convention:measure_trace(t,obs,raw['final_voltages_v'],circuit,convention)
                    for convention in ('loaded','nominal','data0_upper_only')}
    exact_measure = measure_trace(t,exact,reference.state([t[-1]],pulse)[0],circuit)
    errors = {key:dict(maximum=float(np.max(abs(obs[key]-exact[key]))),
                      rmse=float(np.sqrt(np.mean((obs[key]-exact[key])**2)))) for key in obs}
    timing_errors = {}
    for key in ('voltage','sense_current'):
        a=measurements['loaded'][key]['time_s']; b=exact_measure[key]['time_s']
        timing_errors[key] = None if a is None or b is None else a-b
    return dict(position=circuit['position'],data=circuit['data'],pulse_s=pulse,
                measurements=measurements,reference_measurements=exact_measure,
                numerical_error=errors,numerical_timing_error_s=timing_errors,
                attempted_steps=raw['attempted_steps'],segments=len(circuit['capacitances_f']),
                source_endpoint_switch_charge_c=raw['source_endpoint_switch_charge_c']), (t,obs,exact)


@lru_cache(maxsize=20000)
def independent_delays(reference, pulse, convention='loaded', spacing=20e-9):
    times = observation_times(pulse,spacing)
    t,obs,state = reference_trace(reference,pulse,times)
    result = measure_trace(t,obs,state,reference.circuit,convention)
    extras = []
    for measured in result.values():
        if measured['bracket_s']:
            a,b=measured['bracket_s'];extras.extend(np.linspace(a,b,max(2,int(np.ceil((b-a)/.5e-9))+1)))
    t,obs,state=reference_trace(reference,pulse,np.unique(np.r_[times,extras]))
    return measure_trace(t,obs,state,reference.circuit,convention)


def residual_metrics(observed, predicted, uncertainty, full_scale, timing=False):
    observed=np.asarray(observed); predicted=np.asarray(predicted); uncertainty=np.asarray(uncertainty)
    residual=predicted-observed
    allowance=uncertainty+.05*(np.abs(observed) if timing else full_scale)
    return dict(point_count=len(observed),signed_residuals=residual.tolist(),
                bias=float(np.mean(residual)),rmse=float(np.sqrt(np.mean(residual**2))),
                maximum_absolute_error=float(np.max(abs(residual))),
                maximum_relative_error=float(np.max(abs(residual)/np.maximum(abs(observed),1e-9))) if timing else None,
                allowed_error=allowance.tolist(),points_within_rule=int(np.sum(abs(residual)<=allowance)),
                within_rule=bool(np.all(abs(residual)<=allowance)))


def plot_report(output, curves, comparisons, sweeps, waveforms, convergence):
    os.environ.setdefault('MPLCONFIGDIR',str(output/'matplotlib-cache'))
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    plt.rcParams.update({'font.size':10,'axes.grid':True,'grid.alpha':.2})
    def save(fig,name):
        fig.tight_layout(); fig.savefig(output/(name+'.png'),dpi=170); fig.savefig(output/(name+'.svg')); plt.close(fig)
    fig,axs=plt.subplots(2,3,figsize=(13,7),sharex=True)
    for (position,pulse),(t,obs,exact) in waveforms.items():
        row=0 if position==.25 else 1; col=[2.,2.4,2.8].index(round(pulse*1e6,1)); ax=axs[row,col]
        for key,color in [('sense_current','#c04c22'),('cell_current','#226bb0')]:
            ax.plot(t*1e6,obs[key]*1e9,color=color,label='C++ '+key.replace('_',' '))
            ax.plot(t*1e6,exact[key]*1e9,color='black',ls=':',lw=.7)
        for curve in curves:
            case=curve['case']
            if curve['observable'] in ('sense_current','cell_current') and case.get('position')==position and abs(case.get('pulse_s',0)-pulse)<1e-15:
                ax.errorbar([p['x']*1e6 for p in curve['points']],[p['y']*1e9 for p in curve['points']],yerr=[p['y_uncertainty']*1e9 for p in curve['points']],fmt='o',ms=3,color='#c04c22' if curve['observable']=='sense_current' else '#226bb0',mfc='white')
        ax.set(xlim=(0,10),ylim=(-40,200),title=f'Cell at {position:.0%}; pulse {pulse*1e6:g} us',xlabel='Time (us)',ylabel='Current (nA)')

    if axs[0,0].get_legend_handles_labels()[0]: axs[0,0].legend(fontsize=8)
    fig.suptitle('Figures 6/7: C++ transient, independent reference (dotted), publication points')
    save(fig,'current-waveforms')
    fig,axs=plt.subplots(1,2,figsize=(12,4.8))
    for curve in curves:
        if not curve['id'].startswith('fig3a'):continue
        comp=comparisons[curve['id']]
        x=np.array([p['x'] for p in curve['points']])*1e6; y=np.array([p['y'] for p in curve['points']])*1e3
        line,=axs[0].plot(x,np.array(comp['predicted'])*1e3,lw=.8,label=f"{curve['case']['pulse_s']*1e6:g} us" if curve['id']!='fig3a_common_rise' else None)
        axs[0].plot(x,y,'o',ms=3,color=line.get_color(),mfc='white')
        axs[0].plot(x,np.array(comp['alternative'])*1e3,lw=.8,ls='--',color=line.get_color())
        axs[1].plot(x,(np.array(comp['predicted'])-np.array([p['y'] for p in curve['points']]))*1e3,'o-',ms=3,color=line.get_color())
    if axs[0].get_legend_handles_labels()[0]: axs[0].legend(fontsize=7,ncol=2)
    axs[0].set(title='Figure 3a: data 0 (solid), data 1 (dashed)',xlabel='Time (us)',ylabel='Cell voltage (mV)')
    axs[1].set(title='Signed residual: model minus publication',xlabel='Time (us)',ylabel='Voltage error (mV)'); save(fig,'voltage-waveforms')
    fig,axs=plt.subplots(1,3,figsize=(15,4.5))
    for curve in curves:
        if not curve['observable'].endswith('_delay'):continue
        panel=2 if curve['case'].get('aggregate') else 0 if curve['observable']=='voltage_delay' else 1
        ax=axs[panel]; comp=comparisons[curve['id']]
        x=np.array([p['x'] for p in curve['points']])*1e6; y=np.array([p['y'] for p in curve['points']])*1e6
        line,=ax.plot(x,np.array(comp['predicted'])*1e6,lw=1,label=curve['id'].replace('fig3b_','').replace('fig4a_','').replace('fig4b_',''))
        ax.plot(x,y,'o',ms=3,mfc='white',color=line.get_color())
    for ax,title in zip(axs,['Figure 3b: voltage settling','Figure 4a: sensed-current settling','Figure 4b: worst of 12 cases']):
        ax.set(title=title,xlabel='Pulse width (us)',ylabel='Settling time (us)')
        if ax.get_legend_handles_labels()[0]: ax.legend(fontsize=7)
    save(fig,'delay-comparisons')
    fig,axs=plt.subplots(1,2,figsize=(11,4.5))
    for convention,style in [('loaded','-'),('data0_upper_only','--')]:
        for key,color in [('voltage','#226bb0'),('sense_current','#c04c22')]:
            pulses=sorted(set(s['pulse_s'] for s in sweeps)); values=[]
            for pulse in pulses:
                rows=[s['measurements'][convention][key]['time_s'] for s in sweeps if s['pulse_s']==pulse]
                values.append(max(rows) if all(v is not None for v in rows) else np.nan)
            axs[0].plot(np.array(pulses)*1e6,np.array(values)*1e6,style,color=color,label=f'{key}, {convention}')
    axs[0].set(title='Sensitivity to data-0 settling convention',xlabel='Pulse width (us)',ylabel='Worst settling (us)');axs[0].legend(fontsize=7)
    for key in ('voltage','sense_current'):
        axs[1].plot([r['segments'] for r in convergence['mesh']],[r[key]*1e6 for r in convergence['mesh']],'o-',label=key)
    axs[1].set(title='Mesh convergence: far cell, data 1, pulse 2 us',xlabel='Nominal line segments',ylabel='Settling time (us)');axs[1].legend(fontsize=8)
    save(fig,'convergence-and-definitions')


def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=ROOT/'output/validation/nand-kondo-2022')
    parser.add_argument('--segments',type=int,default=96)
    parser.add_argument('--require-reproduction',action='store_true')
    parser.add_argument('--jobs',type=int,default=4)
    args=parser.parse_args(argv)
    if not 1 <= args.jobs <= 16:
        parser.error('--jobs must be between 1 and 16')
    if not 2 <= args.segments <= 1024:
        parser.error('--segments must be between 2 and 1024 (refinement doubles it)')
    manifest,curves,extraction=load_reference()
    output=args.output; output.mkdir(parents=True,exist_ok=True)
    inputs=output/'inputs'; inputs.mkdir(exist_ok=True)
    if not PROBE.is_file(): raise RuntimeError('build with make nand-rc-ladder-probe')
    # Published pulse grid plus the narrow 2.3 us optimum plotted in Fig. 4.
    pulses=np.unique(np.r_[np.arange(0,5.0001e-6,.2e-6),2.3e-6]); pulses=np.round(pulses,14)
    sweeps=[]; waveforms={}; trace_cache={}
    print(f'Running {len(pulses)*12} compiled SLM cases with {args.segments} nominal segments',flush=True)
    with ThreadPoolExecutor(max_workers=args.jobs) as executor:
        for position in POSITIONS:
            for data in (0,1):
                circuit=build_line(args.segments,position,data)
                futures=[]
                for pulse in pulses:
                    name=f'x{position:g}_d{data}_p{pulse*1e6:.1f}'
                    futures.append(executor.submit(evaluate_case,circuit,float(pulse),inputs,name))
                for pulse,future in zip(pulses,futures):
                    row,trace=future.result()
                    sweeps.append(row);trace_cache[(position,data,round(float(pulse),14))]=trace
                    if data==1 and position in (.25,1.) and round(float(pulse)*1e6,1) in (2.,2.4,2.8):
                        waveforms[(position,float(pulse))]=trace
                print(f'  finished x={position:g}, data={data}',flush=True)
    references={}
    def ref(position,data,segments=None):
        key=(position,data,segments or args.segments)
        if key not in references: references[key]=LineReference(build_line(key[2],position,data))
        return references[key]
    convergence={'mesh':[],'integration':[],'maximum_step':[],'observation':[],'position_interpretation':[]}
    for segments in (24,48,96,192,384):
        delays=independent_delays(ref(1.,1,segments),2e-6)
        convergence['mesh'].append(dict(segments=segments,**{k:v['time_s'] for k,v in delays.items()}))
    for tolerance in (1e-7,1e-9,1e-11):
        circuit=build_line(args.segments,.25,1);circuit['solver']['tolerance_v']=tolerance
        row,_=evaluate_case(circuit,2.4e-6,inputs,f'tolerance_{tolerance}')
        convergence['integration'].append(dict(tolerance_v=tolerance,**row))
    for step in (100e-9,20e-9,5e-9):
        circuit=build_line(args.segments,1.,1);circuit['solver']['max_step_s']=step
        row,_=evaluate_case(circuit,2e-6,inputs,f'maxstep_{step}',spacing=200e-9)
        convergence['maximum_step'].append(dict(max_step_s=step,**row))
    for spacing in (40e-9,10e-9,2e-9):
        row,_=evaluate_case(build_line(args.segments,.25,1),2.4e-6,inputs,f'observation_{spacing}',spacing=spacing)
        convergence['observation'].append(dict(spacing_s=spacing,**row))
    for printed,alternative in ((.33,1/3),(.66,2/3)):
        for data in (0,1):
            primary=independent_delays(ref(printed,data),2.4e-6)
            other=independent_delays(ref(alternative,data),2.4e-6)
            convergence['position_interpretation'].append(dict(printed=printed,alternative=alternative,data=data,
                difference_s={k:other[k]['time_s']-primary[k]['time_s'] for k in primary}))
    # Literature predictions use compiled traces. Timing points are selected at
    # the declared pulse grid (axis-pick x uncertainty retained separately).
    comparisons={}
    for curve in curves:
        case=curve['case']; observable=curve['observable']; pts=curve['points']
        predicted=[]; uncertainties=[]; mesh_errors=[]; alternate=[]; budgets=[]
        for point in pts:
            if observable.endswith('_delay'):
                pulse=float(pulses[np.argmin(abs(pulses-point['x']))]);key='voltage' if observable=='voltage_delay' else 'sense_current'
                selected=[r for r in sweeps if abs(r['pulse_s']-pulse)<1e-14 and (case.get('aggregate') or (r['position']==case['position'] and r['data']==case['data']))]
                vals=[r['measurements']['loaded'][key]['time_s'] for r in selected]
                predicted.append(max(vals) if all(v is not None for v in vals) else float('nan'))
                alt=[r['measurements']['data0_upper_only'][key]['time_s'] for r in selected]
                alternate.append(max(alt) if all(v is not None for v in alt) else float('nan'))
                # Independent mesh refinement for every compared timing point.
                refined=[]; shifted=[]
                for row in selected:
                    rf=ref(row['position'],row['data'],args.segments*2)
                    refined.append(independent_delays(rf,pulse)[key]['time_s'])
                    for p in (max(0,pulse-point['x_uncertainty']),pulse+point['x_uncertainty']):
                        shifted.append(independent_delays(ref(row['position'],row['data']),p)[key]['time_s'])
                coarse=max(independent_delays(ref(r['position'],r['data']),pulse)[key]['time_s'] for r in selected)
                mesh=abs(max(refined)-coarse) if all(v is not None for v in refined) else float('nan')
                # Worst-case sensitivity across the selected cases and both x bounds.
                if case.get('aggregate'):
                    left=max(shifted[::2]);right=max(shifted[1::2]); xs=max(abs(left-predicted[-1]),abs(right-predicted[-1]))
                else: xs=max(abs(v-predicted[-1]) for v in shifted)
                mesh_errors.append(mesh)
                budget=dict(digitization=point['y_uncertainty'],abscissa_sensitivity=xs,mesh=mesh,integration=5e-9,event_bracket=1e-9)
                budgets.append(budget);uncertainties.append(sum(budget.values()))
            else:
                position=case['position'];data=case['data'];pulse=round(case['pulse_s'],14)
                t,obs,_=trace_cache[(position,data,pulse)]
                # Requested points avoid source switching; interpolation only
                # resamples model values and never creates reference observations.
                predicted.append(float(np.interp(point['x'],t,obs[observable])))
                rf=ref(position,data,args.segments*2)
                reference_value=ref(position,data).observe([point['x']],pulse)[observable][0]
                mesh=abs(rf.observe([point['x']],pulse)[observable][0]-reference_value)
                resampling=abs(float(np.interp(point['x'],t,trace_cache[(position,data,pulse)][2][observable]))-reference_value)
                shift=ref(position,data).observe([max(0,point['x']-point['x_uncertainty']),point['x']+point['x_uncertainty']],pulse)[observable]
                xs=float(np.max(abs(shift-predicted[-1])))
                mesh_errors.append(float(mesh))
                budget=dict(digitization=point['y_uncertainty'],abscissa_sensitivity=xs,mesh=float(mesh),integration=10e-6 if observable=='voltage' else .01e-9,model_resampling=float(resampling))
                budgets.append(budget);uncertainties.append(sum(budget.values()))
                alternate.append(float(ref(position,1).observe([point['x']],pulse)[observable][0]) if case.get('ambiguous_data_label') else predicted[-1])
        metrics=residual_metrics([p['y'] for p in pts],predicted,uncertainties,
                    .6 if observable=='voltage' else 200e-9,timing=observable.endswith('_delay'))
        comparisons[curve['id']]=dict(**metrics,predicted=predicted,alternative=alternate,
                    mesh_uncertainty=mesh_errors,uncertainty_budget=budgets,observable=observable,case=case,
                    status='incomplete_definition' if (observable.endswith('_delay') or case.get('ambiguous_data_label')) else ('within_rule' if metrics['within_rule'] else 'disagrees'))
    convergence['timing_mesh_maximum_relative_change']=max((error/max(abs(prediction),1e-9) for c in comparisons.values() if c['observable'].endswith('_delay') for error,prediction in zip(c['mesh_uncertainty'],c['predicted'])),default=0.)
    convergence['timing_mesh_within_half_percent']=convergence['timing_mesh_maximum_relative_change']<.005
    numerical=dict(maximum_voltage_error_v=max(r['numerical_error']['voltage']['maximum'] for r in sweeps),
                   maximum_current_error_a=max(r['numerical_error']['sense_current']['maximum'] for r in sweeps),
                   maximum_settling_error_s=max(abs(v) for r in sweeps for v in r['numerical_timing_error_s'].values() if v is not None),
                   unresolved_cases=sum(any(m['time_s'] is None for m in r['measurements']['loaded'].values()) for r in sweeps))
    numerical['passes']=(numerical['maximum_voltage_error_v']<=10e-6 and numerical['maximum_current_error_a']<=.01e-9 and numerical['maximum_settling_error_s']<=5e-9 and numerical['unresolved_cases']==0)
    report=dict(status='incomplete_paper_reproduction',scope='SLM linear bitline only; no transistor/CAM calibration',
                manifest=manifest,numerical=numerical,comparisons=comparisons,convergence=convergence,sweeps=sweeps,
                provenance=dict(source_hashes={str(p.relative_to(ROOT)):sha256(p) for p in [ROOT/'src/model/NandRcLadder.cpp',ROOT/'include/model/NandRcLadder.h',ROOT/'tests/NandRcLadderProbe.cpp',Path(__file__),REFERENCE,DATA/'curves.json',DATA/'extraction.json']},executable_sha256=sha256(PROBE),numpy=np.__version__,python=sys.version),exclusions=extraction['unusable'])
    (output/'report.json').write_text(json.dumps(report,indent=2,allow_nan=False)+'\n')
    plot_report(output,curves,comparisons,sweeps,waveforms,convergence)
    print(json.dumps(numerical,indent=2));print(f'Report: {output / "report.json"}',flush=True)
    return 2 if args.require_reproduction else (0 if numerical['passes'] else 1)


if __name__=='__main__':
    raise SystemExit(main())
