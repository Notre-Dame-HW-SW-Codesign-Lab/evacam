"""Focused standard-library tests for generate_unit_test_inventory.py."""

import ast
import contextlib
import importlib.util
import io
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

from tree_sitter_language_pack import get_parser


REPO_ROOT = Path(__file__).resolve().parents[1]


def load_inventory_script():
    name = "generate_unit_test_inventory_under_test"
    spec = importlib.util.spec_from_file_location(
        name, REPO_ROOT / "scripts" / "generate_unit_test_inventory.py"
    )
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


class UnitTestInventoryGeneratorTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.inventory = load_inventory_script()
        cls.parser = get_parser("cpp")

    def test_cpp_node_helpers_and_callable_metadata(self):
        source = b"""namespace outer { namespace { struct Box { public: int Run(int x) { return x; } private: void Hide(); }; } }\n"""
        root = self.parser.parse(source).root_node
        function = next(self.inventory.descendants(root, "function_definition"))
        declarator = self.inventory.find_function_declarator(function)
        name = self.inventory.function_name_node(declarator)
        class_node = self.inventory.closest_ancestor(function, self.inventory.CPP_CLASS_NODES)

        self.assertEqual("Run", self.inventory.node_text(source, name))
        self.assertIn(function, list(self.inventory.walk(root)))
        self.assertEqual([function], list(self.inventory.descendants(root, "function_definition")))
        self.assertEqual("Box", self.inventory.cpp_class_name(source, class_node))
        self.assertEqual((["outer"], True), self.inventory.cpp_namespace_names(source, function))
        self.assertEqual(["Box"], self.inventory.cpp_enclosing_classes(source, function))
        self.assertEqual("public", self.inventory.cpp_access(source, function, class_node))
        self.assertEqual("int Run(int x)", self.inventory.normalize_signature(" int\n Run(int x) "))
        self.assertEqual("Run", self.inventory.unqualified_name("outer::Box::Run"))
        with mock.patch.object(self.inventory, "ROOT", Path("/tmp")):
            callable_ = self.inventory.cpp_callable_from_node(Path("/tmp/demo.cpp"), source, function)
        self.assertEqual(("outer::Box::Run", "Run", "file-local", 1),
                         (callable_.signature.split("(")[0], callable_.name,
                          callable_.visibility, callable_.arity))

    def test_cpp_discovery_and_header_visibility_use_temporary_tree(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "include").mkdir()
            (root / "src").mkdir()
            (root / "bindings").mkdir()
            (root / "include" / "Thing.h").write_text(
                "class Thing { public: void Go(int); private: void Hide(); };\n", encoding="utf-8"
            )
            (root / "src" / "Thing.cpp").write_text(
                "#include \"Thing.h\"\nvoid Thing::Go(int x) {}\nvoid local() {}\n", encoding="utf-8"
            )
            with mock.patch.object(self.inventory, "ROOT", root), \
                 mock.patch.object(self.inventory, "CPP_ROOTS", (root / "include", root / "src", root / "bindings")):
                visibility = self.inventory.cpp_header_visibility(self.parser)
                callables = self.inventory.cpp_callables(self.parser)
            self.assertEqual("public", visibility[("Thing", "Go", 1)])
            self.assertTrue(any(item.name == "Go" and item.visibility == "public" for item in callables))
            self.assertTrue(any(item.name == "local" and item.visibility == "public" for item in callables))

    def test_python_signature_visitor_and_discovery(self):
        tree = ast.parse("class Outer:\n def method(self, x=2, *, named, opt=3, **more): pass\n async def later(a, /, *rest): pass\ndef top():\n def nested(): pass\n")
        method = tree.body[0].body[0]
        self.assertEqual("method(self, x=<default>, *, named, opt=<default>, **more)",
                         self.inventory.python_signature(method))
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            scripts = root / "scripts"
            evacam = root / "evacam"
            scripts.mkdir()
            evacam.mkdir()
            file = scripts / "fixture.py"
            file.write_text("class Outer:\n def method(self): pass\nasync def later(x): pass\n", encoding="utf-8")
            with mock.patch.object(self.inventory, "ROOT", root), \
                 mock.patch.object(self.inventory, "PYTHON_ROOTS", (evacam, scripts)):
                visitor = self.inventory.PythonCallableVisitor(file)
                visitor.visit(tree)
                # Direct calls keep each visitor hook visible to the inventory's
                # conservative Python reference scanner.
                self.inventory.PythonCallableVisitor.__init__(visitor, file)
                visitor.visit_ClassDef(tree.body[0])
                visitor.visit_FunctionDef(tree.body[1])
                visitor.visit_AsyncFunctionDef(tree.body[0].body[1])
                visitor._add_function(tree.body[1])
                discovered = self.inventory.python_callables()
        self.assertIn("method", [item.name for item in visitor.callables])
        self.assertIn("top", [item.name for item in visitor.callables])
        self.assertIn("later", [item.name for item in visitor.callables])
        self.assertEqual("Outer::method(self)", discovered[0].signature)
        self.assertEqual("later(x)", discovered[1].signature)

    def test_make_targets_and_named_references_find_reachable_named_tests(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            tests = root / "tests"
            tests.mkdir()
            (root / "Makefile").write_text("test-fixture: tests/FixtureTest.cpp tests/test_fixture.py\n", encoding="utf-8")
            (tests / "FixtureTest.cpp").write_text(
                "void Helper() { Work(1); }\nvoid TestReachable() { Helper(); }\nvoid main() { Ignored(); }\n", encoding="utf-8")
            (tests / "test_fixture.py").write_text("def test_python():\n    worker(1, key=2)\n", encoding="utf-8")
            with mock.patch.object(self.inventory, "ROOT", root), mock.patch.object(self.inventory, "TEST_ROOT", tests):
                self.assertEqual("test-fixture", self.inventory.make_targets()["tests/FixtureTest.cpp"])
                references = self.inventory.named_test_references(self.parser)
        self.assertEqual("TestReachable", references["Work"][0].case)
        self.assertEqual(1, references["Work"][0].arity)
        self.assertEqual(2, references["worker"][0].arity)
        self.assertNotIn("Ignored", references)

    def test_deduplicate_and_render_classify_coverage_exemptions_and_missing(self):
        callable_ = self.inventory.Callable
        reference = self.inventory.TestReference
        values = [
            callable_("src/Widget.cpp", 2, "Widget::Run()", "Run", "public", "C++", owner="Widget"),
            callable_("src/Widget.cpp", 2, "Widget::Run()", "Run", "public", "C++", owner="Widget"),
            callable_("src/Widget.cpp", 3, "Widget::Skip()", "Skip", "public", "C++", exemption="no behavior", owner="Widget"),
            callable_("src/Mystery.cpp", 4, "Mystery()", "Mystery", "private", "C++"),
        ]
        references = self.inventory.named_test_references(self.parser)
        references["Widget::Run"] = [reference("tests/WidgetTest.cpp", "TestRunsFast", "test-widget", 0)]
        with mock.patch.object(self.inventory, "TEST_ROOT", REPO_ROOT / "tests"):
            rendered = self.inventory.render(self.inventory.deduplicate(values), references)
        self.assertIn("Widget::Run()\tpublic\tcovered", rendered)
        self.assertIn("Skip()\tpublic\texempt", rendered)
        self.assertIn("Mystery()\tprivate\tmissing", rendered)
        self.assertIn("runs fast", rendered)

    def test_parse_args_and_main_write_and_check_without_repository_mutation(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            output = root / "inventory.tsv"
            callable_ = self.inventory.Callable("scripts/demo.py", 1, "demo()", "demo", "public", "Python")
            generated = (
                "production_file\tline\tlanguage\tqualified_callable\tvisibility\tstatus\t"
                "dedicated_test_case\ttest_target\tbehaviors_or_branches_checked\t"
                "exemption_or_refactor_note\n"
                "scripts/demo.py\t1\tPython\tdemo()\tpublic\tmissing\t\t\t\tAdd a dedicated test\n"
            )
            with mock.patch.object(sys, "argv", ["generator", "--output", str(output), "--check"]):
                self.assertEqual(output, self.inventory.parse_args().output)
            with mock.patch.object(self.inventory, "ROOT", root), \
                 mock.patch.object(self.inventory, "parse_args", return_value=type("Args", (), {"output": output, "check": False})()), \
                 mock.patch.object(self.inventory, "get_parser", return_value=object()), \
                 mock.patch.object(self.inventory, "cpp_callables", return_value=[]), \
                 mock.patch.object(self.inventory, "python_callables", return_value=[callable_]), \
                 mock.patch.object(self.inventory, "named_test_references", return_value={}), \
                 mock.patch.object(self.inventory, "render", return_value=generated):
                self.assertEqual(0, self.inventory.main())
            with mock.patch.object(self.inventory, "ROOT", root), \
                 mock.patch.object(self.inventory, "parse_args", return_value=type("Args", (), {"output": output, "check": True})()), \
                 mock.patch.object(self.inventory, "get_parser", return_value=object()), \
                 mock.patch.object(self.inventory, "cpp_callables", return_value=[]), \
                 mock.patch.object(self.inventory, "python_callables", return_value=[callable_]), \
                 mock.patch.object(self.inventory, "named_test_references", return_value={}), \
                 mock.patch.object(self.inventory, "render", return_value=generated + "changed\n"), \
                 contextlib.redirect_stderr(io.StringIO()) as errors:
                self.assertEqual(1, self.inventory.main())
            self.assertIn("is stale", errors.getvalue())
            self.assertEqual(generated, output.read_text(encoding="utf-8"))
            with mock.patch.object(self.inventory, "ROOT", root), \
                 mock.patch.object(self.inventory, "parse_args", return_value=type("Args", (), {"output": output, "check": True})()), \
                 mock.patch.object(self.inventory, "get_parser", return_value=object()), \
                 mock.patch.object(self.inventory, "cpp_callables", return_value=[]), \
                 mock.patch.object(self.inventory, "python_callables", return_value=[callable_]), \
                 mock.patch.object(self.inventory, "named_test_references", return_value={}), \
                 mock.patch.object(self.inventory, "render", return_value=generated):
                self.assertEqual(0, self.inventory.main())


if __name__ == "__main__":
    unittest.main()
