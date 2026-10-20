#!/usr/bin/env python3
"""Inventory source contracts and /10 branch-preserving extraction parity.

These checks are source evidence, not live game/paint/SDL acceptance. The paired
native runner beside this file compiles the extracted method bodies with declared
model/UI dependency shims. Only the isolated /11 source tree is writable.
"""
from pathlib import Path
import hashlib
import os
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / 'src'
BASELINE = Path(os.environ.get('INVENTORY_V10_SOURCE_ROOT', str(
    ROOT.parent.parent / 'agent-control-impact-v10-20261018' / 'source')))

# Frozen /10 Inventory.cpp from the final isolated checkpoint, read-only input.
BASELINE_SHA256 = '5e50ddfcd2769cc74c2129a7d49a4519e321b6f60f66fdd6aa5c86f80a2b57cc'


def read(relative):
    return (SRC / relative).read_text()


def masked(text):
    """Mask comments and C++ literals without changing brace positions."""
    return re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',
                  lambda m: ' ' * len(m.group()), text, flags=re.S)


def function(text, qualified, signature=None):
    """Extract an entire definition, preserving source bytes inside its body."""
    clean = masked(text)
    matches = re.finditer(r'(?m)^[^\n;{}]*\b' + re.escape(qualified) + r'\s*\(', clean)
    match = next((m for m in matches if signature is None or signature in text[m.start():clean.index('{', m.start())]), None)
    if not match:
        raise AssertionError('Missing function: ' + qualified + ' ' + str(signature))
    left = clean.index('{', match.start())
    depth = 1
    for right in range(left + 1, len(clean)):
        depth += (clean[right] == '{') - (clean[right] == '}')
        if not depth:
            return text[match.start():right + 1]
    raise AssertionError('Unclosed function: ' + qualified)


def body(text, qualified, signature=None):
    whole = function(text, qualified, signature)
    return whole[whole.index('{') + 1:-1]


def tokens(text):
    """Preserve string values and every operator/branch; ignore only comments/space."""
    text = re.sub(r'//[^\n]*|/\*.*?\*/', '', text, flags=re.S)
    return re.findall(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|\w+|[^\w\s]', text)


def without_instrumentation(text):
    # These are the exact additional generation hook and pre-existing supervised
    # entry hooks. Other #if blocks are not normalized away.
    for flag in ('OPENXCOM_EXPERIMENTAL_AGENT_CONTROL',
                 'OPENXCOM_EXPERIMENTAL_INVENTORY_CLICK_CONTROL_GATE',
                 'OPENXCOM_EXPERIMENTAL_INVENTORY_RIGHT_CLICK_CONTROL_GATE',
                 'OPENXCOM_EXPERIMENTAL_INVENTORY_UNLOAD_CONTROL_GATE'):
        text = re.sub(r'#ifdef ' + flag + r'\n.*?#endif\n', '', text, flags=re.S)
    return text


def normal_pointer(text):
    text = without_instrumentation(text)
    # Algebraic reassociation is valid for the bounded UI coordinates tested by
    # the native harness. No gameplay expression/condition is removed.
    for axis in ('X', 'Y'):
        text = text.replace('(int)floor(action->getAbsolute' + axis + 'Mouse()) - '
                            '_selection->get' + ('Width' if axis == 'X' else 'Height') + '()/2 - get' + axis + '()',
                            'local' + axis + ' - _selection->get' + ('Width' if axis == 'X' else 'Height') + '()/2')
        text = text.replace('(int)floor(action->getAbsolute' + axis + 'Mouse()) - get' + axis + '()', 'local' + axis)
    text = text.replace('return;', 'return false;')
    text = text.replace('InteractiveSurface::mouseOver(action, state);', 'return true;')
    return tokens(text)


def normal_activation(text, extracted=False):
    text = without_instrumentation(text)
    for axis in ('X', 'Y'):
        text = text.replace('(int)floor(action->getAbsolute' + axis + 'Mouse()) - get' + axis + '()', 'local' + axis)
    text = text.replace('action->getDetails()->button.button', 'button')
    text = text.replace('(SDL_GetModState() & KMOD_CTRL)', 'control')
    text = text.replace('return;', 'return false;')
    text = text.replace('InteractiveSurface::mouseClick(action, state);', 'return true;')
    # Expand exactly the held branch moved to returnHeldItem. Its new empty-hold
    # guard and success boolean are tested independently below.
    if extracted:
        text = text.replace('returnHeldItem();', '''
            if (_selItem->getSlot()->getType() == INV_GROUND)
            {
                _stackLevel[_selItem->getSlotX()][_selItem->getSlotY()] += 1;
            }
            setSelectedItem(0);''')
    return tokens(text)


class InventoryRoutingArchitectureTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.inventory = read('Battlescape/Inventory.cpp')
        cls.header = read('Battlescape/Inventory.h')
        cls.state = read('Battlescape/InventoryState.cpp')
        path = BASELINE / 'src/Battlescape/Inventory.cpp'
        cls.baseline = None
        if not path.is_file():
            return
        raw = path.read_bytes()
        if hashlib.sha256(raw).hexdigest() != BASELINE_SHA256:
            raise AssertionError('Frozen /10 Inventory.cpp seal does not match')
        cls.baseline = raw.decode()

    def require_baseline(self):
        if self.baseline is None:
            self.skipTest('Frozen /10 input is unavailable; set INVENTORY_V10_SOURCE_ROOT')

    def test_pointer_extraction_preserves_every_ordinary_token(self):
        self.require_baseline()
        self.assertEqual(normal_pointer(body(self.baseline, 'Inventory::mouseOver')),
                         normal_pointer(body(self.inventory, 'Inventory::movePointer')))

    def test_activation_extraction_preserves_every_ordinary_branch(self):
        self.require_baseline()
        self.assertEqual(normal_activation(body(self.baseline, 'Inventory::mouseClick')),
                         normal_activation(body(self.inventory, 'Inventory::activateButton'), extracted=True))

    def test_unload_remains_the_ordinary_implementation(self):
        self.require_baseline()
        self.assertEqual(tokens(body(self.baseline, 'Inventory::unload')),
                         tokens(body(self.inventory, 'Inventory::unload')))

    def test_held_cancel_has_only_guard_stack_restore_clear_and_result(self):
        expected = '''if (!_selItem) return false;
            if (_selItem->getSlot()->getType() == INV_GROUND)
                _stackLevel[_selItem->getSlotX()][_selItem->getSlotY()] += 1;
            setSelectedItem(0); return true;'''
        self.assertEqual(tokens(body(self.inventory, 'Inventory::returnHeldItem')), tokens(expected))

    def test_physical_callbacks_remain_once_and_only_after_shared_path(self):
        for wrapper, shared in (('mouseOver', 'movePointer'), ('mouseClick', 'activateButton')):
            code = body(self.inventory, 'Inventory::' + wrapper)
            clean = without_instrumentation(code)
            self.assertEqual(clean.count(shared + '('), 1)
            self.assertEqual(clean.count('InteractiveSurface::' + wrapper + '('), 1)
            self.assertRegex(clean, r'if\s*\(' + shared + r'\(')
            self.assertLess(clean.index(shared), clean.index('InteractiveSurface::'))
            expected = {
                'mouseOver': 'if (movePointer((int)floor(action->getAbsoluteXMouse()) - getX(), (int)floor(action->getAbsoluteYMouse()) - getY())) InteractiveSurface::mouseOver(action, state);',
                'mouseClick': 'if (activateButton(action->getDetails()->button.button, (int)floor(action->getAbsoluteXMouse()) - getX(), (int)floor(action->getAbsoluteYMouse()) - getY(), (SDL_GetModState() & KMOD_CTRL) != 0)) InteractiveSurface::mouseClick(action, state);',
            }[wrapper]
            self.assertEqual(tokens(clean), tokens(expected))
            for gate in ('InventoryClickControlGate', 'InventoryRightClickControlGate', 'InventoryUnloadControlGate'):
                entry = 'inventoryMouseOverEntered' if wrapper == 'mouseOver' else 'inventoryClickEntered'
                self.assertEqual(code.count(gate + '::' + entry + '(this, action, state);'), 1)
                self.assertLess(code.index(gate), code.index(shared))

    def test_shared_paths_do_not_forge_or_reenter_physical_input(self):
        code = '\n'.join(body(self.inventory, 'Inventory::' + name) for name in
                         ('movePointer', 'activateButton', 'returnHeldItem'))
        for forbidden in ('SDL_Event', 'SDL_PushEvent', 'SDL_WarpMouse', 'SDL_GetModState',
                          'new Action', 'InteractiveSurface::mouse', 'mouseClick(', 'mouseOver('):
            self.assertNotIn(forbidden, code)
        self.assertIn('if (control)', code)  # preserved ordinary Ctrl branch
        self.assertIn('else if (button == SDL_BUTTON_RIGHT)', code)
        self.assertEqual(code.count('item->setAmmoItem(_selItem);'), 1)
        self.assertIn('_selUnit->spendTimeUnits(15)', code)

    def test_normalized_agent_instrumentation_is_revision_only(self):
        for name in ('movePointer', 'activateButton'):
            code = body(self.inventory, 'Inventory::' + name)
            blocks = re.findall(r'#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL\n(.*?)#endif', code, flags=re.S)
            self.assertEqual(len(blocks), 1)
            self.assertEqual(tokens(blocks[0]), tokens('++_agentInventoryRevision;'))

    def test_shared_methods_are_private_with_inventory_state_friend(self):
        private = self.header.split('private:', 1)[1].split('public:', 1)[0]
        self.assertIn('friend class InventoryState;', private)
        for declaration in ('bool movePointer(int localX, int localY);',
                            'bool activateButton(Uint8 button, int localX, int localY, bool control);',
                            'bool returnHeldItem();'):
            self.assertIn(declaration, private)


    def test_request_dispatch_uses_current_offer_and_fresh_target_point(self):
        code = body(self.state, 'InventoryState::agentDispatch', 'const Observer::AgentRequest &request')
        self.assertIn('if (!agentUiState(shown) || !shown.offers(request.action)) return false;', code)
        self.assertIn('if (n >= shown.points.size()) return false;', code)
        self.assertIn('const int x = shown.points[n].first, y = shown.points[n].second;', code)
        self.assertLess(code.index('shown.offers('), code.index('_inv->movePointer('))
        self.assertLess(code.index('n >= shown.points.size()'), code.index('_inv->movePointer('))
        for forbidden in ('getRules(', 'getAmmoItem(', 'setAmmoItem(', 'spendTimeUnits(',
                          'moveItem(', 'fitItem(', 'canBeStacked(', 'setFuseTimer(',
                          'SDL_PushEvent', 'SDL_Event', 'SDL_WarpMouse', 'new Action', 'SDL_BUTTON_RIGHT'):
            self.assertNotIn(forbidden, code)

    def test_request_dispatch_has_exact_shared_hover_and_primary_tails(self):
        code = body(self.state, 'InventoryState::agentDispatch', 'const Observer::AgentRequest &request')
        target = code.split('if (request.action == "inventory_hover" || request.action == "inventory_activate")', 1)[1].split('if (request.action == "cancel_inventory_hold")', 1)[0]
        self.assertEqual(target.count('_inv->movePointer('), 1)
        self.assertEqual(target.count('invMouseOver(nullptr)'), 1)
        self.assertEqual(target.count('_inv->activateButton('), 1)
        self.assertEqual(target.count('invClick(nullptr)'), 1)
        self.assertIn('if (_inv->movePointer(x,y)) invMouseOver(nullptr);', target)
        self.assertIn('if (request.action == "inventory_activate" && _inv->activateButton(SDL_BUTTON_LEFT,x,y,false)) invClick(nullptr);', target)
        self.assertLess(target.index('movePointer('), target.index('invMouseOver('))
        self.assertLess(target.index('invMouseOver('), target.index('activateButton('))
        self.assertLess(target.index('activateButton('), target.index('invClick('))

    def test_request_cancel_routes_only_held_return_with_one_callback(self):
        code = body(self.state, 'InventoryState::agentDispatch', 'const Observer::AgentRequest &request')
        cancel = code.split('if (request.action == "cancel_inventory_hold")', 1)[1].split('return agentDispatch(request.action);', 1)[0]
        self.assertEqual(tokens(cancel), tokens('{ if (_inv->returnHeldItem()) invClick(nullptr); return true; }'))

    def test_simple_dispatch_uses_only_normal_ok_unload_and_page_handlers(self):
        code = body(self.state, 'InventoryState::agentDispatch', 'const std::string &action')
        self.assertIn('if (!agentUiState(shown) || !shown.offers(action)) return false;', code)
        self.assertEqual(re.findall(r'\b(btn\w+)\(nullptr\)', code),
                         ['btnOkClick', 'btnUnloadClick', 'btnGroundClick'])
        for forbidden in ('unload()', 'arrangeGround(', 'getSelectedItem(', 'getAmmoItem(',
                          'spendTimeUnits(', 'popState(', 'pushState(', 'SDL_'):
            self.assertNotIn(forbidden, code)

    def test_ordinary_state_callbacks_and_buttons_remain_baseline_identical(self):
        self.require_baseline()
        raw = (BASELINE / 'src/Battlescape/InventoryState.cpp').read_bytes()
        self.assertEqual(hashlib.sha256(raw).hexdigest(), '1f0c9411d69da3216ca32021793606a5db4fb73d98694584a98cb576fff338b5')
        baseline = raw.decode()
        for name in ('invClick', 'invMouseOver', 'btnUnloadClick', 'btnGroundClick', 'btnOkClick'):
            with self.subTest(method=name):
                self.assertEqual(tokens(body(baseline, 'InventoryState::' + name)),
                                 tokens(body(self.state, 'InventoryState::' + name)))

    def test_battlescape_open_inventory_reuses_visible_normal_button(self):
        code = read('Battlescape/BattlescapeState.cpp')
        ui = body(code, 'BattlescapeState::agentUiState')
        self.assertIn('agentWidgetUncovered(_btnInventory, _surfaces)', ui)
        self.assertIn('AgentUiAction("open_inventory", "Inventory", "visible_control_semantics")', ui)
        self.assertIn('agentIncludeWidget(out, _btnInventory)', ui)
        dispatch = body(code, 'BattlescapeState::agentDispatch', 'const std::string &action')
        self.assertRegex(dispatch, r'action == "open_inventory"\)\s*btnInventoryClick\((?:0|nullptr)\)')


if __name__ == '__main__':
    unittest.main(verbosity=2)
