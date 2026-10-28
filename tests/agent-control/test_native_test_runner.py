import copy
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest

SPEC = importlib.util.spec_from_file_location('runner', Path(__file__).with_name('native_test_runner.py'))
runner = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(runner)


class NativeRunnerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.source = self.root / 'source tree'
        self.source.mkdir()
        self.input = self.source / 'input.txt'
        self.input.write_text('unchanged', encoding='utf-8')
        self.output = self.root / 'test output'

    def recipe(self, code='print("passed")', args=()):
        return dict(version=1, source=str(self.source), output=str(self.output),
                    inputs=[str(self.input)], steps=[dict(name='test',
                    argv=[sys.executable, '-c', code, *args], cwd=str(self.output), env={})])

    def test_success_receipt_logs_and_environment_overlay(self):
        r = self.recipe('import os; print(os.environ["RUNNER_TEST_VALUE"])')
        r['steps'][0]['env'] = {'RUNNER_TEST_VALUE': 'explicit overlay'}
        result = runner.run_recipe(r)
        self.assertTrue(result['all_passed'])
        self.assertTrue(result['inputs_unchanged'])
        self.assertEqual(result['steps'][0]['exit_code'], 0)
        self.assertEqual((self.output / 'test.log').read_text().strip(), 'explicit overlay')
        self.assertEqual(json.loads((self.output / 'RESULT.json').read_text()), result)
        self.assertNotIn('RUNNER_TEST_VALUE', (self.output / 'RESULT.json').read_text())

    def test_failure_stops_subsequent_steps(self):
        r = self.recipe('import sys; print("failed");sys.exit(7)')
        r['steps'].append(dict(name='later', argv=[sys.executable, '-c', 'print("never")'], cwd=str(self.output), env={}))
        result = runner.run_recipe(r)
        self.assertFalse(result['all_passed'])
        self.assertEqual(result['steps'][0]['exit_code'], 7)
        self.assertEqual(len(result['steps']), 1)
        self.assertFalse((self.output / 'later.log').exists())

    def test_input_mutation_and_deletion_fail_even_zero_exit(self):
        for operation in ('write_text("changed")', 'unlink()'):
            with self.subTest(operation=operation):
                self.output = self.root / ('out-' + str(len(operation)))
                self.input.write_text('before')
                result = runner.run_recipe(self.recipe('from pathlib import Path;import sys;Path(sys.argv[1]).' + operation, (str(self.input),)))
                self.assertEqual(result['steps'][0]['exit_code'], 0)
                self.assertFalse(result['inputs_unchanged'])
                self.assertFalse(result['all_passed'])

    def test_collision_and_inside_source_refused_without_commands(self):
        self.output.mkdir()
        marker = self.output / 'keep'
        marker.write_text('preserve')
        with self.assertRaises(ValueError):
            runner.run_recipe(self.recipe())
        self.assertEqual(marker.read_text(), 'preserve')
        self.output = self.source / 'nested output'
        with self.assertRaises(ValueError):
            runner.run_recipe(self.recipe())
        self.assertFalse(self.output.exists())

    def test_metacharacters_and_spaces_are_literal_argv_not_shell(self):
        sentinel = self.root / 'injected'
        argument = 'spaces ; touch ' + str(sentinel) + ' $(echo dangerous) & | >'
        result = runner.run_recipe(self.recipe('import sys; print(repr(sys.argv[1]))', (argument,)))
        self.assertTrue(result['all_passed'])
        self.assertEqual((self.output / 'test.log').read_text().strip(), repr(argument))
        self.assertFalse(sentinel.exists())

    def test_launch_failure_receipt(self):
        r = self.recipe()
        r['steps'][0]['argv'] = [str(self.root / 'nonexistent executable')]
        result = runner.run_recipe(r)
        self.assertFalse(result['all_passed'])
        self.assertEqual(result['steps'][0]['launch_error'], 'FileNotFoundError')
        self.assertIsNone(result['steps'][0]['exit_code'])

    def test_strict_recipe_and_duplicate_json_keys(self):
        bads = []
        r = self.recipe(); r['shell'] = True; bads.append(r)
        r = self.recipe(); r['version'] = True; bads.append(r)
        r = self.recipe(); r['steps'][0]['name'] = '../escape'; bads.append(r)
        r = self.recipe(); r['steps'][0]['argv'] = 'echo unsafe'; bads.append(r)
        r = self.recipe(); r['steps'][0]['env'] = {'BAD=KEY': 'x'}; bads.append(r)
        r = self.recipe(); r['inputs'].append(str(self.input)); bads.append(r)
        r = self.recipe(); r['steps'].append(copy.deepcopy(r['steps'][0])); bads.append(r)
        for recipe in bads:
            with self.assertRaises(ValueError):
                runner.run_recipe(recipe)
        self.assertFalse(self.output.exists())
        file = self.root / 'bad.json'
        file.write_text('{"version":1,"version":1}')
        with self.assertRaises(ValueError):
            runner.load_recipe(file)
        file.write_text('{"value":NaN}')
        with self.assertRaises(ValueError):
            runner.load_recipe(file)

    def test_cli_success_and_refusal(self):
        file = self.root / 'recipe.json'
        file.write_text(json.dumps(self.recipe()))
        self.assertEqual(runner.main([str(file)]), 0)
        self.assertEqual(runner.main([str(file)]), 2)


if __name__ == '__main__':
    unittest.main()
