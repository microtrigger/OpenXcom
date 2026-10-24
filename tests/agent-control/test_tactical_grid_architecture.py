#!/usr/bin/env python3
"""Source contracts for bounded draw snapshots; no GUI or game-state execution."""
import unittest
from test_menu_architecture import body, read


class TacticalGridArchitectureTest(unittest.TestCase):
    def test_ordinary_input_keeps_private_clamp_after_shared_math(self):
        ordinary = body('Battlescape/Camera.cpp', 'Camera::convertScreenToMap')
        self.assertLess(ordinary.index('convertScreenToMapUnclamped('), ordinary.index('Clamp('))
        self.assertIn('Clamp(*mapX, -1, _mapsize_x)', ordinary)
        self.assertIn('Clamp(*mapY, -1, _mapsize_y)', ordinary)
        unclamped = body('Battlescape/Camera.cpp', 'Camera::convertScreenToMapUnclamped')
        self.assertIn('CameraProjectionMath(', unclamped)
        self.assertIn('.screenToMapUnclamped(', unclamped)
        self.assertNotIn('_mapsize', unclamped)
        self.assertNotIn('Clamp(', unclamped)
        self.assertIn('.mapToScreen(', body('Battlescape/Camera.cpp', 'Camera::convertMapToScreen'))

    def test_viewport_grid_has_no_map_model_or_tile_enumeration(self):
        code = body('Battlescape/CameraProjection.h', 'cameraViewportGrid')
        for forbidden in ('_save', '_mapsize', 'getMapSize', 'getTile', 'getUnit', 'isDiscovered',
                          'getVisible', 'getPosition', 'Clamp(', 'mapToScreen(', 'mapX', 'mapY'):
            self.assertNotIn(forbidden, code)
        self.assertIn('width > 640', code)
        self.assertIn('height > 400', code)
        self.assertIn('nearest.size() < 128', code)
        self.assertIn('"g" + std::to_string(i)', code)
        self.assertLess(code.index('for (int y'), code.index('for (int x'))
        self.assertIn('y + quarter < height', code)
        self.assertIn('x + half < width', code)

    def test_hidden_gate_is_captured_from_exact_ordinary_draw_branch(self):
        code = body('Battlescape/Map.cpp', 'Map::draw')
        self.assertIn('bool hiddenMovement = false;', code)
        self.assertIn('if ((_save->getSelectedUnit() && _save->getSelectedUnit()->getVisible()) || '
                      '_unitDying || _save->getSelectedUnit() == 0 || _save->getDebugMode() || '
                      '_projectileInFOV || _explosionInFOV)', code)
        branch = code[code.index('drawTerrain(this);'):code.index('_agentDrawSnapshot =')]
        self.assertLess(branch.index('else'), branch.index('_message->blit(this)'))
        self.assertLess(branch.index('_message->blit(this)'), branch.index('hiddenMovement = true'))
        self.assertEqual(code.count('hiddenMovement = true'), 1)
        capture = body('Battlescape/CameraProjection.h', 'cameraMapSnapshot')
        self.assertIn('if (hiddenMovement) snapshot.hiddenMovementLabel = paintedMessage;', capture)
        self.assertIn('else snapshot.grid = cameraViewportGrid(', capture)

    def test_snapshot_revalidates_public_context_and_fails_closed_when_stale(self):
        code = body('Battlescape/Map.cpp', 'Map::agentMapSnapshot')
        # Engine-backed context now revalidates only admitted public semantic facts.
        for forbidden in ('getSide', 'getViewLevel', 'getArmor', 'getAI'):
            self.assertNotIn(forbidden, code)
        self.assertIn('out = AgentMapSnapshot()', code)
        self.assertIn('!agentContextVisible()', code)
        self.assertIn('!Observer::agentPaintHealthy()', code)
        self.assertIn('_agentDrawViewRevision != _camera->agentViewRevision()', code)
        self.assertIn('_agentDrawSurfaceHeight != getHeight()', code)
        draw = body('Battlescape/Map.cpp', 'Map::draw')
        for axis in ('x', 'y'):
            self.assertIn('_agentDrawSnapshot.' + axis + ' >= 0', draw)
            self.assertIn('_agentDrawSnapshot.' + axis + ' <= 4095', draw)

    def test_hidden_label_uses_only_complete_rendered_text_and_ink(self):
        code = body('Battlescape/BattlescapeMessage.cpp', 'BattlescapeMessage::agentPaintedLabel')
        for required in ('agentContextVisible()', 'agentTextComplete()', 'agentPaintedBounds(ink)',
                         'getFont()', 'getTextWidth()', 'getTextHeight()', 'return _text->getText()'):
            self.assertIn(required, code)
        for forbidden in ('getString(', 'tr(', 'STR_HIDDEN_MOVEMENT'):
            self.assertNotIn(forbidden, code)
        self.assertIn('bounds = AgentCameraRect()', code)
        self.assertIn('bounds.width = ink.w', code)
        self.assertIn('int(ink.x) + ink.w > viewportWidth', code)
        self.assertIn('int(ink.y) + ink.h > viewportHeight', code)
        draw = body('Battlescape/Map.cpp', 'Map::draw')
        self.assertLess(draw.index('_message->blit(this)'), draw.index('agentPaintedLabel('))
        self.assertIn('_agentDrawSnapshot.messageRegion = messageRegion', draw)

    def test_camera_mutations_advance_only_private_generation_before_nested_draw(self):
        for method in ('scrollXY', 'jumpXY', 'up', 'down', 'setViewLevel', 'centerOnPosition',
                       'setMapOffset', 'toggleShowAllLayers', 'resize'):
            code = body('Battlescape/Camera.cpp', 'Camera::' + method)
            self.assertIn('++_agentViewRevision', code)
            if '_map->draw()' in code:
                self.assertLess(code.index('++_agentViewRevision'), code.index('_map->draw()'))
        for method in ('convertScreenToMap', 'convertMapToScreen', 'agentViewportSnapshot'):
            self.assertNotIn('++_agentViewRevision', body('Battlescape/Camera.cpp', 'Camera::' + method))
        self.assertNotIn('_mapsize', body('Battlescape/Camera.cpp', 'Camera::agentViewportSnapshot'))

    def test_snapshot_data_contains_no_hidden_identity_or_actions(self):
        code = read('Battlescape/CameraProjection.h')
        snapshot = code.split('struct AgentMapSnapshot', 1)[1].split('};', 1)[0]
        for forbidden in ('mapX', 'mapY', 'mapZ', 'viewLevel', 'actor', 'revision', 'valid', 'passable'):
            self.assertNotIn(forbidden, snapshot)
        for forbidden in ('SDL_', 'Action', 'SavedBattleGame', 'BattleUnit', 'Tile *'):
            self.assertNotIn(forbidden, code)


if __name__ == '__main__':
    unittest.main(verbosity=2)
