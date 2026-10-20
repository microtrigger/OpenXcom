import unittest
import inventory_ui as inventory


class InventoryParserTests(unittest.TestCase):
    def bindings(self, source):
        return list(inventory.CALL.finditer(inventory.masked(source, mask_strings=True)))

    def test_plain_receiver(self):
        found = self.bindings('_button->onMouseClick((ActionHandler)&Screen::ok);')
        self.assertEqual([(m['receiver'], m['event']) for m in found], [('_button', 'onMouseClick')])

    def test_array_receiver(self):
        self.assertEqual(self.bindings('_buttons[i]->onChange(handler);')[0]['receiver'], '_buttons[i]')

    def test_vector_receiver(self):
        self.assertEqual(self.bindings('_regions.at(offset)->onMousePress(handler);')[0]['receiver'], '_regions.at(offset)')

    def test_iterator_receiver(self):
        self.assertEqual(self.bindings('(*i)->onMouseClick(handler, 0);')[0]['receiver'], '(*i)')

    def test_comments_and_literals_are_not_bindings(self):
        source = '// a->onMouseClick(fake);\n/* b->onChange(fake); */\nconst char *x="c->onChange(fake)";'
        self.assertEqual(self.bindings(source), [])

    def test_positions_and_lines_preserved(self):
        source = '/* c\n c */\n_button->onChange((ActionHandler)&Screen::ok);'
        self.assertEqual(len(source), len(inventory.masked(source, mask_strings=True)))
        self.assertEqual(source.count('\n', 0, self.bindings(source)[0].start()) + 1, 3)

    def test_balanced_arguments(self):
        source = '_button->onMouseClick((ActionHandler)&Screen::ok, Options::keyOk);'
        match = self.bindings(source)[0]
        self.assertEqual(inventory.balanced_call(source, match.end()), '(ActionHandler)&Screen::ok, Options::keyOk')

    def test_balanced_quoted_parentheses(self):
        source = '_button->onChange(fn("(x)", nested(1)), 0);'
        match = self.bindings(source)[0]
        self.assertEqual(inventory.balanced_call(source, match.end()), 'fn("(x)", nested(1)), 0')

    def test_unclosed_arguments_fail(self):
        with self.assertRaises(ValueError):
            inventory.balanced_call('nested(', 0)

    def test_direct_input_implementations(self):
        source = 'void Map::mouseClick(Action *a) {} void TextEdit::keyboardPress(Action *a) {}'
        self.assertEqual([(m[1], m[2]) for m in inventory.DIRECT.finditer(source)], [('Map', 'mouseClick'), ('TextEdit', 'keyboardPress')])


if __name__ == '__main__':
    unittest.main()
