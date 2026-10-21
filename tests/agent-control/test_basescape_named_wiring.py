"""Source wiring regression; not pixel, controller, or live acceptance.

Pairs the actual constructor opt-in list with every projected named button.
The complete game must still compile and live admission remains fail-closed.
"""
from pathlib import Path
import re
import unittest

SOURCE = Path(__file__).resolve().parents[2] / 'src/Basescape/BasescapeState.cpp'


def missing_opt_ins(source):
    constructor = source.split('BasescapeState::~BasescapeState', 1)[0]
    loops = re.findall(r'for\s*\(TextButton\s*\*\s*button\s*:\s*\{([^}]+)\}\)\s*button->agentEnableNamedControl\(\)', constructor)
    enabled = set(re.findall(r'_btn\w+', ','.join(loops)))
    projected = set(re.findall(r'button\((_btn\w+),"[^"]+","[^"]+"\)', source))
    if not projected or not enabled:
        raise ValueError('constructor/provider syntax changed; update explicit wiring audit')
    return projected - enabled


class NamedWiring(unittest.TestCase):
    def test_every_current_projected_button_enabled_in_actual_constructor(self):
        self.assertEqual(missing_opt_ins(SOURCE.read_text()), set())

    def test_original_transfer_omission_is_detected(self):
        source = SOURCE.read_text()
        source = source.replace('_btnPurchase,_btnTransfer,_btnGeoscape}', '_btnPurchase,_btnGeoscape}')
        self.assertEqual(missing_opt_ins(source), {'_btnTransfer'})

    def test_return_omission_is_detected(self):
        source = SOURCE.read_text().replace(',_btnGeoscape})', '})')
        self.assertEqual(missing_opt_ins(source), {'_btnGeoscape'})


if __name__ == '__main__':
    unittest.main()
