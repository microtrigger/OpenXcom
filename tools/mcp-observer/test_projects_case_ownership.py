# SPDX-License-Identifier: GPL-3.0-or-later
"""Deterministic fake-owned Projects case lifetime; no socket/game timeout proof."""
import ast
import copy
import inspect
from pathlib import Path
import subprocess
import threading
import unittest
from unittest import mock

import test_agent_projects as projects
from snapshots import Unavailable
from test_agent_control import state

BASELINE = '639679d42155f27680356b5f9241106cab08c4b7'
METHOD = 'test_direction_membership_from_current_button_only'
BOUND = 2


def check(value, message):
    if not value:
        raise AssertionError(message)


def method_node(source):
    cls = next(n for n in ast.parse(source).body
               if isinstance(n, ast.ClassDef) and n.name == 'ProjectsClientTests')
    return next(n for n in cls.body if isinstance(n, ast.FunctionDef) and n.name == METHOD)


def baseline_source():
    return subprocess.run(['git', '-C', str(Path(__file__).resolve().parent), 'show',
                           BASELINE + ':tools/mcp-observer/test_agent_projects.py'],
                          check=True, capture_output=True, text=True).stdout


def baseline_method():
    node = method_node(baseline_source())
    namespace = dict(vars(projects))
    exec(compile(ast.fix_missing_locations(ast.Module(body=[node], type_ignores=[])),
                 '<baseline-projects-owned-fake>', 'exec'), namespace)
    return namespace[METHOD]


class FakeOwner:
    def __init__(self, factory, ordinal, *, screen, transform):
        self.factory, self.ordinal = factory, ordinal
        self.screen, self.transform = screen, transform
        self.client = self
        self.closed = False
        self.close_calls = 0
        self.staff_directions = set()
        self.directions = []
        self.ready, self.release, self.finished = threading.Event(), threading.Event(), threading.Event()
        self.worker_error = None
        self.thread = None
        self.started = False

    def start(self):
        if not self.factory.barrier:
            return
        def work():
            try:
                self.ready.set()
                if not self.release.wait(BOUND):
                    self.worker_error = 'release_barrier_failed'
            finally:
                self.finished.set()
        self.thread = threading.Thread(target=work)
        self.thread.start()
        self.started = True
        check(self.ready.wait(BOUND), 'ready_barrier_failed')

    def observe(self):
        if self.ordinal == 0 and self.factory.body_fault == 'observe':
            raise ValueError('fixed_body_observe')
        obj = self.transform('R', state(screen=self.screen))
        self.staff_directions = {b['id'] for b in obj['visible']['buttons']
                                 if b['id'] in ('increase', 'decrease')}
        if self.ordinal == 0 and self.factory.body_fault == 'assertion':
            self.staff_directions = set()
        return obj

    def execute(self, **kwargs):
        direction = kwargs['args']['direction']
        self.directions.append(direction)
        if direction == 'increase':
            raise Unavailable('Unavailable')
        check(direction == 'decrease', 'unexpected_direction')
        if self.ordinal == 0 and self.factory.body_fault == 'execute':
            raise ValueError('fixed_body_execute')
        return {'status': 'dispatched'}

    def close(self):
        self.close_calls += 1
        check(not self.closed, 'duplicate_close')
        self.release.set()
        if self.started:
            self.thread.join(BOUND)
            check(not self.thread.is_alive(), 'worker_join_failed')
        self.closed = True
        self.factory.events.append(('close', self.ordinal))
        check(self.worker_error is None, 'worker_barrier_failed')
        if self.ordinal == 0 and self.factory.cleanup_fault:
            raise RuntimeError('fixed_cleanup_failure')


class Factory:
    def __init__(self, *, barrier=False, body_fault=None, cleanup_fault=False):
        self.barrier, self.body_fault, self.cleanup_fault = barrier, body_fault, cleanup_fault
        self.owners, self.events, self.previous_closed, self.previous_finished = [], [], [], []

    def __call__(self, **kwargs):
        if self.owners:
            self.previous_closed.append(self.owners[-1].closed)
            self.previous_finished.append(self.owners[-1].finished.is_set())
        owner = FakeOwner(self, len(self.owners), **kwargs)
        # Register ownership before starting or waiting: readiness failure is owned.
        self.owners.append(owner)
        self.events.append(('create', owner.ordinal))
        owner.start()
        return owner

    def finish(self):
        first = None
        for owner in self.owners:
            if owner.closed:
                continue
            try:
                owner.close()
            except BaseException:
                # Fixed reason only; continue releasing all remaining owners.
                if first is None:
                    first = 'outer_cleanup_failure'
        if first is not None:
            raise AssertionError(first)


def make_case(method):
    cls = type('OwnedCase', (unittest.TestCase,), {
        'runTest': method, 'fixture': projects.ProjectsClientTests.fixture})
    return cls()


class ProjectsCaseOwnershipTests(unittest.TestCase):
    def candidate(self):
        return getattr(projects.ProjectsClientTests, METHOD)

    def test_normal_case_end_closes_once_before_next_case(self):
        factory = Factory()
        case = make_case(self.candidate())
        try:
            with mock.patch.object(projects, 'NativeFixture', factory):
                case.runTest()
            self.assertGreater(len(factory.owners), 1)
            self.assertTrue(all(factory.previous_closed))
            self.assertEqual(case._cleanups, [])
            for owner in factory.owners:
                self.assertEqual(owner.close_calls, 1)
                self.assertEqual(owner.directions, ['increase', 'decrease'])
        finally:
            factory.finish()

    def test_body_failures_close_once_and_stop(self):
        for fault, expected in [('observe', ValueError), ('assertion', AssertionError), ('execute', ValueError)]:
            with self.subTest(fault=fault):
                factory = Factory(body_fault=fault)
                case = make_case(self.candidate())
                try:
                    with mock.patch.object(projects, 'NativeFixture', factory), self.assertRaises(expected):
                        case.runTest()
                    self.assertEqual(len(factory.owners), 1)
                    self.assertEqual(factory.owners[0].close_calls, 1)
                    self.assertEqual(case._cleanups, [])
                finally:
                    factory.finish()

    def test_cleanup_failure_stops_before_next_case(self):
        factory = Factory(cleanup_fault=True)
        try:
            with mock.patch.object(projects, 'NativeFixture', factory), self.assertRaisesRegex(RuntimeError, '^fixed_cleanup_failure$'):
                make_case(self.candidate()).runTest()
            self.assertEqual(len(factory.owners), 1)
            self.assertEqual(factory.owners[0].close_calls, 1)
        finally:
            factory.finish()

    def test_body_and_cleanup_failure_chain_retains_both(self):
        factory = Factory(body_fault='observe', cleanup_fault=True)
        try:
            with mock.patch.object(projects, 'NativeFixture', factory):
                try:
                    make_case(self.candidate()).runTest()
                except RuntimeError as error:
                    self.assertEqual(str(error), 'fixed_cleanup_failure')
                    self.assertIsInstance(error.__context__, ValueError)
                    self.assertEqual(str(error.__context__), 'fixed_body_observe')
                else:
                    self.fail('missing_cleanup_failure')
            self.assertEqual(len(factory.owners), 1)
            self.assertEqual(factory.owners[0].close_calls, 1)
        finally:
            factory.finish()

    def test_ownership_scaffolding_is_only_method_ast_delta(self):
        old = method_node(baseline_source())
        current = method_node(inspect.getsource(projects))
        loop = current.body[-1]
        self.assertIsInstance(loop, ast.For)
        create, guarded = loop.body
        self.assertEqual(ast.dump(create.value.func), ast.dump(ast.Name(id='NativeFixture', ctx=ast.Load())))
        self.assertIsInstance(guarded, ast.Try)
        self.assertEqual(guarded.handlers, [])
        self.assertEqual(guarded.orelse, [])
        self.assertEqual(ast.dump(guarded.finalbody[0]), ast.dump(ast.parse('peer.close()').body[0]))
        self.assertEqual(len(guarded.finalbody), 1)
        restored = copy.deepcopy(current)
        restored.body[-1].body = [copy.deepcopy(create)] + copy.deepcopy(guarded.body)
        restored.body[-1].body[0].value.func = ast.Attribute(value=ast.Name(id='self', ctx=ast.Load()), attr='fixture', ctx=ast.Load())
        self.assertEqual(ast.dump(old), ast.dump(restored))
        self.assertFalse(any(isinstance(n, ast.Assert) for n in ast.walk(current)))

    def test_controlled_barrier_baseline_retained_candidate_released(self):
        observations = {}
        for variant, method in [('baseline', baseline_method()), ('candidate', self.candidate())]:
            factory = Factory(barrier=True)
            case = make_case(method)
            # Baseline method has its own globals dictionary copied from module.
            globals_dict = method.__globals__
            try:
                with mock.patch.dict(globals_dict, {'NativeFixture': factory}), mock.patch.object(projects, 'NativeFixture', factory):
                    case.runTest()
                self.assertGreater(len(factory.owners), 1)
                observations[variant] = (list(factory.previous_closed), list(factory.previous_finished))
                self.assertTrue(case.doCleanups())
            finally:
                factory.finish()
            self.assertTrue(all(o.closed and o.finished.is_set() and not o.thread.is_alive() for o in factory.owners))
            self.assertTrue(all(o.close_calls == 1 for o in factory.owners))
        self.assertTrue(all(not x for x in observations['baseline'][0]))
        self.assertTrue(all(not x for x in observations['baseline'][1]))
        self.assertTrue(all(observations['candidate'][0]))
        self.assertTrue(all(observations['candidate'][1]))

    def test_prestart_failure_is_registered_and_outer_cleanup_continues(self):
        factory = Factory()
        with mock.patch.object(FakeOwner, 'start', side_effect=AssertionError('fixed_prestart_failure')):
            with self.assertRaisesRegex(AssertionError, '^fixed_prestart_failure$'):
                factory(screen='research_info', transform=lambda op, obj: obj)
        self.assertEqual(len(factory.owners), 1)
        factory.finish()
        self.assertTrue(factory.owners[0].closed)
        self.assertEqual(factory.owners[0].close_calls, 1)
        # Multiple still-owned peers are cleaned even when the first close fails.
        factory = Factory(cleanup_fault=True)
        for _ in range(2):
            factory(screen='research_info', transform=lambda op, obj: obj)
        with self.assertRaisesRegex(AssertionError, '^outer_cleanup_failure$'):
            factory.finish()
        self.assertTrue(all(o.closed and o.close_calls == 1 for o in factory.owners))


if __name__ == '__main__':
    unittest.main()
