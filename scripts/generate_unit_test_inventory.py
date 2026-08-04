#!/usr/bin/env python3
"""Generate the production-callable to dedicated-test inventory.

C++ is parsed with tree-sitter rather than regular expressions.  Python is
parsed with the standard-library ast module.  Existing coverage is classified
conservatively: only references inside named Test*/test_* functions count as a
dedicated test.  References made only by a broad main() remain missing.
"""

from __future__ import annotations

import argparse
import ast
import csv
import io
import re
import sys
from dataclasses import dataclass, replace
from pathlib import Path

try:
    from tree_sitter_language_pack import get_parser
except ImportError as error:  # pragma: no cover - dependency failure path
    raise SystemExit(
        "generate_unit_test_inventory.py requires tree-sitter-language-pack"
    ) from error


ROOT = Path(__file__).resolve().parents[1]
CPP_ROOTS = (ROOT / "include", ROOT / "src", ROOT / "bindings")
PYTHON_ROOTS = (ROOT / "evacam", ROOT / "scripts")
TEST_ROOT = ROOT / "tests"
DEFAULT_OUTPUT = TEST_ROOT / "unit_test_inventory.tsv"

CPP_FUNCTION_NODE = "function_definition"
CPP_DECLARATION_NODES = {"declaration", "field_declaration"}
CPP_CLASS_NODES = {"class_specifier", "struct_specifier", "union_specifier"}


@dataclass(frozen=True)
class Callable:
    file: str
    line: int
    signature: str
    name: str
    visibility: str
    language: str
    exemption: str = ""
    owner: str = ""
    arity: int = 0


@dataclass(frozen=True)
class TestReference:
    file: str
    case: str
    target: str
    arity: int


def node_text(source: bytes, node) -> str:
    return source[node.start_byte : node.end_byte].decode("utf-8", errors="replace")


def walk(node):
    yield node
    for child in node.children:
        yield from walk(child)


def descendants(node, node_type: str):
    for child in walk(node):
        if child.type == node_type:
            yield child


def closest_ancestor(node, node_types: set[str]):
    parent = node.parent
    while parent is not None:
        if parent.type in node_types:
            return parent
        parent = parent.parent
    return None


def cpp_class_name(source: bytes, class_node) -> str:
    name = class_node.child_by_field_name("name")
    return node_text(source, name) if name is not None else "<anonymous>"


def cpp_namespace_names(source: bytes, node) -> tuple[list[str], bool]:
    names: list[str] = []
    has_anonymous = False
    parent = node.parent
    while parent is not None:
        if parent.type == "namespace_definition":
            name = parent.child_by_field_name("name")
            if name is None:
                has_anonymous = True
            else:
                names.append(node_text(source, name))
        parent = parent.parent
    names.reverse()
    return names, has_anonymous


def cpp_enclosing_classes(source: bytes, node) -> list[str]:
    names: list[str] = []
    parent = node.parent
    while parent is not None:
        if parent.type in CPP_CLASS_NODES:
            names.append(cpp_class_name(source, parent))
        parent = parent.parent
    names.reverse()
    return names


def cpp_access(source: bytes, node, class_node) -> str:
    default = "private" if class_node.type == "class_specifier" else "public"
    body = class_node.child_by_field_name("body")
    if body is None:
        return default

    access = default
    for child in body.children:
        if child.start_byte > node.start_byte:
            break
        if child.type == "access_specifier":
            access = node_text(source, child).rstrip(":").strip()
    return access


def find_function_declarator(node):
    if node.type == "function_declarator":
        return node
    for child in node.children:
        found = find_function_declarator(child)
        if found is not None:
            return found
    return None


def function_name_node(function_declarator):
    declarator = function_declarator.child_by_field_name("declarator")
    if declarator is not None:
        return declarator
    return function_declarator.children[0] if function_declarator.children else None


def normalize_signature(text: str) -> str:
    return re.sub(r"\s+", " ", text).strip()


def unqualified_name(name: str) -> str:
    value = name.rsplit("::", 1)[-1]
    return value.strip()


def cpp_callable_from_node(path: Path, source: bytes, node) -> Callable | None:
    function_declarator = find_function_declarator(node)
    if function_declarator is None:
        return None

    name_node = function_name_node(function_declarator)
    if name_node is None:
        return None

    raw_name = normalize_signature(node_text(source, name_node))
    signature = normalize_signature(node_text(source, function_declarator))
    classes = cpp_enclosing_classes(source, node)
    namespaces, has_anonymous_namespace = cpp_namespace_names(source, node)

    prefix: list[str] = []
    if "::" not in raw_name:
        prefix.extend(namespaces)
        prefix.extend(classes)
    qualified_name = "::".join(prefix + [raw_name]) if prefix else raw_name
    if raw_name != qualified_name:
        signature = signature.replace(raw_name, qualified_name, 1)

    class_node = closest_ancestor(node, CPP_CLASS_NODES)
    if path.suffix == ".cpp" and has_anonymous_namespace:
        visibility = "file-local"
    elif class_node is not None:
        visibility = cpp_access(source, node, class_node)
    elif path.suffix == ".cpp" and raw_name.startswith("operator"):
        visibility = "file-local"
    else:
        visibility = "public"

    full_text = normalize_signature(node_text(source, node))
    parameter_list = function_declarator.child_by_field_name("parameters")
    if parameter_list is None:
        parameter_list = next(
            (child for child in function_declarator.children if child.type == "parameter_list"),
            None,
        )
    arity = 0
    if parameter_list is not None:
        arity = sum(
            child.type in {"parameter_declaration", "optional_parameter_declaration"}
            for child in parameter_list.children
        )
    owner = ""
    qualified_parts = qualified_name.split("::")
    if len(qualified_parts) > 1:
        owner = "::".join(qualified_parts[:-1])
    exemption = ""
    if re.search(r"=\s*default\s*;?$", full_text):
        exemption = "explicitly defaulted special member; no custom behavior"
    elif re.search(r"=\s*delete\s*;?$", full_text):
        exemption = "deleted special member; no runtime behavior"
    elif re.search(r"=\s*0\s*;?$", full_text):
        exemption = "pure-virtual declaration; concrete overrides are inventoried"

    return Callable(
        file=path.relative_to(ROOT).as_posix(),
        line=node.start_point.row + 1,
        signature=signature,
        name=unqualified_name(raw_name),
        visibility=visibility,
        language="C++",
        exemption=exemption,
        owner=owner,
        arity=arity,
    )


def cpp_header_visibility(parser) -> dict[tuple[str, str, int], str]:
    visibility: dict[tuple[str, str, int], str] = {}
    for path in sorted((ROOT / "include").rglob("*.h")):
        source = path.read_bytes()
        tree = parser.parse(source)
        for node in walk(tree.root_node):
            if node.type not in CPP_DECLARATION_NODES | {CPP_FUNCTION_NODE}:
                continue
            class_node = closest_ancestor(node, CPP_CLASS_NODES)
            declarator = find_function_declarator(node)
            if class_node is None or declarator is None:
                continue
            name_node = function_name_node(declarator)
            if name_node is None:
                continue
            name = unqualified_name(normalize_signature(node_text(source, name_node)))
            parameters = declarator.child_by_field_name("parameters")
            if parameters is None:
                parameters = next(
                    (child for child in declarator.children if child.type == "parameter_list"),
                    None,
                )
            arity = 0 if parameters is None else sum(
                child.type in {"parameter_declaration", "optional_parameter_declaration"}
                for child in parameters.children
            )
            visibility[(cpp_class_name(source, class_node), name, arity)] = cpp_access(
                source, node, class_node
            )
    return visibility


def cpp_callables(parser) -> list[Callable]:
    callables: list[Callable] = []
    for root in CPP_ROOTS:
        for path in sorted(root.rglob("*")):
            if path.suffix not in {".h", ".hpp", ".cpp", ".cc"}:
                continue
            source = path.read_bytes()
            tree = parser.parse(source)
            for node in walk(tree.root_node):
                include = node.type == CPP_FUNCTION_NODE
                if node.type in CPP_DECLARATION_NODES:
                    text = normalize_signature(node_text(source, node))
                    include = bool(re.search(r"=\s*(?:0|default|delete)\s*;$", text))
                if not include:
                    continue
                callable_ = cpp_callable_from_node(path, source, node)
                if callable_ is not None:
                    callables.append(callable_)
    header_visibility = cpp_header_visibility(parser)
    adjusted: list[Callable] = []
    for callable_ in callables:
        owner = callable_.owner.rsplit("::", 1)[-1]
        access = header_visibility.get((owner, callable_.name, callable_.arity))
        adjusted.append(replace(callable_, visibility=access) if access else callable_)
    return adjusted


def python_signature(node: ast.FunctionDef | ast.AsyncFunctionDef) -> str:
    args: list[str] = []
    positional = [*node.args.posonlyargs, *node.args.args]
    first_default = len(positional) - len(node.args.defaults)
    for index, argument in enumerate(positional):
        value = argument.arg
        if index >= first_default:
            value += "=<default>"
        args.append(value)
    if node.args.vararg:
        args.append("*" + node.args.vararg.arg)
    elif node.args.kwonlyargs:
        args.append("*")
    for argument, default in zip(node.args.kwonlyargs, node.args.kw_defaults):
        args.append(argument.arg + ("=<default>" if default is not None else ""))
    if node.args.kwarg:
        args.append("**" + node.args.kwarg.arg)
    return f"{node.name}({', '.join(args)})"


class PythonCallableVisitor(ast.NodeVisitor):
    def __init__(self, path: Path):
        self.path = path
        self.class_names: list[str] = []
        self.callables: list[Callable] = []

    def visit_ClassDef(self, node: ast.ClassDef) -> None:
        self.class_names.append(node.name)
        self.generic_visit(node)
        self.class_names.pop()

    def visit_FunctionDef(self, node: ast.FunctionDef) -> None:
        self._add_function(node)
        # Nested named functions are maintained callables too.
        self.generic_visit(node)

    def visit_AsyncFunctionDef(self, node: ast.AsyncFunctionDef) -> None:
        self._add_function(node)
        self.generic_visit(node)

    def _add_function(self, node: ast.FunctionDef | ast.AsyncFunctionDef) -> None:
        prefix = "::".join(self.class_names)
        signature = python_signature(node)
        if prefix:
            signature = f"{prefix}::{signature}"
        visibility = "private" if node.name.startswith("_") else "public"
        self.callables.append(
            Callable(
                file=self.path.relative_to(ROOT).as_posix(),
                line=node.lineno,
                signature=signature,
                name=node.name,
                visibility=visibility,
                language="Python",
                owner=prefix,
                arity=len(node.args.posonlyargs) + len(node.args.args)
                + len(node.args.kwonlyargs),
            )
        )


def python_callables() -> list[Callable]:
    callables: list[Callable] = []
    for root in PYTHON_ROOTS:
        for path in sorted(root.rglob("*.py")):
            visitor = PythonCallableVisitor(path)
            visitor.visit(ast.parse(path.read_text(encoding="utf-8"), filename=str(path)))
            callables.extend(visitor.callables)
    return callables


def make_targets() -> dict[str, str]:
    targets: dict[str, str] = {}
    current = ""
    for line in (ROOT / "Makefile").read_text(encoding="utf-8").splitlines():
        match = re.match(r"^([A-Za-z0-9_.-]+)\s*:", line)
        if match and not line.startswith((" ", "\t")):
            current = match.group(1)
        for test_path in re.findall(r"tests/[A-Za-z0-9_.-]+\.(?:cpp|py)", line):
            targets[test_path] = current
    return targets


def named_test_references(parser) -> dict[str, list[TestReference]]:
    references: dict[str, list[TestReference]] = {}
    targets = make_targets()

    for path in sorted(TEST_ROOT.glob("*.cpp")):
        source = path.read_bytes()
        tree = parser.parse(source)
        functions: dict[str, tuple[object, list[tuple[str, str, int]]]] = {}
        for node in descendants(tree.root_node, CPP_FUNCTION_NODE):
            declarator = find_function_declarator(node)
            if declarator is None:
                continue
            name_node = function_name_node(declarator)
            if name_node is None:
                continue
            case = unqualified_name(normalize_signature(node_text(source, name_node)))
            calls: list[tuple[str, str, int]] = []
            for call in descendants(node, "call_expression"):
                function = call.child_by_field_name("function")
                arguments = call.child_by_field_name("arguments")
                if function is None or arguments is None:
                    continue
                called_text = normalize_signature(node_text(source, function))
                called_name_match = re.search(r"(?:->|\.|::)?(~?[A-Za-z_]\w*|operator\S+)$", called_text)
                if called_name_match is None:
                    continue
                called_name = called_name_match.group(1)
                arity = sum(child.is_named for child in arguments.children)
                calls.append((called_text, called_name, arity))
            functions[case] = (node, calls)

        for case in sorted(functions):
            if not (case.startswith("Test") or case.startswith("test_")):
                continue
            pending = [case]
            visited: set[str] = set()
            reachable_calls: list[tuple[str, str, int]] = []
            while pending:
                function_name = pending.pop()
                if function_name in visited or function_name not in functions:
                    continue
                visited.add(function_name)
                for call_info in functions[function_name][1]:
                    reachable_calls.append(call_info)
                    if call_info[1] in functions:
                        pending.append(call_info[1])

            for called_text, called_name, arity in reachable_calls:
                reference = TestReference(
                    file=path.relative_to(ROOT).as_posix(),
                    case=case,
                    target=targets.get(path.relative_to(ROOT).as_posix(), ""),
                    arity=arity,
                )
                references.setdefault(called_name, []).append(reference)
                if "::" in called_text and "." not in called_text and "->" not in called_text:
                    references.setdefault(called_text, []).append(reference)

    for path in sorted(TEST_ROOT.glob("*.py")):
        source = path.read_text(encoding="utf-8")
        tree = ast.parse(source, filename=str(path))
        for node in ast.walk(tree):
            if not isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
                continue
            if not node.name.startswith("test_"):
                continue
            for call in ast.walk(node):
                if not isinstance(call, ast.Call):
                    continue
                if isinstance(call.func, ast.Name):
                    called_name = call.func.id
                elif isinstance(call.func, ast.Attribute):
                    called_name = call.func.attr
                else:
                    continue
                reference = TestReference(
                    file=path.relative_to(ROOT).as_posix(),
                    case=node.name,
                    target=targets.get(path.relative_to(ROOT).as_posix(), ""),
                    arity=len(call.args) + len(call.keywords),
                )
                references.setdefault(called_name, []).append(reference)

    for name in references:
        references[name] = sorted(
            set(references[name]), key=lambda item: (item.file, item.case, item.target)
        )
    return references


def deduplicate(callables: list[Callable]) -> list[Callable]:
    # A definition is normally seen once.  Keep overloads distinct while
    # removing parser duplicates at the same source location.
    unique: dict[tuple[str, int, str], Callable] = {}
    for callable_ in callables:
        unique[(callable_.file, callable_.line, callable_.signature)] = callable_
    return sorted(unique.values(), key=lambda value: (value.file, value.line, value.signature))


def render(callables: list[Callable], references: dict[str, list[TestReference]]) -> str:
    output = io.StringIO()
    writer = csv.writer(output, delimiter="\t", lineterminator="\n")
    writer.writerow(
        [
            "production_file",
            "line",
            "language",
            "qualified_callable",
            "visibility",
            "status",
            "dedicated_test_case",
            "test_target",
            "behaviors_or_branches_checked",
            "exemption_or_refactor_note",
        ]
    )
    owners_by_name: dict[str, set[str]] = {}
    overload_counts: dict[tuple[str, str, int], int] = {}
    for callable_ in callables:
        owners_by_name.setdefault(callable_.name, set()).add(callable_.owner)
        key = (callable_.owner, callable_.name, callable_.arity)
        overload_counts[key] = overload_counts.get(key, 0) + 1

    focused_test_stems = {
        "IntValueDomain": {"ExplorationDomain"},
        "ExplorationSpec": {"ExplorationDomain"},
        "ExplorationSpaceResolver": {"ExplorationDomain"},
        "YamlNodeHelpers": {"YamlHelpers", "YamlPrimitiveCoverage"},
        "YamlUnitParsers": {"YamlHelpers", "YamlPrimitiveCoverage"},
        "Technology": {"TechnologyYamlLoader", "Technology"},
        "MemCell": {"MemCell"},
        "formula": {"Formula", "FormulaCoverage"},
        "Wire": {"WireCopy", "WireAndFactory"},
        "WireProcessTable": {"WireAndFactory"},
        "WireFactory": {"WireAndFactory"},
        "FunctionUnit": {"FunctionUnit"},
        "BasicDecoder": {"DecoderComponents"},
        "PredecodeBlock": {"DecoderComponents"},
        "Mux": {"DriverMuxComponents"},
        "OutputDriver": {"DriverMuxComponents"},
        "Precharger": {"ChargingAndSensingComponents"},
        "RowDecoder": {"ChargingAndSensingComponents"},
        "SenseAmp": {"ChargingAndSensingComponents"},
        "CAM_BasicEncoder": {"CamEncoderComponents"},
        "CAM_Encoder": {"CamEncoderComponents"},
        "CAM_PriorityEncoder": {"CamEncoderComponents"},
        "CAM_InputEncoder": {"CamInputPeripheralComponents"},
        "CAM_DataBuffer": {"CamInputPeripheralComponents"},
        "CAM_LevelShifter": {"CamInputPeripheralComponents"},
        "CAM_BasicMMR": {"CamMmrSenseComponents"},
        "CAM_MMR": {"CamMmrSenseComponents"},
        "CAM_OutputAccumulator": {"CamMmrSenseComponents"},
        "CAM_SenseAmp": {"CamMmrSenseComponents"},
        "CAM_Line": {"CamLine"},
        "CAM_SubArray": {"CamSubArrayTopology", "CamSubArrayMatch", "CamSubArrayVariation"},
        "Mat": {"MatAndBank"},
        "Bank": {"MatAndBank"},
        "BankWithoutHtree": {"MatAndBank", "BankWithoutHtreeAndFactory"},
        "BankWithHtree": {"BankWithHtreeCoverage"},
        "BankFactory": {"BankWithoutHtreeAndFactory"},
        "Result": {"ResultModel"},
        "UnitFormatter": {"UnitFormatter"},
        "ResultsYaml": {"ResultsSerialization"},
        "VariationSamplesCsv": {"ResultsSerialization"},
        "EvaCamResultExtractor": {"OutputServices"},
        "EvaCamConfigPrinter": {"OutputServices"},
        "EvaCamOutput": {"OutputServices"},
        "EvaCamOutputDetail": {"OutputServices"},
        "Logger": {"AppServices"},
        "EvaCamContextBuilder": {"AppServices"},
        "EvaCamExplorer": {"EvaCamExplorer"},
        "EvaCAM_Match": {"EvaCAMMatchFocused"},
        "EvaCamRun": {"RunEvaCamBoundary"},
        "main": {"RunEvaCamBoundary"},
        "VariationSampler": {"VariationSampler"},
        "EvaCamYamlLoader": {"TopLevelConfigParser"},
        "__init__": {"test_python_package_data"},
        "generate_unit_test_inventory": {"test_unit_test_inventory_generator"},
        "migrate_configs_to_v2": {"test_config_migration_scripts"},
        "sync_python_package_config_lib": {"test_config_sync_script"},
        "generate_variation_mc_sweep": {
            "test_generation_scripts", "test_sweep_analysis_scripts"
        },
        "run_corner_sweep": {"test_sweep_analysis_scripts"},
        "analyze_corner_sweep": {"test_sweep_analysis_scripts"},
        "generate_corner_variation_table": {
            "test_generation_scripts", "test_sweep_analysis_scripts"
        },
        "plot_corner_variation": {"test_plotting_scripts"},
        "plot_variation_histograms": {"test_plotting_scripts"},
        "plot_variation_qq": {"test_plotting_scripts"},
        "poster_figures": {"test_plotting_scripts"},
        "test_variation_normality": {"test_plotting_scripts"},
        "EvaCAM_Pybind": {"test_pybind_match", "test_pybind_run"},
        "TechnologyLoader": {"TechnologyYamlLoader", "TechnologyAndVariationConfig"},
        "VariationConfigBuilder": {"YamlHelpers", "TechnologyAndVariationConfig"},
        "CellYamlLoader": {"CellYamlLoader", "CellAndMemoryLoaderBranches"},
        "MemoryDeviceYamlLoader": {"CellYamlLoader", "CellAndMemoryLoaderBranches"},
        "PhysicalDomainValidators": {"PhysicalDomainValidators"},
        "CustomSenseAmpYamlLoader": {"CustomSenseAmpYamlLoader", "SenseAmpLoaderBranches"},
        "SenseAmpYamlLoader": {"CustomSenseAmpYamlLoader", "SenseAmpLoaderBranches"},
        "TechnologyYamlLoader": {"TechnologyYamlLoader", "TechnologyYamlLoaderBranches"},
        "EvaCamConfig": {"TopLevelConfigParser", "EvaCamConfig"},
        "EvaCamConfigValidator": {"ConfigValidators"},
        "InputRuleValidator": {"ConfigValidators", "InputValidation"},
        "OutputFileLock": {"OutputFileLock"},
    }
    indirect_test_cases = {
        ("src/config/ConfigNormalizer.cpp", "SameDomain"): [
            ("tests/ConfigNormalizerTest.cpp", "TestDeepExplorationExpandsOnlyDefaultDomains", "test-config-normalizer"),
            ("tests/ConfigNormalizerTest.cpp", "TestExplicitGeometryAndFixedDomainArePreserved", "test-config-normalizer"),
        ],
        ("src/config/ConfigNormalizer.cpp", "ApplyExplorationDefaults"): [
            ("tests/ConfigNormalizerTest.cpp", "TestOrdinaryDefaultsRestrictDefaultGeometry", "test-config-normalizer"),
            ("tests/ConfigNormalizerTest.cpp", "TestDeepExplorationExpandsOnlyDefaultDomains", "test-config-normalizer"),
            ("tests/ConfigNormalizerTest.cpp", "TestExplicitGeometryAndFixedDomainArePreserved", "test-config-normalizer"),
        ],
        ("src/config/ConfigNormalizer.cpp", "ApplyCactiAssumption"): [
            ("tests/ConfigNormalizerTest.cpp", "TestCactiAssumptionOverridesActiveGeometry", "test-config-normalizer"),
            ("tests/ConfigNormalizerTest.cpp", "TestCactiAssumptionRejectsNonPositiveColumnDomain", "test-config-normalizer"),
        ],
        ("src/config/ConfigSectionReaders.cpp", "TryReadOptionalChild"): [
            ("tests/ConfigSectionReadersTest.cpp", "TestReadOrganizationSection", "test-config-sections"),
        ],
        ("src/config/ConfigSectionReaders.cpp", "ReadCoordinatePair"): [
            ("tests/ConfigSectionReadersTest.cpp", "TestReadOrganizationSection", "test-config-sections"),
        ],
    }

    def map_indirect(file: str, names: list[str],
            test_file: str, test_case: str, target: str) -> None:
        mapping = (test_file, test_case, target)
        for name in names:
            indirect_test_cases.setdefault((file, name), []).append(mapping)

    map_indirect("src/config/OutputFileLock.cpp",
            ["OutputFileLock", "~OutputFileLock", "Release"], "tests/OutputFileLockTest.cpp",
            "TestAcquireCreatesParentAndDestructorReleasesLock", "test-output-file-lock")
    map_indirect("src/config/OutputFileLock.cpp", ["OutputFileLock"],
            "tests/OutputFileLockTest.cpp", "TestMoveConstructionTransfersOwnership",
            "test-output-file-lock")
    map_indirect("src/config/OutputFileLock.cpp", ["operator=", "Release"],
            "tests/OutputFileLockTest.cpp", "TestMoveAssignmentReleasesOldLockAndTransfersNewLock",
            "test-output-file-lock")
    map_indirect("src/config/OutputFileLock.cpp", ["OutputFileLock"],
            "tests/OutputFileLockTest.cpp", "TestPathConstructorOwnsAndReleasesAnExistingLockDirectory",
            "test-output-file-lock")

    map_indirect("src/config/TechnologyLoader.cpp",
            ["FindYamlSpec", "BuildTechFromSpec", "LoadTechFromYaml"],
            "tests/TechnologyAndVariationConfigTest.cpp",
            "TestTechnologyLoaderLoadsExactUpdatedAndLegacyNodes",
            "test-technology-variation-config")
    map_indirect("src/config/TechnologyLoader.cpp",
            ["LegacyBucketNode", "FindYamlBaseSpec", "LoadCell"],
            "tests/TechnologyAndVariationConfigTest.cpp",
            "TestTechnologyLoaderUsesLegacyBucketsAndLoadsCellRelativeToCellFile",
            "test-technology-variation-config")
    map_indirect("src/config/TechnologyLoader.cpp",
            ["HighInterpolationNode", "InterpolationAlpha", "LoadTechFromYaml"],
            "tests/TechnologyAndVariationConfigTest.cpp",
            "TestTechnologyLoaderInterpolatesBetweenUpdatedNodes",
            "test-technology-variation-config")
    map_indirect("src/config/TechnologyLoader.cpp", ["LoadFefetTech"],
            "tests/TechnologyAndVariationConfigTest.cpp",
            "TestTechnologyLoaderRejectsUnavailableNodesAndRoadmaps",
            "test-technology-variation-config")
    map_indirect("src/config/VariationConfigBuilder.cpp", ["DefaultVariationSeed"],
            "tests/TechnologyAndVariationConfigTest.cpp",
            "TestVariationConfigBuilderDerivesCornerCountsAndSeeds",
            "test-technology-variation-config")
    map_indirect("src/config/VariationConfigBuilder.cpp", ["ValidateCornerVariation"],
            "tests/TechnologyAndVariationConfigTest.cpp",
            "TestVariationConfigBuilderRejectsInvalidValues",
            "test-technology-variation-config")
    map_indirect("src/config/VariationConfigBuilder.cpp", ["CornerDimensionCount"],
            "tests/TechnologyAndVariationConfigTest.cpp",
            "TestVariationConfigBuilderDerivesCornerCountsAndSeeds",
            "test-technology-variation-config")

    map_indirect("src/technology/Technology.cpp", ["ToString"],
            "tests/TechnologyTest.cpp",
            "TestPrintPropertyIncludesIdentityAndRepresentativeValues", "test-technology")
    map_indirect("src/technology/Technology.cpp", ["PrintProperty"],
            "tests/TechnologyTest.cpp",
            "TestPrintPropertyIncludesIdentityAndRepresentativeValues", "test-technology")
    map_indirect("include/technology/Technology.h", ["Technology", "~Technology"],
            "tests/TechnologyTest.cpp", "TestDefaultConstructionIsUninitialized",
            "test-technology")
    map_indirect("src/technology/Technology.cpp", ["ApplySpec"],
            "tests/TechnologyTest.cpp", "TestInitializeFromSpecCopiesEveryGetter",
            "test-technology")
    map_indirect("src/technology/Technology.cpp",
            ["ExpandCurrentTable", "ExpandTemperatureTables"],
            "tests/TechnologyTest.cpp",
            "TestTemperatureTablesPreserveEndpointsAndInterpolateInteriorValues",
            "test-technology")

    map_indirect("src/technology/MemCell.cpp", ["MemCell"],
            "tests/MemCellTest.cpp", "TestConstructorEstablishesDocumentedDefaults",
            "test-mem-cell")

    map_indirect("src/circuit/formula.cpp",
            ["CheckTransistorWidths", "CalculatePmosRatio", "CalculateMaxPlanarWidths",
             "ApplyPlanarPmosLayout", "ApplyPlanarNmosLayout", "ApplyFinfetRegionLayout",
             "CalculateGateLayout"],
            "tests/FormulaCoverageTest.cpp",
            "TestGateAreaAndCapacitanceCoverPlanarFinfetAndZeroWidths",
            "test-formula-coverage")
    map_indirect("src/circuit/formula.cpp",
            ["CalculateFBRAMGateCap", "CalculateFBRAMDrainCap"],
            "tests/FormulaCoverageTest.cpp",
            "TestGateCapAndFbramCapacitancesUseReferenceValues",
            "test-formula-coverage")
    map_indirect("src/circuit/formula.cpp", ["CalculateDrainCap"],
            "tests/FormulaCoverageTest.cpp", "TestDrainCapacitorCoversNmosAndPmosPaths",
            "test-formula-coverage")
    map_indirect("src/circuit/formula.cpp",
            ["CalculateGateLeakage", "CalculateOnResistance"],
            "tests/FormulaCoverageTest.cpp",
            "TestLeakageAndResistanceCoverTypesAndTemperatureBounds",
            "test-formula-coverage")
    map_indirect("src/circuit/formula.cpp", ["CalculateTransconductance"],
            "tests/FormulaCoverageTest.cpp", "TestTransconductanceCoversLegacyUpdatedAndTypes",
            "test-formula-coverage")
    map_indirect("src/circuit/formula.cpp",
            ["CalculateWireResistance", "CalculateWireCapacitance"],
            "tests/FormulaCoverageTest.cpp",
            "TestHorowitzAndWireReferencesAndInvalidDimensions",
            "test-formula-coverage")

    map_indirect("include/circuit/Wire.h", ["Wire"],
            "tests/WireAndFactoryTest.cpp", "TestCalculateLatencyAndPowerRequiresInitialization",
            "test-wire-factory")
    map_indirect("src/circuit/Wire.cpp", ["Initialize"],
            "tests/WireAndFactoryTest.cpp", "TestInitializeAllConfiguredWireTypes",
            "test-wire-factory")
    map_indirect("src/circuit/WireProcessTable.cpp", ["FindWireProcessSpec"],
            "tests/WireAndFactoryTest.cpp", "TestInitializeAllConfiguredWireTypes",
            "test-wire-factory")
    map_indirect("src/circuit/Wire.cpp", ["CalculateLatencyAndPower"],
            "tests/WireAndFactoryTest.cpp", "TestPassiveWireReferenceValuesAndOptionalOutputs",
            "test-wire-factory")
    map_indirect("src/circuit/Wire.cpp", ["CalculatePassiveLatencyAndPower"],
            "tests/WireAndFactoryTest.cpp", "TestPassiveWireReferenceValuesAndOptionalOutputs",
            "test-wire-factory")
    map_indirect("src/circuit/Wire.cpp", ["CalculateLowSwingLatencyAndPower"],
            "tests/WireAndFactoryTest.cpp", "TestLowSwingWireCalculationAndResetBehavior",
            "test-wire-factory")
    map_indirect("src/circuit/Wire.cpp", ["CalculateRepeatedLatencyAndPower"],
            "tests/WireAndFactoryTest.cpp",
            "TestRepeatedWireCalculationUsesUnitValuesAndResetsOutputs",
            "test-wire-factory")
    map_indirect("src/circuit/Wire.cpp",
            ["RepeaterPenalty", "CalculateRepeaterTransistorWidths",
             "CalculateRepeaterElectricals", "findOptimalRepeater", "findPenalizedRepeater"],
            "tests/WireAndFactoryTest.cpp", "TestOptimalAndPenalizedRepeaters",
            "test-wire-factory")
    map_indirect("src/circuit/Wire.cpp", ["PrintProperty"],
            "tests/WireAndFactoryTest.cpp", "TestPrintPropertyForPassiveAndRepeatedWires",
            "test-wire-factory")
    map_indirect("src/factories/WireFactory.cpp",
            ["CreateDefaultLocalWire", "CreateDefaultGlobalWire"],
            "tests/WireAndFactoryTest.cpp", "TestWireFactoryDefaultsAndFixedConfiguration",
            "test-wire-factory")

    map_indirect("src/circuit/FunctionUnit.cpp", ["FunctionUnit"],
            "tests/FunctionUnitTest.cpp", "TestConstructorZerosEveryMetricAndLeavesConfigNull",
            "test-function-unit")
    map_indirect("src/circuit/FunctionUnit.cpp", ["PrintProperty"],
            "tests/FunctionUnitTest.cpp", "TestPrintPropertyReportsScaledValuesAndLabels",
            "test-function-unit")

    map_indirect("include/circuit/BasicDecoder.h", ["BasicDecoder"],
            "tests/DecoderComponentsTest.cpp", "TestUninitializedDecodersRejectCalculations",
            "test-decoder-components")
    map_indirect("src/circuit/BasicDecoder.cpp",
            ["Initialize", "CalculateArea", "CalculateRC", "CalculateLatency", "CalculatePower"],
            "tests/DecoderComponentsTest.cpp", "TestBasicDecoderTopologiesAndLoads",
            "test-decoder-components")
    map_indirect("src/circuit/BasicDecoder.cpp", ["PrintProperty"],
            "tests/DecoderComponentsTest.cpp", "TestBasicDecoderPrintsIdentity",
            "test-decoder-components")
    map_indirect("include/circuit/PredecodeBlock.h", ["PredecodeBlock"],
            "tests/DecoderComponentsTest.cpp", "TestUninitializedDecodersRejectCalculations",
            "test-decoder-components")
    map_indirect("src/circuit/PredecodeBlock.cpp",
            ["Initialize", "CalculateArea", "CalculateRC", "CalculateLatency", "CalculatePower"],
            "tests/DecoderComponentsTest.cpp", "TestPredecodeBlockTopologies",
            "test-decoder-components")
    map_indirect("src/circuit/PredecodeBlock.cpp", ["PrintProperty"],
            "tests/DecoderComponentsTest.cpp", "TestPredecodeBlockPrintsIdentity",
            "test-decoder-components")

    map_indirect("src/circuit/Mux.cpp", ["HasMux", "Mux"],
            "tests/DriverMuxComponentsTest.cpp", "TestMuxModelsActiveAndAbsentTopologies",
            "test-driver-mux-components")
    map_indirect("src/circuit/Mux.cpp",
            ["Initialize", "CalculateArea", "CalculateRC", "CalculateLatency", "CalculatePower"],
            "tests/DriverMuxComponentsTest.cpp", "TestMuxModelsActiveAndAbsentTopologies",
            "test-driver-mux-components")
    map_indirect("src/circuit/Mux.cpp", ["PrintProperty"],
            "tests/DriverMuxComponentsTest.cpp", "TestMuxPrintsIdentityAndFunctionUnitProperties",
            "test-driver-mux-components")
    map_indirect("include/circuit/OutputDriver.h", ["OutputDriver"],
            "tests/DriverMuxComponentsTest.cpp", "TestOutputDriverRequiresInitialization",
            "test-driver-mux-components")
    map_indirect("src/circuit/OutputDriver.cpp",
            ["Initialize", "CalculateArea", "CalculateRC", "CalculateLatency", "CalculatePower"],
            "tests/DriverMuxComponentsTest.cpp", "TestOutputDriverModelsLatencyAndAreaTargets",
            "test-driver-mux-components")
    map_indirect("src/circuit/OutputDriver.cpp", ["PrintProperty"],
            "tests/DriverMuxComponentsTest.cpp", "TestOutputDriverPrintsIdentityAndStageCount",
            "test-driver-mux-components")

    map_indirect("include/circuit/Precharger.h", ["Precharger"],
            "tests/ChargingAndSensingComponentsTest.cpp",
            "TestUninitializedComponentsRejectCalculations", "test-charging-sensing-components")
    map_indirect("src/circuit/Precharger.cpp",
            ["Initialize", "CalculateArea", "CalculateRC", "CalculateLatency", "CalculatePower",
             "PrintProperty"],
            "tests/ChargingAndSensingComponentsTest.cpp",
            "TestPrechargerCalculatesGeometryLatencyAndPower",
            "test-charging-sensing-components")
    map_indirect("include/circuit/RowDecoder.h", ["RowDecoder"],
            "tests/ChargingAndSensingComponentsTest.cpp",
            "TestUninitializedComponentsRejectCalculations", "test-charging-sensing-components")
    map_indirect("src/circuit/RowDecoder.cpp",
            ["Initialize", "CalculateArea", "CalculateRC", "CalculateLatency", "CalculatePower",
             "PrintProperty"],
            "tests/ChargingAndSensingComponentsTest.cpp",
            "TestRowDecoderTopologiesAndInputValidation", "test-charging-sensing-components")
    map_indirect("include/circuit/SenseAmp.h", ["SenseAmp"],
            "tests/ChargingAndSensingComponentsTest.cpp",
            "TestUninitializedComponentsRejectCalculations", "test-charging-sensing-components")
    map_indirect("src/circuit/SenseAmp.cpp",
            ["LookupNodeValue", "Initialize", "CalculateArea", "CalculateRC", "CalculateLatency",
             "CalculatePower", "PrintProperty"],
            "tests/ChargingAndSensingComponentsTest.cpp", "TestSenseAmpVoltageAndCurrentModes",
            "test-charging-sensing-components")

    map_indirect("src/config/InputRuleValidator.cpp",
            ["LoadCellFileForValidation", "ResolveReference", "LoadMemoryDeviceForValidation",
             "LoadMemCellTypeForValidation", "ValidateMemCellSupport"],
            "tests/ConfigValidatorsTest.cpp",
            "TestInputRuleValidatorAcceptsV2ReferencePathsAndRejectsLegacyCells",
            "test-config-validators")
    map_indirect("src/config/InputRuleValidator.cpp",
            ["InferCamTypeToken", "LoadCamTypeForValidation"],
            "tests/ConfigValidatorsTest.cpp", "TestInputRuleValidatorInfersCamTypeFromCellName",
            "test-config-validators")
    map_indirect("src/config/InputRuleValidator.cpp",
            ["IsCamModelMemCellTypeSupported", "ValidateCamModelSupport"],
            "tests/ConfigValidatorsTest.cpp",
            "TestInputRuleValidatorAcceptsAndRejectsEveryCamMemoryType",
            "test-config-validators")
    map_indirect("src/config/InputRuleValidator.cpp",
            ["ValidateCamPortPresence", "LoadPortConnectionRegion", "ValidateCamColumnTopology"],
            "tests/ConfigValidatorsTest.cpp",
            "TestInputRuleValidatorValidatesCamPortPresenceAndConnections",
            "test-config-validators")
    map_indirect("src/config/InputRuleValidator.cpp", ["ValidateMcamResistanceStates"],
            "tests/ConfigValidatorsTest.cpp", "TestInputRuleValidatorValidatesMcamMapsAndVoltages",
            "test-config-validators")
    map_indirect("src/config/InputRuleValidator.cpp",
            ["IsSupportedCamSenseAmpType", "ValidateCustomSenseAmpFile",
             "ValidateDefaultSenseAmpFile", "ValidatePeripheralSupport"],
            "tests/ConfigValidatorsTest.cpp", "TestInputRuleValidatorValidatesPeripheralVariants",
            "test-config-validators")
    map_indirect("src/config/InputRuleValidator.cpp",
            ["CheckedMultiply", "CheckedTotalProduct", "ValidateScalarDomains",
             "ValidateDerivedInputs", "ValidateAndResolveExplicitSubarrayDimensions"],
            "tests/ConfigValidatorsTest.cpp",
            "TestInputRuleValidatorCoversRemainingSizingAndScalarRules",
            "test-config-validators")

    map_indirect("src/input/YamlNodeHelpers.cpp",
            ["mapping"], "tests/YamlPrimitiveCoverageTest.cpp",
            "TestEnumMappingsCaseBehaviorAndInitializerList", "test-yaml-primitives")
    map_indirect("src/input/YamlUnitParsers.cpp",
            ["trim", "unit_suffix_matches", "parse_quantity_string"],
            "tests/YamlPrimitiveCoverageTest.cpp",
            "TestQuantityPrivateParserBehaviorAndErrors", "test-yaml-primitives")
    map_indirect("src/input/PhysicalDomainValidators.cpp", ["ValidateOptionalPositive"],
            "tests/PhysicalDomainValidatorsTest.cpp",
            "TestValidateMemCellChecksOptionalResistanceAndFlashFields",
            "test-physical-domain-validators")
    map_indirect("src/input/PhysicalDomainValidators.cpp", ["ValidateCurrentArray"],
            "tests/PhysicalDomainValidatorsTest.cpp",
            "TestValidateTechnologyChecksEveryCurrentTable",
            "test-physical-domain-validators")

    map_indirect("src/input/CellYamlLoader.cpp",
            ["to_lower", "parse_voltage_fields", "ReadV2CellSection", "ReadAccessDeviceSection",
             "ReadResistanceSection", "ReadCapacitanceSection", "ReadDeviceSection",
             "ReadReadSection", "ReadWriteSection", "ReadSramSection", "ReadFlashSection",
             "ReadVariationSection", "ReadMcamSection", "ReadPortsSection",
             "ReadMemoryDeviceReference", "ReadMemCellFromYaml"],
            "tests/CellAndMemoryLoaderBranchesTest.cpp",
            "TestReadMemCellFromYamlV2ResolvesRelativeDeviceAndParsesEverySection",
            "test-cell-memory-loader-branches")
    map_indirect("src/input/CellYamlLoader.cpp",
            ["trim", "parse_fraction_or_percent_node", "resolve_reference"],
            "tests/CellAndMemoryLoaderBranchesTest.cpp",
            "TestReadMemCellFromYamlV2ResolvesRelativeDeviceAndParsesEverySection",
            "test-cell-memory-loader-branches")
    map_indirect("src/input/CellYamlLoader.cpp",
            ["has_key", "parse_port_connection", "parse_ports", "ValidateV2CellKeys",
             "RejectV2DeviceSections", "RejectUnsupportedDramSection"],
            "tests/CellAndMemoryLoaderBranchesTest.cpp",
            "TestLoadersRejectSchemasKeysAndUnsupportedForms",
            "test-cell-memory-loader-branches")
    map_indirect("src/input/MemoryDeviceYamlLoader.cpp",
            ["ReadResistanceSection", "ReadReadSection", "ReadWriteSection", "ReadMcamSection",
             "validate_memory_device_keys"],
            "tests/CellAndMemoryLoaderBranchesTest.cpp",
            "TestReadMemoryDeviceFromYamlLegacyCellAndSections",
            "test-cell-memory-loader-branches")

    map_indirect("src/input/CustomSenseAmpYamlLoader.cpp",
            ["AreaUnits", "FeatureLengthUnits", "ReadOptionalQuantity"],
            "tests/SenseAmpLoaderBranchesTest.cpp",
            "TestCustomNestedAndTopLevelLegacyShapesConvertUnits",
            "test-sense-amp-loader-branches")
    map_indirect("src/input/CustomSenseAmpYamlLoader.cpp", ["ValidateScalarSenseAmp"],
            "tests/SenseAmpLoaderBranchesTest.cpp",
            "TestCustomScalarRejectsSchemaShapeUnitsAndPhysicalDomains",
            "test-sense-amp-loader-branches")
    map_indirect("src/input/SenseAmpYamlLoader.cpp",
            ["ReadOptionalFeatureWidth", "ReadOptionalFeatureArea", "ReadNodeTable"],
            "tests/SenseAmpLoaderBranchesTest.cpp",
            "TestDefaultModelReadsTablesUnitsAndFallbacks",
            "test-sense-amp-loader-branches")
    map_indirect("src/input/TechnologyYamlLoader.cpp",
            ["CapacitancePerLengthUnits", "ComputeJunctionCap", "ComputeSidewallCap",
             "ComputeDrainToChannelCap", "ComputeVdsat", "ReadUseUpdatedLib"],
            "tests/TechnologyYamlLoaderBranchesTest.cpp",
            "TestUpdatedAndLegacyModelsAndDerivedValues", "test-technology-yaml-branches")
    map_indirect("src/input/TechnologyYamlLoader.cpp",
            ["ReadRoadmapKey", "BuildSpec"],
            "tests/TechnologyYamlLoaderBranchesTest.cpp",
            "TestRoadmapsMultipleSpecsAndFinGeometry", "test-technology-yaml-branches")
    map_indirect("src/input/TechnologyYamlLoader.cpp",
            ["ReadTable", "ReadTemperatureGridSize", "ValidateCurrentTable"],
            "tests/TechnologyYamlLoaderBranchesTest.cpp",
            "TestGridTablesAndNumericDomains", "test-technology-yaml-branches")

    for file, names, test_file, test_case, target in [
        ("src/cam/CAM_BasicEncoder.cpp", ["CAM_BasicEncoder"],
         "tests/CamEncoderComponentsTest.cpp", "TestEncodersRequireInitialization",
         "test-cam-encoder-components"),
        ("src/cam/CAM_Encoder.cpp", ["CAM_Encoder"],
         "tests/CamEncoderComponentsTest.cpp", "TestEncodersRequireInitialization",
         "test-cam-encoder-components"),
        ("src/cam/CAM_PriorityEncoder.cpp", ["CAM_PriorityEncoder"],
         "tests/CamEncoderComponentsTest.cpp", "TestEncodersRequireInitialization",
         "test-cam-encoder-components"),
        ("src/cam/CAM_InputEncoder.cpp", ["CAM_InputEncoder"],
         "tests/CamInputPeripheralComponentsTest.cpp",
         "TestUninitializedComponentsRejectCalculations",
         "test-cam-input-peripheral-components"),
        ("src/cam/CAM_DataBuffer.cpp", ["CAM_DataBuffer"],
         "tests/CamInputPeripheralComponentsTest.cpp",
         "TestUninitializedComponentsRejectCalculations",
         "test-cam-input-peripheral-components"),
        ("src/cam/CAM_LevelShifter.cpp", ["CAM_LevelShifter"],
         "tests/CamInputPeripheralComponentsTest.cpp",
         "TestUninitializedComponentsRejectCalculations",
         "test-cam-input-peripheral-components"),
        ("src/cam/CAM_BasicMMR.cpp", ["CAM_BasicMMR"],
         "tests/CamMmrSenseComponentsTest.cpp",
         "TestUninitializedCamComponentsRejectCalculations",
         "test-cam-mmr-sense-components"),
        ("src/cam/CAM_MMR.cpp", ["CAM_MMR"],
         "tests/CamMmrSenseComponentsTest.cpp",
         "TestUninitializedCamComponentsRejectCalculations",
         "test-cam-mmr-sense-components"),
        ("src/cam/CAM_OutputAccumulator.cpp", ["CAM_OutputAccumulator"],
         "tests/CamMmrSenseComponentsTest.cpp",
         "TestUninitializedCamComponentsRejectCalculations",
         "test-cam-mmr-sense-components"),
        ("src/cam/CAM_SenseAmp.cpp", ["CAM_SenseAmp"],
         "tests/CamMmrSenseComponentsTest.cpp",
         "TestUninitializedCamComponentsRejectCalculations",
         "test-cam-mmr-sense-components"),
        ("include/cam/CAM_Line.h", ["CAM_Line"], "tests/CamLineTest.cpp",
         "TestConstructorAndCopySemantics", "test-cam-line"),
        ("include/cam/CAM_SubArray.h", ["CAM_SubArray"],
         "tests/CamSubArrayTopologyTest.cpp",
         "TestPreInitializationAndCalculationOrderGuards", "test-cam-subarray-topology"),
        ("include/model/Mat.h", ["Mat"], "tests/MatAndBankTest.cpp",
         "TestMatGuardsAndInvalidConfigurations", "test-mat-bank"),
        ("include/model/Bank.h", ["Bank"], "tests/MatAndBankTest.cpp",
         "TestMatVariantsAndBankBaseBehavior", "test-mat-bank"),
        ("include/model/BankWithoutHtree.h", ["BankWithoutHtree"],
         "tests/BankWithoutHtreeAndFactoryTest.cpp", "TestPreInitializationGuards",
         "test-bank-without-htree-factory"),
        ("include/model/Result.h", ["Result"], "tests/ResultModelTest.cpp",
         "TestConstructorInitializeAndReset", "test-result-model"),
    ]:
        map_indirect(file, names, test_file, test_case, target)

    map_indirect("src/cam/CAM_SubArray.cpp",
            ["UsesSramStyleCamModel", "UsesResistiveCamModel", "MatchlineMosType",
             "MatchlineOffCurrent", "UsesMemoryResistanceInMatchPath",
             "BuildDrainSourceMatchlineParams", "BuildDiodeMatchlineParams",
             "BuildFloatingMatchlineParams", "CamDecoderNandInputCount", "Initialize"],
            "tests/CamSubArrayTopologyTest.cpp", "TestResistiveTcamAggregateCalculations",
            "test-cam-subarray-topology")
    map_indirect("src/cam/CAM_SubArray.cpp",
            ["CornerName", "NextCorner", "BuildCornerSelection", "ApplyCornerVariation"],
            "tests/CamSubArrayVariationTest.cpp",
            "TestMonteCarloCellAndCornerResistanceSamples", "test-cam-subarray-variation")
    map_indirect("src/cam/CAM_SubArray.cpp",
            ["MixVariationSeed", "BuildResistanceSample",
             "BuildCellMonteCarloResistanceSample", "BuildCornerResistanceSample"],
            "tests/CamSubArrayVariationTest.cpp",
            "TestResistanceSamplingIsDeterministicAndStreamSeparated",
            "test-cam-subarray-variation")
    map_indirect("src/cam/CAM_SubArray.cpp",
            ["BuildMetricStats", "BuildSinglePointMetric"],
            "tests/CamSubArrayVariationTest.cpp",
            "TestTimingSummaryHandlesDisabledSinglePointAndMonteCarlo",
            "test-cam-subarray-variation")
    map_indirect("src/cam/CAM_SubArray.cpp",
            ["McamStateTau", "McamStateDelays", "CalculateSearchPathLatenciesAfterMatchline",
             "MatchlineAllMatchTau"],
            "tests/CamSubArrayMatchTest.cpp", "TestMcamAndSearchlineHelpers",
            "test-cam-subarray-match")

    map_indirect("src/model/Bank.cpp",
            ["SumRowDrivers", "SumColumnMuxes", "PrintSubarrayAreaBreakdown",
             "PrintSearchLatencyBreakdown", "PrintSearchEnergyBreakdown",
             "PrintWriteEnergyBreakdown", "PrintLeakageBreakdown"],
            "tests/MatAndBankTest.cpp", "TestMatVariantsAndBankBaseBehavior",
            "test-mat-bank")
    map_indirect("src/model/Mat.cpp",
            ["Log2Rounded", "MarkInvalid"], "tests/MatAndBankTest.cpp",
            "TestMatGuardsAndInvalidConfigurations", "test-mat-bank")
    map_indirect("src/model/Mat.cpp",
            ["SplitPredecoderAddressBits", "InitializePredecoderPair",
             "ForEachPredecoderBlock", "SumPredecoderMetric", "MaxPairReadLatency",
             "Initialize"],
            "tests/MatAndBankTest.cpp", "TestMatTopologyAndMetrics", "test-mat-bank")

    map_indirect("src/model/BankWithHtree.cpp", ["BankWithHtree"],
            "tests/BankWithHtreeCoverageTest.cpp", "TestConstructorAndPreInitializationGuards",
            "test-bank-with-htree-coverage")
    map_indirect("src/model/BankWithHtree.cpp",
            ["MarkInvalid", "MarkInvalidInitialized", "HasRoutableBits"],
            "tests/BankWithHtreeCoverageTest.cpp",
            "TestInvalidConfigurationsAndReinitialization", "test-bank-with-htree-coverage")
    map_indirect("src/model/BankWithHtree.cpp",
            ["TotalHorizontalBits", "TotalVerticalBits", "GetWireAreaModel",
             "AccumulateHtreeLevelLatencyAndPower"],
            "tests/BankWithHtreeCoverageTest.cpp",
            "TestActiveCountClampingAndWireAreaModels", "test-bank-with-htree-coverage")
    map_indirect("src/model/BankWithHtree.cpp",
            ["CreateInitialRoutingState", "InitializeFirstHorizontalLevel",
             "ReduceExtraHorizontalLevels", "ReduceExtraVerticalLevels",
             "ReducePairedHorizontalAndVerticalLevels", "FinalizeMatRoutingBits"],
            "tests/BankWithHtreeCoverageTest.cpp",
            "TestExtraHorizontalAndVerticalRoutingLevels", "test-bank-with-htree-coverage")

    map_indirect("src/model/ResultConsole.cpp", ["print"],
            "tests/ResultModelTest.cpp", "TestConsolePrintForCalculatedResult",
            "test-result-model")

    map_indirect("include/output/UnitFormatter.h", ["operator<<"],
            "tests/UnitFormatterTest.cpp", "TestFormattedUnitStreamInsertion",
            "test-unit-formatter")

    map_indirect("include/app/Logger.h",
            ["Logger", "Line", "~Line", "operator<<"],
            "tests/AppServicesTest.cpp", "TestLoggerOutputAndManipulators",
            "test-app-services")
    map_indirect("include/app/Logger.h", ["Line", "~Line"],
            "tests/AppServicesTest.cpp", "TestLoggerMoveFlushesOnce",
            "test-app-services")
    map_indirect("include/app/Logger.h", ["OutputMutex"],
            "tests/AppServicesTest.cpp", "TestLoggerConcurrentWritesAreCompleteLines",
            "test-app-services")

    map_indirect("src/app/EvaCamContextBuilder.cpp", ["ValidateInputFile"],
            "tests/AppServicesTest.cpp", "TestContextBuilderRejectsMissingAndUnreadableInputs",
            "test-app-services")

    map_indirect("src/config/EvaCamConfigPrinter.cpp",
            ["ToString", "PrintConstraintIfSet"],
            "tests/OutputServicesTest.cpp", "TestConfigPrinterAndConsoleSummary",
            "test-output-services")

    map_indirect("src/app/EvaCamResultExtractor.cpp",
            ["OptimizationTargetName", "SafeCellArea", "SafeRatio", "HasUnit",
             "AddCoreSummary", "AddGeometry", "AddBreakdown", "ExtractDesignResult"],
            "tests/OutputServicesTest.cpp", "TestExtractorCompletePartialAndNoSolutions",
            "test-output-services")
    map_indirect("src/app/EvaCamResultExtractor.cpp",
            ["ExtractMetricStats", "ExtractVariation"],
            "tests/OutputServicesTest.cpp", "TestExtractorVariation",
            "test-output-services")

    map_indirect("src/app/EvaCamOutput.cpp", ["OptimizationTargetName"],
            "tests/OutputServicesTest.cpp", "TestYamlAndVariationSampleWrites",
            "test-output-services")
    map_indirect("src/app/EvaCamOutput.cpp",
            ["HasVariationSamples", "WriteVariationSamplesFile"],
            "tests/OutputServicesTest.cpp", "TestYamlAndVariationSampleWrites",
            "test-output-services")
    map_indirect("src/app/EvaCamOutput.cpp",
            ["PrintVariationMetric", "PrintVariationSampleMetric"],
            "tests/OutputServicesTest.cpp", "TestConfigPrinterAndConsoleSummary",
            "test-output-services")

    map_indirect("src/output/ResultsYaml.cpp",
            ["YamlWriter", "begin_map", "end_map", "line", "indent", "fmt_second",
             "fmt_bps", "fmt_joule", "fmt_watt", "fmt_voltage", "fmt_meter", "fmt_sqm",
             "fmt_percent", "safe_cell_area", "safe_percent", "write_assumptions",
             "write_breakdown", "write_summary", "write_results_body"],
            "tests/ResultsSerializationTest.cpp",
            "TestSingleResultStructureAssumptionsAndBreakdowns",
            "test-results-serialization")
    map_indirect("src/output/ResultsYaml.cpp",
            ["write_metric_stats", "write_metric_sample"],
            "tests/ResultsSerializationTest.cpp",
            "TestVariationSummarySinglePointAndMonteCarlo",
            "test-results-serialization")
    map_indirect("src/output/ResultsYaml.cpp", ["optimization_target_name", "roadmap_name", "bool_name"],
            "tests/ResultsSerializationTest.cpp",
            "TestMultiResultTargetNamesAndRoadmapsAndNoSolutions",
            "test-results-serialization")
    map_indirect("src/output/VariationSamplesCsv.cpp", ["CsvString"],
            "tests/ResultsSerializationTest.cpp",
            "TestVariationSamplesCsvHeaderQuotingAndSamples",
            "test-results-serialization")

    map_indirect("src/app/EvaCamExplorer.cpp",
            ["InitializeExploration", "InitializeBestResults", "InitializeWireCandidates",
             "CreateBestResultsBuffer"],
            "tests/EvaCamExplorerTest.cpp", "TestInitializationAndDeterministicThreading",
            "test-evacam-explorer")
    map_indirect("src/app/EvaCamExplorer.cpp",
            ["OpenExplorationCsv", "FlushExplorationCsvBuffer", "MaybeWriteExplorationCsv"],
            "tests/EvaCamExplorerTest.cpp", "TestWireCandidateFilteringAndCsvBuffering",
            "test-evacam-explorer")
    map_indirect("src/app/EvaCamExplorer.cpp",
            ["RunExplorationPass", "EvaluateGeometry", "RunConstrainedExploration",
             "BuildBank", "MakeResult", "IsValidCandidate", "MeetsConstraints",
             "ValidateCapacityOrThrow", "UpdateBestResults", "MergeBestResults"],
            "tests/EvaCamExplorerTest.cpp", "TestUnconstrainedAndConstrainedPasses",
            "test-evacam-explorer")
    map_indirect("src/app/EvaCamExplorer.cpp", ["PrintSolutionCount"],
            "tests/EvaCamExplorerTest.cpp", "TestConstrainedPassCanReturnZeroSolutions",
            "test-evacam-explorer")

    map_indirect("src/app/EvaCAM_Match.cpp", ["EvaCAM_Match", "InitializeConfiguredBank",
            "BuildMismatchLut", "EnsureInitialized", "CreateLocalWire", "CreateGlobalWire"],
            "tests/EvaCAMMatchFocusedTest.cpp", "TestConstructorWordWidthAndExactPublicOverloads",
            "test-evacam-match-focused")
    map_indirect("src/app/EvaCAM_Match.cpp",
            ["evaluate_vector", "EvaluateExactVector", "EvaluateExactTcamVector",
             "LookupMismatchResult", "CountTcamMismatches"],
            "tests/EvaCAMMatchFocusedTest.cpp", "TestConstructorWordWidthAndExactPublicOverloads",
            "test-evacam-match-focused")
    map_indirect("src/app/EvaCAM_Match.cpp",
            ["evaluate_array", "EvaluateBestVector", "EvaluateBestTcamArray",
             "MaxDetectableMismatches", "ValidateTcamMismatchCounts",
             "ValidateBestMatchSenseMargin"],
            "tests/EvaCAMMatchFocusedTest.cpp", "TestBestArrayOverloadsAndValidationRules",
            "test-evacam-match-focused")
    map_indirect("src/app/EvaCAM_Match.cpp",
            ["EvaluateThresholdVector", "ValidateMaxMismatches", "ValidateThresholdSenseMargin"],
            "tests/EvaCAMMatchFocusedTest.cpp", "TestThresholdOverloadsAndValidationRules",
            "test-evacam-match-focused")
    map_indirect("src/app/EvaCAM_Match.cpp",
            ["ValidateTcamMismatchCount", "ValidateMismatchCount", "ValidateBinaryVector",
             "ValidateVectorLength", "ValidateTcamStoredVector"],
            "tests/EvaCAMMatchFocusedTest.cpp", "TestExactTcamValidationRules",
            "test-evacam-match-focused")
    map_indirect("src/app/EvaCAM_Match.cpp", ["evaluate_vector", "evaluate_array",
            "EvaluateBestVector", "EvaluateThresholdVector", "ValidateAnalogVector",
            "ValidateAcamRangeVector"],
            "tests/EvaCAMMatchFocusedTest.cpp", "TestNonTcamAndAnalogPublicOverloadGaps",
            "test-evacam-match-focused")

    map_indirect("src/app/EvaCamRun.cpp",
            ["ScopedStdoutRedirect", "~ScopedStdoutRedirect", "BuildCliOptions", "RunEvaCam"],
            "tests/RunEvaCamBoundaryTest.cpp",
            "TestRunEvaCamQuietSuccessWritesOverrideAndRestoresStdout",
            "test-run-evacam-boundary")
    map_indirect("src/app/EvaCamRun.cpp", ["ScopedStdoutRedirect", "~ScopedStdoutRedirect", "RunEvaCam"],
            "tests/RunEvaCamBoundaryTest.cpp", "TestRunEvaCamErrorRestoresStdout",
            "test-run-evacam-boundary")
    map_indirect("src/app/main.cpp", ["ScopedStdoutRedirect", "~ScopedStdoutRedirect", "main"],
            "tests/RunEvaCamBoundaryTest.cpp", "TestExecutableHelpDiagnosticsQuietAndOutputPath",
            "test-run-evacam-boundary")

    # Phase 8 closes the remaining inventory links that are necessarily
    # indirect: these helpers are deliberately exercised through their public
    # parser, script, or binding boundary rather than exposed for testing.
    map_indirect("bindings/EvaCAM_Pybind.cpp", ["PYBIND11_MODULE"],
            "tests/test_pybind_run.py", "test_pybind_run_module", "test-pybind-run")
    map_indirect("include/config/EvaCamConfig.h", ["EvaCamConfig", "~EvaCamConfig"],
            "tests/EvaCamConfigTest.cpp", "TestConstructionDefaults", "test-evacam-config")
    map_indirect("include/input/YamlNodeHelpers.h",
            ["checked_integer", "read_required", "read_scalar_required", "read_required_index", "read_optional", "read_enum_required"],
            "tests/YamlPrimitiveCoverageTest.cpp", "TestReadTemplatesAndValidationTemplates", "test-yaml-primitives")
    map_indirect("include/input/YamlNodeHelpers.h", ["read_enum_required"],
            "tests/YamlPrimitiveCoverageTest.cpp", "TestEnumMappingsCaseBehaviorAndInitializerList", "test-yaml-primitives")
    map_indirect("src/config/EvaCamYamlLoader.cpp",
            ["HasKey", "RejectKeys", "ValidateRunConfigKeys", "ValidateArchitectureConfigKeys"],
            "tests/TopLevelConfigParserTest.cpp", "TestUnknownConfigKeysThrow", "test-top-level-parser")
    map_indirect("src/config/EvaCamYamlLoader.cpp",
            ["ResolveReference", "CopyIfPresent", "CopyMappedIfPresent", "ResolveSensingNode", "ReadMergedConfig", "BuildMergedRootV2"],
            "tests/TopLevelConfigParserTest.cpp", "TestSplitConfigParsesAndMapsMovedFields", "test-top-level-parser")
    map_indirect("src/config/EvaCamYamlLoader.cpp", ["ResolveSensingNode"],
            "tests/TopLevelConfigParserTest.cpp", "TestSensingFileActivatesSenseAmpModel", "test-top-level-parser")
    map_indirect("src/config/ExplorationSpaceResolver.cpp", ["ValidateNotEmpty"],
            "tests/ExplorationDomainTest.cpp", "TestDependentDomains", "test-exploration")
    map_indirect("src/config/ExplorationSpec.cpp", ["BuildPow2ClampedValues"],
            "tests/ExplorationDomainTest.cpp", "TestDependentDomains", "test-exploration")
    map_indirect("src/config/IntValueDomain.cpp", ["NormalizeFixedValues", "IntValueDomain"],
            "tests/ExplorationDomainTest.cpp", "TestIntValueDomain", "test-exploration")
    map_indirect("src/model/VariationSampler.cpp", ["VariationSampler"],
            "tests/VariationSamplerTest.cpp", "test_same_seed_same_sequence", "test-variation")

    map_indirect("scripts/generate_unit_test_inventory.py",
            ["visit_ClassDef", "visit_FunctionDef", "visit_AsyncFunctionDef", "_add_function"],
            "tests/test_unit_test_inventory_generator.py", "test_python_signature_visitor_and_discovery", "test-inventory-generator")
    map_indirect("scripts/generate_unit_test_inventory.py",
            ["map_indirect", "relevant_reference", "behavior_names"],
            "tests/test_unit_test_inventory_generator.py", "test_deduplicate_and_render_classify_coverage_exemptions_and_missing", "test-inventory-generator")
    map_indirect("scripts/generate_unit_test_inventory.py", ["parse_args", "main"],
            "tests/test_unit_test_inventory_generator.py", "test_parse_args_and_main_write_and_check_without_repository_mutation", "test-inventory-generator")

    map_indirect("scripts/migrate_configs_to_v2.py",
            ["load_yaml", "relpath", "relative_reference", "normalize_type", "make_memory_device", "iter_legacy_files", "referenced_legacy_cell_files"],
            "tests/test_config_migration_scripts.py", "test_conversion_resolves_references_and_is_idempotent", "test-config-migration-scripts")
    map_indirect("scripts/migrate_configs_to_v2.py", ["main"],
            "tests/test_config_migration_scripts.py", "test_main_converts_discovered_files_and_writes_report", "test-config-migration-scripts")
    map_indirect("scripts/sync_python_package_config_lib.py", ["main"],
            "tests/test_config_sync_script.py", "test_main_checks_and_synchronizes", "test-config-sync-script")
    map_indirect("scripts/generate_variation_mc_sweep.py", ["main"],
            "tests/test_generation_scripts.py", "test_mc_main_generates_consistent_local_references_on_repeat", "test-generation-scripts")

    map_indirect("scripts/run_corner_sweep.py", ["parse_args", "main"],
            "tests/test_sweep_analysis_scripts.py", "test_main_generates_configs_or_runs_sweep", "test-sweep-analysis-scripts")
    map_indirect("scripts/run_corner_sweep.py", ["main"],
            "tests/test_sweep_analysis_scripts.py", "test_main_generates_configs_or_runs_sweep", "test-sweep-analysis-scripts")
    map_indirect("scripts/run_corner_sweep.py",
            ["run_id", "run_dir", "expected_corners", "cell_path", "memory_device_path", "tool_path", "result_path", "samples_path", "log_path", "relative_to_root", "replace_variation_block", "replace_reference", "manifest_row", "generate_configs", "generate_artifacts", "write_summary", "run_sweep"],
            "tests/test_sweep_analysis_scripts.py", "test_config_generation_artifacts_and_summary_helpers", "test-sweep-analysis-scripts")
    map_indirect("scripts/run_corner_sweep.py",
            ["manifest_row", "write_summary", "run_sweep"],
            "tests/test_sweep_analysis_scripts.py", "test_completion_resume_summary_and_atomic_csv", "test-sweep-analysis-scripts")
    map_indirect("scripts/run_corner_sweep.py", ["generate_artifacts", "run_sweep"],
            "tests/test_sweep_analysis_scripts.py", "test_run_one_propagates_tool_and_artifact_subprocess_failures", "test-sweep-analysis-scripts")
    map_indirect("scripts/analyze_corner_sweep.py",
            ["parse_args", "read_csv", "write_csv", "build_main_effects", "build_sensitivity", "format_number", "write_report", "main"],
            "tests/test_sweep_analysis_scripts.py", "test_analysis_main_generates_csv_rankings_and_report_without_plots", "test-sweep-analysis-scripts")
    map_indirect("scripts/analyze_corner_sweep.py", ["build_main_effects", "build_sensitivity", "format_number"],
            "tests/test_sweep_analysis_scripts.py", "test_percent_helpers_derived_metrics_ranking_and_validation", "test-sweep-analysis-scripts")
    map_indirect("scripts/analyze_corner_sweep.py", ["setup_matplotlib", "plot_main_effects", "plot_distributions"],
            "tests/test_sweep_analysis_scripts.py", "test_analysis_plot_helpers_render_headlessly", "test-sweep-analysis-scripts")
    map_indirect("scripts/generate_corner_variation_table.py",
            ["sample_key", "read_rows", "available_metrics", "metric_value", "sorted_metric_rows", "metric_ranks", "format_value", "format_changed", "column_name", "markdown_table", "default_output_path", "main"],
            "tests/test_sweep_analysis_scripts.py", "test_table_reads_metrics_ranks_escapes_and_writes_markdown", "test-sweep-analysis-scripts")

    map_indirect("scripts/plot_corner_variation.py", ["read_rows", "available_metrics", "main"],
            "tests/test_plotting_scripts.py", "test_corner_and_poster_entry_points_render_from_real_csvs", "test-plotting-scripts")
    map_indirect("scripts/plot_variation_histograms.py", ["read_samples", "column_values", "available_metrics", "default_output_path", "main"],
            "tests/test_plotting_scripts.py", "test_histogram_main_validates_bins_and_uses_default_output", "test-plotting-scripts")
    map_indirect("scripts/plot_variation_histograms.py", ["column_values", "available_metrics", "default_output_path"],
            "tests/test_plotting_scripts.py", "test_readers_columns_metric_discovery_statistics_and_default_paths", "test-plotting-scripts")
    map_indirect("scripts/plot_variation_qq.py", ["read_samples", "column_values", "default_output_path", "main"],
            "tests/test_plotting_scripts.py", "test_qq_main_uses_default_output_path", "test-plotting-scripts")
    map_indirect("scripts/plot_variation_qq.py", ["column_values", "default_output_path"],
            "tests/test_plotting_scripts.py", "test_readers_columns_metric_discovery_statistics_and_default_paths", "test-plotting-scripts")
    map_indirect("scripts/poster_figures.py", ["read_rows", "style_matplotlib", "clean_axis", "save_figure", "main"],
            "tests/test_plotting_scripts.py", "test_corner_and_poster_entry_points_render_from_real_csvs", "test-plotting-scripts")
    map_indirect("scripts/test_variation_normality.py", ["read_samples", "available_metrics", "main"],
            "tests/test_plotting_scripts.py", "test_empty_malformed_short_and_main_decisions", "test-plotting-scripts")
    map_indirect("scripts/test_variation_normality.py", ["available_metrics"],
            "tests/test_plotting_scripts.py", "test_readers_metric_discovery_and_statistical_decision_boundaries", "test-plotting-scripts")
    known_test_cases = {
        (reference.file, reference.case, reference.target)
        for references_for_name in references.values()
        for reference in references_for_name
    }
    for mappings in indirect_test_cases.values():
        for mapping in mappings:
            if mapping not in known_test_cases:
                raise RuntimeError(
                    "indirect inventory mapping references an unknown test case: "
                    + "::".join(mapping)
                )

    def relevant_reference(callable_: Callable, reference: TestReference) -> bool:
        if reference.file.endswith("RegressionTest.cpp"):
            return False
        production_stem = Path(callable_.file).stem
        test_stem = Path(reference.file).stem.removesuffix("Test")
        allowed = focused_test_stems.get(production_stem)
        if allowed is not None:
            return test_stem in allowed
        owner = callable_.owner.rsplit("::", 1)[-1]
        candidates = {production_stem.lower(), owner.lower()}
        return any(candidate and candidate in test_stem.lower() for candidate in candidates)

    def behavior_names(matched_: list[TestReference]) -> str:
        names: list[str] = []
        for item in matched_:
            name = re.sub(r"^(?:Test|test_)", "", item.case)
            name = re.sub(r"(?<!^)(?=[A-Z])", " ", name).replace("_", " ").lower()
            if name not in names:
                names.append(name)
        return "; ".join(names)

    for callable_ in callables:
        exact_key = f"{callable_.owner}::{callable_.name}" if callable_.owner else ""
        matched = references.get(exact_key, []) if exact_key else []
        if (len(owners_by_name[callable_.name]) == 1
                or Path(callable_.file).stem in focused_test_stems):
            matched = [*matched, *references.get(callable_.name, [])]
        for file, case, target in indirect_test_cases.get(
                (callable_.file, callable_.name), []):
            matched.append(TestReference(
                file=file,
                case=case,
                target=target,
                arity=callable_.arity,
            ))
        matched = [item for item in matched if item.arity == callable_.arity]
        matched = [item for item in matched if relevant_reference(callable_, item)]
        if (overload_counts[(callable_.owner, callable_.name, callable_.arity)] > 1
                and not indirect_test_cases.get((callable_.file, callable_.name))):
            matched = []
        matched = sorted(set(matched), key=lambda item: (item.file, item.case, item.target))
        if callable_.exemption:
            status = "exempt"
            cases = ""
            targets = ""
            behavior = ""
            note = callable_.exemption
        elif matched:
            status = "covered"
            cases = "; ".join(f"{item.file}::{item.case}" for item in matched)
            targets = "; ".join(sorted({item.target for item in matched if item.target}))
            behavior = behavior_names(matched)
            note = "n/a"
        else:
            status = "missing"
            cases = ""
            targets = ""
            behavior = ""
            note = (
                "Add a focused boundary test for this entry point"
                if callable_.name == "main"
                else "Add a dedicated test; refactor behind an internal header if behavior cannot be isolated"
                if callable_.visibility in {"private", "file-local"}
                else "Add a dedicated test"
            )
        writer.writerow(
            [
                callable_.file,
                callable_.line,
                callable_.language,
                callable_.signature,
                callable_.visibility,
                status,
                cases,
                targets,
                behavior,
                note,
            ]
        )
    return output.getvalue()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument(
        "--check",
        action="store_true",
        help="fail if the checked-in inventory differs from generated output",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    parser = get_parser("cpp")
    callables = deduplicate(cpp_callables(parser) + python_callables())
    generated = render(callables, named_test_references(parser))
    output = args.output if args.output.is_absolute() else ROOT / args.output

    if args.check:
        if not output.exists():
            print(f"missing inventory: {output.relative_to(ROOT)}", file=sys.stderr)
            return 1
        if output.read_text(encoding="utf-8") != generated:
            print(
                f"{output.relative_to(ROOT)} is stale; run "
                "python3 scripts/generate_unit_test_inventory.py",
                file=sys.stderr,
            )
            return 1
        print(f"{output.relative_to(ROOT)} is current ({len(callables)} callables)")
        return 0

    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(generated, encoding="utf-8")
    statuses = {"covered": 0, "missing": 0, "exempt": 0}
    for row in csv.DictReader(io.StringIO(generated), delimiter="\t"):
        statuses[row["status"]] += 1
    print(
        f"wrote {output.relative_to(ROOT)}: {len(callables)} callables "
        f"({statuses['covered']} covered, {statuses['missing']} missing, "
        f"{statuses['exempt']} exempt)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
