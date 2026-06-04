#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "EvaCAM_Match.h"
#include "EvaCAMMatchResult.h"

namespace py = pybind11;

PYBIND11_MODULE(evacam_py, module) {
    module.doc() = "Python bindings for EvaCAM match evaluation";

    py::class_<EvaCAMMatchResult>(module, "EvaCAMMatchResult")
        .def_readonly("hit", &EvaCAMMatchResult::hit)
        .def_readonly("search_latency", &EvaCAMMatchResult::searchLatency)
        .def_readonly("search_dynamic_energy", &EvaCAMMatchResult::searchDynamicEnergy)
        .def_readonly("matchline_delay", &EvaCAMMatchResult::matchlineDelay)
        .def_readonly("sense_margin", &EvaCAMMatchResult::senseMargin);

    py::class_<EvaCAM_Match>(module, "EvaCAMMatch")
        .def(py::init<const std::string &>(), py::arg("config_path"))
        .def("match", &EvaCAM_Match::match, py::arg("stored"), py::arg("query"))
        .def("evaluate", &EvaCAM_Match::evaluate, py::arg("stored"), py::arg("query"))
        .def("word_width", &EvaCAM_Match::word_width);
}
