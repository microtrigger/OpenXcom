# SPDX-License-Identifier: GPL-3.0-or-later
"""Transfer loop lifetime doubles; no real transfer/socket or timeout proof."""
import ast
import copy
import inspect
from pathlib import Path
import hashlib
import unittest
from unittest import mock

import test_agent_transfer as transfer
import test_projects_case_ownership as owned
from snapshots import Unavailable
from test_agent_control import state

BASELINE = '61a8ab2cc97143e13b78338b8f63c3b2eab33715'
METHODS = ('test_every_action_fresh_dispatch_and_cache_clear',
           'test_direction_gate_and_unoffered_rows_before_send')


def method_node(source, name):
    cls = next(n for n in ast.parse(source).body if isinstance(n, ast.ClassDef) and n.name == 'TransferTests')
    return next(n for n in cls.body if isinstance(n, ast.FunctionDef) and n.name == name)


def old_source():
    # Exact archived parent source, restored by reversing its original commit
    # patch, not reconstructed from the candidate under test. See fixture README.
    data = (Path(__file__).resolve().parent / 'fixtures/ownership/transfer-61a8ab2c.py.txt').read_bytes()
    if hashlib.sha256(data).hexdigest() != '7af69e5118cf7fdca3194f337285b8f394aafc494eae1b8f744c6878a4b06bba':
        raise AssertionError('pinned_ownership_baseline_mismatch')
    return data.decode('utf-8')

def old_method(name):
    node = method_node(old_source(), name)
    namespace = dict(vars(transfer))
    exec(compile(ast.fix_missing_locations(ast.Module(body=[node], type_ignores=[])),
                 '<baseline-transfer-owned-fake>', 'exec'), namespace)
    return namespace[name]


class TransferOwner(owned.FakeOwner):
    def __init__(self, factory, ordinal, *, screen, transform=None):
        super().__init__(factory, ordinal, screen=screen, transform=transform)
        self.requests, self.current = [], None
        self.observations, self.attempts = 0, 0
        self.transfer_rows, self.visible_rows, self.scroll_directions = set(), set(), set()
        self.capability = None

    def observe(self):
        owned.check(not self.closed, 'observe_after_close')
        if self.ordinal == 0 and self.factory.body_fault == 'observe':
            raise ValueError('fixed_body_observe')
        self.observations += 1
        obj = state(screen=self.screen, revision=self.observations,
                    capability=f'{self.observations:032x}')
        if self.transform:
            obj = self.transform('R', obj)
        self.current = obj
        self.capability = obj['capability']
        self.transfer_rows = {(r['handle'], direction) for r in obj['visible'].get('rows', [])
                              for direction in r.get('controls', [])}
        self.visible_rows = {r['handle'] for r in obj['visible'].get('rows', [])}
        self.scroll_directions = {k for k, v in obj['visible'].get('scroll', {}).items() if v}
        return obj

    def execute(self, **kwargs):
        owned.check(not self.closed, 'execute_after_close')
        self.attempts += 1
        if any(kwargs[k] != self.current[k] for k in ('epoch', 'capability', 'revision')):
            raise Unavailable('Unavailable')
        args, action = kwargs['args'], kwargs['action']
        if action == 'change_transfer_quantity':
            operation = args['operation']
            required = ({'increase'} if operation == 'max' else {'decrease'} if operation == 'clear'
                        else {'increase', 'decrease'} if operation == 'set'
                        else {'increase'} if args['value'] > 0 else {'decrease'})
            if not all((args['row'], direction) in self.transfer_rows for direction in required):
                raise Unavailable('Unavailable')
        if self.ordinal == 0 and self.factory.body_fault == 'execute':
            raise ValueError('fixed_body_execute')
        self.requests.append(b'E synthetic')
        self.transfer_rows, self.visible_rows, self.scroll_directions = set(), set(), set()
        self.capability = None
        if self.ordinal == 0 and self.factory.body_fault == 'assertion':
            self.transfer_rows = {'fixed_invalid_cache'}
        return {'status': 'dispatched'}


class TransferFactory(owned.Factory):
    def __call__(self, **kwargs):
        if self.owners:
            self.previous_closed.append(self.owners[-1].closed)
            self.previous_finished.append(self.owners[-1].finished.is_set())
        owner = TransferOwner(self, len(self.owners), **kwargs)
        self.owners.append(owner)  # Before start/readiness, including failures.
        self.events.append(('create', owner.ordinal))
        owner.start()
        return owner


def make_case(method):
    return type('OwnedTransferCase', (unittest.TestCase,), {
        'runTest': method, 'fixture': transfer.TransferTests.fixture})()


class TransferCaseOwnershipTests(unittest.TestCase):
    def test_pinned_baseline_tamper_and_missing_fail_closed(self):
        with mock.patch.object(Path, 'read_bytes', return_value=b'not historical source'):
            with self.assertRaisesRegex(AssertionError, '^pinned_ownership_baseline_mismatch$'):
                old_source()
        with mock.patch.object(Path, 'read_bytes', side_effect=FileNotFoundError('missing fixture')):
            with self.assertRaises(FileNotFoundError):
                old_source()

    def candidate(self, name):
        return getattr(transfer.TransferTests, name)

    def test_normal_loops_close_once_and_trailing_peer_is_shared(self):
        for name in METHODS:
            with self.subTest(method=name):
                factory = TransferFactory()
                case = make_case(self.candidate(name))
                try:
                    with mock.patch.object(transfer, 'NativeFixture', factory):
                        case.runTest()
                    self.assertGreater(len(factory.owners), 1)
                    self.assertTrue(all(factory.previous_closed))
                    if name == METHODS[1]:
                        self.assertEqual(len(factory.owners), 13)
                        tail = factory.owners[-1]
                        self.assertFalse(tail.closed)
                        self.assertEqual((tail.observations, tail.attempts, tail.requests), (2, 2, []))
                        self.assertEqual(len(case._cleanups), 1)
                    else:
                        self.assertEqual(case._cleanups, [])
                    self.assertTrue(case.doCleanups())
                    self.assertTrue(all(o.closed and o.close_calls == 1 for o in factory.owners))
                finally:
                    factory.finish()

    def test_body_failures_close_once_and_stop(self):
        for name in METHODS:
            for fault in ('observe', 'execute'):
                with self.subTest(method=name, fault=fault):
                    factory = TransferFactory(body_fault=fault)
                    case = make_case(self.candidate(name))
                    try:
                        with mock.patch.object(transfer, 'NativeFixture', factory), self.assertRaises(ValueError):
                            case.runTest()
                        self.assertEqual(len(factory.owners), 1)
                        self.assertEqual(factory.owners[0].close_calls, 1)
                        self.assertEqual(case._cleanups, [])
                    finally:
                        factory.finish()

    def test_original_cache_assertion_failure_still_closes(self):
        factory = TransferFactory(body_fault='assertion')
        try:
            with mock.patch.object(transfer, 'NativeFixture', factory), self.assertRaises(AssertionError):
                make_case(self.candidate(METHODS[0])).runTest()
            self.assertEqual(len(factory.owners), 1)
            self.assertEqual(factory.owners[0].close_calls, 1)
        finally:
            factory.finish()

    def test_cleanup_failure_stops_each_method(self):
        for name in METHODS:
            with self.subTest(method=name):
                factory = TransferFactory(cleanup_fault=True)
                try:
                    with mock.patch.object(transfer, 'NativeFixture', factory), self.assertRaisesRegex(RuntimeError, '^fixed_cleanup_failure$'):
                        make_case(self.candidate(name)).runTest()
                    self.assertEqual(len(factory.owners), 1)
                    self.assertEqual(factory.owners[0].close_calls, 1)
                finally:
                    factory.finish()

    def test_body_and_cleanup_chain_is_preserved(self):
        for name in METHODS:
            with self.subTest(method=name):
                factory = TransferFactory(body_fault='observe', cleanup_fault=True)
                try:
                    with mock.patch.object(transfer, 'NativeFixture', factory):
                        try:
                            make_case(self.candidate(name)).runTest()
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

    def test_exact_two_ownership_scaffolds_only_ast_delta(self):
        baseline, current = old_source(), inspect.getsource(transfer)
        for name in METHODS:
            with self.subTest(method=name):
                old, new = method_node(baseline, name), method_node(current, name)
                restored = copy.deepcopy(new)
                sites = 0
                for loop in ast.walk(restored):
                    if not isinstance(loop, ast.For) or len(loop.body) != 2 or not isinstance(loop.body[1], ast.Try):
                        continue
                    create, guarded = loop.body
                    self.assertEqual(ast.dump(create.value.func), ast.dump(ast.Name(id='NativeFixture', ctx=ast.Load())))
                    self.assertEqual(guarded.handlers, [])
                    self.assertEqual(guarded.orelse, [])
                    self.assertEqual(len(guarded.finalbody), 1)
                    self.assertEqual(ast.dump(guarded.finalbody[0]), ast.dump(ast.parse('peer.close()').body[0]))
                    create.value.func = ast.Attribute(value=ast.Name(id='self', ctx=ast.Load()), attr='fixture', ctx=ast.Load())
                    loop.body = [create] + guarded.body
                    sites += 1
                self.assertEqual(sites, 1)
                self.assertEqual(ast.dump(old), ast.dump(restored))
                self.assertFalse(any(isinstance(n, ast.Assert) for n in ast.walk(new)))

    def test_controlled_barriers_baseline_retains_candidate_releases_loops(self):
        for name in METHODS:
            observations = {}
            for variant, method in [('baseline', old_method(name)), ('candidate', self.candidate(name))]:
                factory = TransferFactory(barrier=True)
                case = make_case(method)
                try:
                    with mock.patch.dict(method.__globals__, {'NativeFixture': factory}), mock.patch.object(transfer, 'NativeFixture', factory):
                        case.runTest()
                    self.assertGreater(len(factory.owners), 1)
                    observations[variant] = (list(factory.previous_closed), list(factory.previous_finished))
                    if name == METHODS[1]:
                        self.assertFalse(factory.owners[-1].closed)
                        self.assertEqual(factory.owners[-1].observations, 2)
                    self.assertTrue(case.doCleanups())
                finally:
                    factory.finish()
                self.assertTrue(all(o.closed and o.finished.is_set() and not o.thread.is_alive() for o in factory.owners))
                self.assertTrue(all(o.close_calls == 1 for o in factory.owners))
            self.assertTrue(all(not x for x in observations['baseline'][0]))
            self.assertTrue(all(not x for x in observations['baseline'][1]))
            self.assertTrue(all(observations['candidate'][0]))
            self.assertTrue(all(observations['candidate'][1]))


if __name__ == '__main__':
    unittest.main()
