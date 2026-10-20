#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# New contribution maintained in this OpenXcom fork, 2026-10-10.
# Supervised stance integration added 2026-10-11, GPL-3.0-or-later.
# This file is free software: you may redistribute and/or modify it under
# GNU GPL version 3 or, at your option, any later version.
# Provided without warranty; see the repository LICENSE.txt and LICENSE-NOTICES.md.
"""Local MCP: historical reads, opt-in presentation/audio and supervised controls."""
import json
import sys

PROTOCOL = '2025-11-25'
MAX_MESSAGE = 16384
MAX_RESPONSE = 131072
UNAVAILABLE = {'content': [{'type': 'text', 'text':
    '{"available":false,"freshness":"unverified","reason":"unsupported_freshness"}'}],
    'isError': True}
NAMES = ('read_last_hud', 'read_last_cursor')


def silence_broken_stdout():
    # A buffered stream otherwise retries its failed flush during interpreter
    # shutdown and emits an uncontrolled diagnostic after the handled failure.
    import os
    try:
        with open(os.devnull, 'wb', buffering=0) as discard:
            os.dup2(discard.fileno(), sys.stdout.fileno())
    except (OSError, ValueError):
        pass


def strict_json(raw):
    def pairs(items):
        result = {}
        for k, v in items:
            if k in result:
                raise ValueError('duplicate key')
            result[k] = v
        return result
    def reject(_):
        raise ValueError('nonfinite number')
    return json.loads(raw, object_pairs_hook=pairs, parse_constant=reject)


def error(ident, code, message):
    return {'jsonrpc': '2.0', 'id': ident, 'error': {'code': code, 'message': message}}


def definitions(presentation=False, presented_hud=False, mixed_audio=False, supervised_stance=False,
                supervised_selection=False, supervised_turn=False, supervised_map_left_click=False,
                supervised_inventory=False, supervised_inventory_close=False, supervised_inventory_ground=False,
                supervised_inventory_click=False, supervised_inventory_right_click=False, supervised_inventory_unload=False,
                agent_control=False):
    result = [{'name': name,
             'description': 'Read-only historical public observation, never current-state authority. HUD and cursor sessions are independent. require_current always fails closed.',
             'inputSchema': {'type': 'object', 'properties': {'require_current': {'type': 'boolean', 'default': False}}, 'additionalProperties': False},
             'annotations': {'readOnlyHint': True, 'destructiveHint': False,
                             'idempotentHint': True, 'openWorldHint': False}}
            for name in NAMES]
    if presentation:
        result.append({'name': 'read_presented_cursor',
                       'description': 'Request one normal displayed cursor crop presented after this request; never instantaneous current world state. Unsupported presentation or timeout is unavailable. ' + ('This tool returns pixels only.' if presented_hud else 'HUD freshness is unsupported.'),
                       'inputSchema': {'type': 'object', 'properties': {}, 'additionalProperties': False},
                       'annotations': {'readOnlyHint': True, 'destructiveHint': False,
                                       'idempotentHint': False, 'openWorldHint': False}})
        if presented_hud:
            result.append({'name': 'read_presented_hud',
                           'description': 'Request four displayed HUD integers from one normal presentation after this request, or null when no eligible HUD was displayed. Never instantaneous current world state. Requires explicitly selected presentation version 2.',
                           'inputSchema': {'type': 'object', 'properties': {}, 'additionalProperties': False},
                           'annotations': {'readOnlyHint': True, 'destructiveHint': False,
                                           'idempotentHint': False, 'openWorldHint': False}})
    if mixed_audio:
        result.append({'name': 'read_mixed_audio',
                       'description': 'Read one lossless 500ms application postmix WAV window after audio callback admission. Fixed 22050Hz signed 16-bit little-endian stereo. Not physical audibility, instantaneous current state or synchronized video. Between-request sounds can be missed. Wait at least one second after each result; failure requires explicit relaunch.',
                       'inputSchema': {'type': 'object', 'properties': {}, 'additionalProperties': False},
                       'annotations': {'readOnlyHint': True, 'destructiveHint': False,
                                       'idempotentHint': False, 'openWorldHint': False}})
    if supervised_stance:
        opaque = {'type': 'string', 'pattern': '^[0-9a-f]{32}$'}
        stance = {'type': 'string', 'enum': ['kneel', 'stand']}
        result.extend([
            {'name': 'read_stance_context',
             'description': 'Read the actual displayed bottom-panel pixels after this request and receive a one-use stance capability. Interpret the visible stance yourself. A new read invalidates the previous capability. No unit IDs, hidden stats or semantic HUD extraction. Blocked while local confirmation is pending. After any failed observation or transport uncertainty, relaunch explicitly; never automatically retry.',
             'inputSchema': {'type': 'object', 'properties': {}, 'additionalProperties': False},
             'annotations': {'readOnlyHint': True, 'destructiveHint': False,
                             'idempotentHint': False, 'openWorldHint': False}},
            {'name': 'request_stance',
             'description': 'Experimental supervised pilot: ask the local game UI to confirm an explicit kneel/stand action based on the supplied visual interpretation and fresh context capability. Only a human using ordinary local game controls can approve. A model boolean cannot approve. The action may spend time units or trigger reaction fire. pending_confirmation is not dispatch; dispatched is never success. Poll read_stance_status, then obtain a fresh context image for visible verification. Never resend after uncertainty. An exact duplicate only queries status; conflicting command-ID reuse fails.',
             'inputSchema': {'type': 'object', 'properties': {
                 'capability': dict(opaque), 'control_epoch': dict(opaque),
                 'command_id': dict(opaque), 'observed': dict(stance), 'desired': dict(stance)},
                 'required': ['capability', 'control_epoch', 'command_id', 'observed', 'desired'],
                 'additionalProperties': False},
             'annotations': {'readOnlyHint': False, 'destructiveHint': True,
                             'idempotentHint': True, 'openWorldHint': False}},
            {'name': 'read_stance_status',
             'description': 'Read one known command receipt: pending_confirmation, dispatched or unavailable. This never retries or resends an action. dispatched is not success; read a fresh post-action context image for visible verification. Poll explicitly while pending. Transport failure closes the stance channel; do not retry automatically.',
             'inputSchema': {'type': 'object', 'properties': {'command_id': dict(opaque)},
                             'required': ['command_id'], 'additionalProperties': False},
             'annotations': {'readOnlyHint': True, 'destructiveHint': False,
                             'idempotentHint': True, 'openWorldHint': False}}])
        if supervised_selection:
            result[-3]['description'] += ' This capability is shared with request_selection; either action consumes it.'
            result[-1]['description'] += ' This also reads request_selection command receipts from the shared ledger.'
            result.append({
                'name': 'request_selection',
                'description': 'Experimental supervised pilot: ask the local game UI to confirm selecting the next or previous eligible player unit using the normal selection handler and a fresh read_stance_context capability. Only a human using ordinary local game controls can approve. No target IDs, coordinates or model approval. Stance and selection share one-use context and command IDs. pending_confirmation is not dispatch; dispatched is never success. Poll read_stance_status, then read a fresh context image to verify visible selection. Never resend after uncertainty. An exact duplicate only queries status; conflicting command-ID reuse, including across action types, fails.',
                'inputSchema': {'type': 'object', 'properties': {
                    'capability': dict(opaque), 'control_epoch': dict(opaque),
                    'command_id': dict(opaque), 'direction': {'type': 'string', 'enum': ['next', 'previous']}},
                    'required': ['capability', 'control_epoch', 'command_id', 'direction'],
                    'additionalProperties': False},
                'annotations': {'readOnlyHint': False, 'destructiveHint': True,
                                'idempotentHint': True, 'openWorldHint': False}})
        if supervised_turn:
            for tool in result:
                if tool['name'] == 'read_stance_context':
                    tool['description'] += ' This panel capability cannot authorize request_turn. Reading either context retires the previous grant. After a dispatched turn, read_tactical_context is required first.'
                elif tool['name'] == 'read_stance_status':
                    tool['description'] += ' This also reads request_turn receipts from the shared ledger. After dispatched, obtain a fresh tactical image; a failed observation stops the channel without retry.'
            result.extend([
                {'name': 'read_tactical_context',
                 'description': 'Read one actual 320x200 tactical presentation after this request as a lossless PNG and receive a one-use request_turn capability. Coordinates are logical image pixels. Reading either tactical or stance context retires the previous grant; this capability cannot authorize stance or selection. No world coordinates, target IDs, hidden stats or semantic extraction. Blocked while local confirmation is pending. A fresh tactical image is required after a dispatched turn. On failed observation or uncertainty, stop and relaunch explicitly; never automatically retry.',
                 'inputSchema': {'type': 'object', 'properties': {}, 'additionalProperties': False},
                 'annotations': {'readOnlyHint': True, 'destructiveHint': False,
                                 'idempotentHint': False, 'openWorldHint': False}},
                {'name': 'request_turn',
                 'description': 'Experimental supervised pilot: ask the local game UI to confirm one ordinary right-click at a pixel in a fresh read_tactical_context image. Ordinary right-click can turn toward that pixel or attempt to open a door, under normal game rules, time-unit costs and reservations. Only a human using ordinary local game controls can approve. No world coordinates, desired direction, target IDs, cost predictions or model approval. Coordinates are exact logical image integers: x 16..303, y 40..127. All control actions share command IDs. pending_confirmation is not dispatch; dispatched is never success. Poll read_stance_status, then obtain a fresh tactical image before any further context/action to verify the visible outcome. Never resend after uncertainty. An exact duplicate only queries status; conflicting command-ID reuse, including across action types, fails.',
                 'inputSchema': {'type': 'object', 'properties': {
                     'capability': dict(opaque), 'control_epoch': dict(opaque),
                     'command_id': dict(opaque), 'x': {'type': 'integer', 'minimum': 16, 'maximum': 303},
                     'y': {'type': 'integer', 'minimum': 40, 'maximum': 127}},
                     'required': ['capability', 'control_epoch', 'command_id', 'x', 'y'],
                     'additionalProperties': False},
                 'annotations': {'readOnlyHint': False, 'destructiveHint': True,
                                 'idempotentHint': True, 'openWorldHint': False}}])
            if supervised_map_left_click:
                for tool in result:
                    if tool['name'] == 'read_stance_context':
                        tool['description'] += ' This panel capability also cannot authorize request_map_left_click. After a dispatched map left-click, read_tactical_context is required first.'
                    elif tool['name'] == 'read_stance_status':
                        tool['description'] += ' This also reads request_map_left_click receipts from the same ledger.'
                    elif tool['name'] == 'read_tactical_context':
                        tool['description'] += ' This same one-use tactical capability may instead authorize request_map_left_click. After a dispatched map left-click, the mandatory fresh tactical read allows up to ten seconds for the native bounded ordinary movement wait; all other client waits are unchanged. No busy reason or predicted outcome is exposed.'
                result.append({
                    'name': 'request_map_left_click',
                    'description': 'Experimental supervised pilot: ask the local game UI to confirm one ordinary left-click at a pixel in a fresh read_tactical_context image. Ordinary left-click may move, select a visible player unit, or do nothing, under normal game rules, time-unit costs, reservations, contacts and reaction fire. Only the default disabled path-preview option is supported; settings are never rewritten. Only a human using ordinary local game controls can approve. No target lists, world coordinates, path or cost predictions, preflight queries or model approval. Coordinates are exact logical image integers: x 16..303, y 40..127. The tactical capability authorizes either right-click or left-click once. All control actions share command IDs. pending_confirmation is not dispatch; dispatched is never success. Poll read_stance_status, then obtain a fresh tactical image before another action to verify the visible outcome. A failed observation or uncertainty stops control; never resend. An exact duplicate only queries status; changed coordinates or cross-action command-ID reuse fails.',
                    'inputSchema': {'type': 'object', 'properties': {
                        'capability': dict(opaque), 'control_epoch': dict(opaque),
                        'command_id': dict(opaque), 'x': {'type': 'integer', 'minimum': 16, 'maximum': 303},
                        'y': {'type': 'integer', 'minimum': 40, 'maximum': 127}},
                        'required': ['capability', 'control_epoch', 'command_id', 'x', 'y'],
                        'additionalProperties': False},
                    'annotations': {'readOnlyHint': False, 'destructiveHint': True,
                                    'idempotentHint': True, 'openWorldHint': False}})
            if supervised_inventory:
                for tool in result:
                    if tool['name'] == 'read_stance_context':
                        tool['description'] += ' This panel capability cannot authorize request_open_inventory. After dispatched inventory open, read_inventory_view is required first.'
                    elif tool['name'] == 'read_stance_status':
                        tool['description'] += ' This also reads request_open_inventory receipts from the same ledger. After dispatched inventory open, read_inventory_view is required first; status queries never replace that read.'
                    elif tool['name'] == 'read_tactical_context':
                        tool['description'] += ' This same one-use tactical capability may instead authorize request_open_inventory. After dispatched inventory open, read_inventory_view is required first.'
                result.extend([
                    {'name': 'request_open_inventory',
                     'description': 'Experimental supervised pilot: ask the local game UI to confirm opening the selected player unit inventory using the ordinary inventory handler and a fresh read_tactical_context capability. Only a human using ordinary local game controls can approve. Exactly three opaque tokens; no model approval, actor, item, action or coordinate arguments. All controls share command IDs. pending_confirmation is not dispatch; dispatched is never success. Poll read_stance_status, then read_inventory_view before any other shared context or action. An exact duplicate only queries status; conflicting command-ID reuse fails. Never resend after uncertainty. Inventory stays open; the user closes it manually. No close or item-manipulation tool is provided.',
                     'inputSchema': {'type': 'object', 'properties': {
                         'capability': dict(opaque), 'control_epoch': dict(opaque), 'command_id': dict(opaque)},
                         'required': ['capability', 'control_epoch', 'command_id'], 'additionalProperties': False},
                     'annotations': {'readOnlyHint': False, 'destructiveHint': True,
                                     'idempotentHint': True, 'openWorldHint': False}},
                    {'name': 'read_inventory_view',
                     'description': 'Read one actual 320x200 supported inventory presentation after this request as a lossless PNG. May also read a manually opened inventory. Pixels only: no capability, epoch, semantic item list, names, IDs or hidden metadata. Retires any prior control grant and authorizes no actions. Inventory remains open; the user closes it manually before a fresh stance or tactical context can authorize an action. Blocked while local confirmation or required tactical verification is pending. Failed observation or uncertain transport closes control without retry or terminating the game.',
                     'inputSchema': {'type': 'object', 'properties': {}, 'additionalProperties': False},
                     'annotations': {'readOnlyHint': True, 'destructiveHint': False,
                                     'idempotentHint': False, 'openWorldHint': False}}])
                if supervised_inventory_close:
                    for tool in result:
                        if tool['name'] == 'request_open_inventory':
                            tool['description'] = tool['description'].replace(
                                'Inventory stays open; the user closes it manually. No close or item-manipulation tool is provided.',
                                'Inventory stays open. After the mandatory capless read_inventory_view, a separate fresh read_inventory_close_context can authorize supervised OK/close. No item-manipulation tool is provided.')
                        elif tool['name'] == 'read_inventory_view':
                            tool['description'] = tool['description'].replace(
                                'Inventory remains open; the user closes it manually before a fresh stance or tactical context can authorize an action.',
                                'Inventory remains open; this capless image cannot authorize close. A separate fresh read_inventory_close_context is required for supervised OK/close.')
                        elif tool['name'] == 'read_stance_status':
                            tool['description'] += ' This also reads request_close_inventory receipts. After dispatched close, read_tactical_context is required first; duplicate receipts never replace or rearm that read.'
                        elif tool['name'] in ('read_stance_context', 'read_tactical_context'):
                            tool['description'] += ' This capability cannot authorize request_close_inventory. After dispatched close, read_tactical_context is required before another shared observation or action.'
                    result.extend([
                        {'name': 'read_inventory_close_context',
                         'description': 'Read one actual 320x200 supported inventory presentation after this request as a lossless PNG and receive a one-use capability for ordinary inventory OK/close only. This separate context retires the previous grant. No item IDs, item collections, slots, actor IDs, hidden metadata or eligibility predictions. Existing read_inventory_view remains capless. Blocked while local confirmation or mandatory post-action verification is pending. Failed observation or uncertain transport closes control without retry or terminating the game.',
                         'inputSchema': {'type': 'object', 'properties': {}, 'additionalProperties': False},
                         'annotations': {'readOnlyHint': True, 'destructiveHint': False,
                                         'idempotentHint': False, 'openWorldHint': False}},
                        {'name': 'request_close_inventory',
                         'description': 'Experimental supervised pilot: ask the local in-place inventory UI to confirm ordinary OK/close using a fresh read_inventory_close_context capability. Only a human using ordinary local game controls can approve. Exactly three opaque tokens; no model approval, action selector, coordinates, actor or item arguments. All controls share command IDs. pending_confirmation is not dispatch; dispatched means ordinary handler dispatch, never successful closing or completed cleanup. Poll read_stance_status, then obtain a fresh read_tactical_context before another shared observation or action. Native cleanup and display restoration proceed normally. If the tactical view is unavailable, stop for manual reconciliation; never replay close. An exact duplicate only queries status; conflicting command-ID reuse fails. No item manipulation is provided.',
                         'inputSchema': {'type': 'object', 'properties': {
                             'capability': dict(opaque), 'control_epoch': dict(opaque), 'command_id': dict(opaque)},
                             'required': ['capability', 'control_epoch', 'command_id'], 'additionalProperties': False},
                         'annotations': {'readOnlyHint': False, 'destructiveHint': True,
                                         'idempotentHint': True, 'openWorldHint': False}}])
                    if supervised_inventory_ground is True:
                        for tool in result:
                            if tool['name'] in ('request_open_inventory', 'request_close_inventory'):
                                tool['description'] = tool['description'].replace(' No item-manipulation tool is provided.', '').replace(' No item manipulation is provided.', '')
                                tool['description'] += ' A separate fresh read_inventory_ground_context can authorize ordinary Ground, which rearranges ground slots/stacks and cycles the page.'
                            elif tool['name'] == 'read_inventory_view':
                                tool['description'] += ' This capless view also cannot authorize Ground. After dispatched Ground, this read is mandatory before another shared observation or action.'
                            elif tool['name'] == 'read_stance_status':
                                tool['description'] += ' This also reads request_inventory_ground receipts. After dispatched Ground, read_inventory_view is required first; duplicate receipts never replace or rearm that read.'
                            elif tool['name'] in ('read_stance_context', 'read_tactical_context', 'read_inventory_close_context'):
                                tool['description'] += ' This capability cannot authorize request_inventory_ground. A separate fresh read_inventory_ground_context is required; after dispatched Ground, read_inventory_view is required first.'
                        result.extend([
                            {'name': 'read_inventory_ground_context',
                             'description': 'Read one actual 320x200 supported inventory presentation after this request as a lossless PNG and receive a one-use capability for ordinary inventory Ground only. This separate context retires the previous grant and cannot authorize OK/close or any other control. No item IDs, item collections, slots, actor IDs, page enumeration, hidden metadata or eligibility predictions. Existing read_inventory_view remains capless. Blocked while local confirmation or mandatory post-action verification is pending. Failed observation or uncertain transport closes control without retry or terminating the game.',
                             'inputSchema': {'type': 'object', 'properties': {}, 'additionalProperties': False},
                             'annotations': {'readOnlyHint': True, 'destructiveHint': False,
                                             'idempotentHint': False, 'openWorldHint': False}},
                            {'name': 'request_inventory_ground',
                             'description': 'Experimental supervised pilot: ask the local in-place inventory UI to confirm ordinary Ground using a fresh read_inventory_ground_context capability. Ground rearranges ground slots/stacks and cycles the page through the ordinary game handler. Only a human using ordinary local game controls can approve. Exactly three opaque tokens; no model approval, action selector, coordinates, actor, item, page or preflight arguments. All controls share command IDs. pending_confirmation is not dispatch; dispatched means ordinary handler dispatch, never a successful rearrangement or a particular page. Poll read_stance_status, then obtain a fresh capless read_inventory_view before another shared observation or action. If that view is unavailable, stop for manual reconciliation; never replay Ground. An exact duplicate only queries status; conflicting command-ID reuse fails.',
                             'inputSchema': {'type': 'object', 'properties': {
                                 'capability': dict(opaque), 'control_epoch': dict(opaque), 'command_id': dict(opaque)},
                                 'required': ['capability', 'control_epoch', 'command_id'], 'additionalProperties': False},
                             'annotations': {'readOnlyHint': False, 'destructiveHint': True,
                                             'idempotentHint': True, 'openWorldHint': False}}])
                    if supervised_inventory_click is True:
                        for tool in result:
                            if tool['name'] in ('request_open_inventory', 'request_close_inventory'):
                                tool['description'] = tool['description'].replace(' No item-manipulation tool is provided.', '').replace(' No item manipulation is provided.', '')
                            if tool['name'] == 'read_stance_status':
                                tool['description'] += ' This also reads request_inventory_click receipts. After dispatched inventory LEFT click, read_inventory_view is required first; duplicate receipts never replace or rearm that read.'
                            elif tool['name'] == 'read_inventory_view':
                                tool['description'] += ' After dispatched inventory LEFT click, this capless read is mandatory before another shared observation or action.'
                            elif tool['name'] in ('read_stance_context', 'read_tactical_context', 'read_inventory_close_context', 'read_inventory_ground_context'):
                                tool['description'] += ' This capability cannot authorize request_inventory_click. A separate fresh read_inventory_click_context is required.'
                        result.extend([
                            {'name': 'read_inventory_click_context',
                             'description': 'Read one actual 320x200 supported inventory presentation after this request as a lossless PNG and receive a one-use capability for ordinary inventory LEFT click only. This separate context retires the previous grant and cannot authorize any other control. Pixels only: no held-item metadata, item IDs or collections, slots, actor IDs, page enumeration, eligibility or cost predictions. Existing read_inventory_view remains capless. Blocked while local confirmation or mandatory post-action verification is pending. Failed observation or uncertain transport closes control without retry or terminating the game.',
                             'inputSchema': {'type': 'object', 'properties': {}, 'additionalProperties': False},
                             'annotations': {'readOnlyHint': True, 'destructiveHint': False,
                                             'idempotentHint': False, 'openWorldHint': False}},
                            {'name': 'request_inventory_click',
                             'description': 'Experimental supervised pilot: ask the local in-place inventory UI to confirm one ordinary unmodified LEFT click at a pointer coordinate from a fresh read_inventory_click_context image. A click may pick up, place, stack, load ammo, warn or do nothing under normal game rules and time-unit costs; no successful transfer is promised. Coordinates are exact logical image integers: right hand x 0..31, y 63..110; left hand x 128..159, y 63..110; displayed ground x 0..319, y 152..199. These are pointer coordinates, not a desired slot or item top-left. No button selector, modifiers, item queries, preflight or model approval. Each click requires fresh human confirmation using ordinary local game controls. Picking up and then placing an item requires two separately confirmed clicks with a mandatory capless read_inventory_view between them; no automatic second click or rollback. All controls share command IDs. pending_confirmation is not dispatch; dispatched means ordinary handler dispatch, never success. Poll read_stance_status, then obtain a fresh capless read_inventory_view before another shared observation or action. If unavailable, stop for manual reconciliation; never replay. An exact duplicate only queries status; changed coordinates or cross-action command-ID reuse fails.',
                             'inputSchema': {'type': 'object', 'properties': {
                                 'capability': dict(opaque), 'control_epoch': dict(opaque), 'command_id': dict(opaque),
                                 'x': {'type': 'integer', 'minimum': 0, 'maximum': 319},
                                 'y': {'type': 'integer', 'minimum': 63, 'maximum': 199}},
                                 'required': ['capability', 'control_epoch', 'command_id', 'x', 'y'], 'additionalProperties': False,
                                 'anyOf': [
                                     {'properties': {'x': {'minimum': 0, 'maximum': 31}, 'y': {'minimum': 63, 'maximum': 110}}},
                                     {'properties': {'x': {'minimum': 128, 'maximum': 159}, 'y': {'minimum': 63, 'maximum': 110}}},
                                     {'properties': {'x': {'minimum': 0, 'maximum': 319}, 'y': {'minimum': 152, 'maximum': 199}}}]},
                             'annotations': {'readOnlyHint': False, 'destructiveHint': True,
                                             'idempotentHint': True, 'openWorldHint': False}}])
                    if supervised_inventory_right_click is True:
                        for tool in result:
                            if tool['name'] in ('request_open_inventory', 'request_close_inventory'):
                                tool['description'] = tool['description'].replace(' No item-manipulation tool is provided.', '').replace(' No item manipulation is provided.', '')
                            if tool['name'] == 'read_stance_status':
                                tool['description'] += ' This also reads request_inventory_right_click receipts. After dispatched inventory RIGHT click, a mandatory read_inventory_view or read_tactical_context remains unresolved until the appropriate image succeeds. The receipt never selects that branch; duplicate receipts never replace or rearm that read.'
                            elif tool['name'] == 'read_inventory_view':
                                tool['description'] += ' After dispatched inventory RIGHT click with prior visible held selection, this capless read is mandatory. Choose it from the visible situation; never probe or fall back to the tactical view if unavailable.'
                            elif tool['name'] in ('read_stance_context', 'read_tactical_context', 'read_inventory_close_context', 'read_inventory_ground_context', 'read_inventory_click_context'):
                                tool['description'] += ' This capability cannot authorize request_inventory_right_click. A separate fresh read_inventory_right_click_context is required.'
                        for tool in result:
                            if tool['name'] == 'read_tactical_context':
                                tool['description'] += ' After dispatched inventory RIGHT click with prior visible unheld inventory, this is the mandatory original-parent view. Choose it from the visible situation; never probe or fall back to inventory if unavailable.'
                        result.extend([
                            {'name': 'read_inventory_right_click_context',
                             'description': 'Read one actual 320x200 supported inventory presentation after this request as a lossless PNG and receive a one-use capability for ordinary inventory RIGHT click only. This separate context retires the previous grant and cannot authorize any other control. Pixels only: no held-item metadata, item IDs or collections, slots, actor IDs, page enumeration, eligibility or cost predictions. Existing read_inventory_view remains capless. Blocked while local confirmation or mandatory post-action verification is pending. Failed observation or uncertain transport closes control without retry or terminating the game.',
                             'inputSchema': {'type': 'object', 'properties': {}, 'additionalProperties': False},
                             'annotations': {'readOnlyHint': True, 'destructiveHint': False,
                                             'idempotentHint': False, 'openWorldHint': False}},
                            {'name': 'request_inventory_right_click',
                             'description': 'Experimental supervised pilot: ask the local in-place inventory UI to confirm one ordinary unmodified RIGHT click at a pointer coordinate from a fresh read_inventory_right_click_context image. With a held item, ordinary RIGHT cancels the held selection; without one, ordinary RIGHT closes the inventory through normal cleanup. Each click requires fresh human confirmation using ordinary local game controls. Coordinates are exact logical image integers: right hand x 0..31, y 63..110; left hand x 128..159, y 63..110; displayed ground x 0..319, y 152..199. These are pointer coordinates, not a desired slot or item top-left. No button selector, modifiers, held-state query, outcome hint, preflight or model approval. All controls share command IDs. pending_confirmation is not dispatch; dispatched means ordinary handler dispatch, never success, and its receipt does not disclose the branch. Poll read_stance_status, then choose one mandatory image from the prior visible situation: read_inventory_view after held-selection cancel, or read_tactical_context after unheld close. The native producer enforces the actual destination. Never probe one view and fall back to another; an unavailable or failed observation closes control and requires manual reconciliation. No other context or new action is permitted first. An exact duplicate only queries status; changed coordinates or cross-action command-ID reuse fails.',
                             'inputSchema': {'type': 'object', 'properties': {
                                 'capability': dict(opaque), 'control_epoch': dict(opaque), 'command_id': dict(opaque),
                                 'x': {'type': 'integer', 'minimum': 0, 'maximum': 319},
                                 'y': {'type': 'integer', 'minimum': 63, 'maximum': 199}},
                                 'required': ['capability', 'control_epoch', 'command_id', 'x', 'y'], 'additionalProperties': False,
                                 'anyOf': [
                                     {'properties': {'x': {'minimum': 0, 'maximum': 31}, 'y': {'minimum': 63, 'maximum': 110}}},
                                     {'properties': {'x': {'minimum': 128, 'maximum': 159}, 'y': {'minimum': 63, 'maximum': 110}}},
                                     {'properties': {'x': {'minimum': 0, 'maximum': 319}, 'y': {'minimum': 152, 'maximum': 199}}}]},
                             'annotations': {'readOnlyHint': False, 'destructiveHint': True,
                                             'idempotentHint': True, 'openWorldHint': False}}])
                    if supervised_inventory_unload is True:
                        for tool in result:
                            if tool['name'] in ('request_open_inventory', 'request_close_inventory'):
                                tool['description'] = tool['description'].replace(' No item-manipulation tool is provided.', '').replace(' No item manipulation is provided.', '')
                                tool['description'] += ' A separate fresh read_inventory_unload_context can authorize an ordinary Unload attempt.'
                            elif tool['name'] == 'read_inventory_view':
                                tool['description'] += ' This capless view cannot authorize Unload. After every dispatched Unload attempt, including a no-op or ordinary refusal, this exact read is mandatory before another shared observation or action.'
                            elif tool['name'] == 'read_stance_status':
                                tool['description'] += ' This also reads request_inventory_unload receipts. After every dispatched Unload attempt, read_inventory_view is required first; duplicate receipts never replace or rearm that read.'
                            elif tool['name'] in ('read_stance_context', 'read_tactical_context', 'read_inventory_close_context', 'read_inventory_ground_context', 'read_inventory_click_context', 'read_inventory_right_click_context'):
                                tool['description'] += ' This capability cannot authorize request_inventory_unload. A separate fresh read_inventory_unload_context is required; after dispatched Unload, read_inventory_view is required first.'
                        result.extend([
                            {'name': 'read_inventory_unload_context',
                             'description': 'Read one actual 320x200 supported inventory presentation after this request as a lossless PNG and receive a one-use capability for an ordinary inventory Unload attempt only. This separate context retires the previous grant and cannot authorize any other control. Pixels only: no item or held-state metadata, ammunition, actor IDs, eligibility, costs, preflight or outcome fields. Existing read_inventory_view remains capless. Blocked while local confirmation or mandatory post-action verification is pending. Failed observation or uncertain transport closes control without retry or terminating the game.',
                             'inputSchema': {'type': 'object', 'properties': {}, 'additionalProperties': False},
                             'annotations': {'readOnlyHint': True, 'destructiveHint': False,
                                             'idempotentHint': False, 'openWorldHint': False}},
                            {'name': 'request_inventory_unload',
                             'description': 'Experimental supervised pilot: ask the local in-place inventory UI to confirm one ordinary Unload button attempt using a fresh read_inventory_unload_context capability. Normal game rules may unload a held loaded weapon, do nothing or show a warning. In the stock supported tactical case, a successful unload spends 8 TU, places the weapon in the right hand and ammunition in the left, and clears held selection. Each attempt requires fresh human confirmation using ordinary local game controls. Exactly three opaque tokens; no coordinates, button, modifiers, items, held-state, eligibility, ammunition, preflight, outcome or model approval fields. Cancelling confirmation, timeout or disconnect does not cancel held selection. All controls share command IDs. pending_confirmation is not dispatch; dispatched means an ordinary button-gesture dispatch attempt, never successful unloading. Poll read_stance_status, then obtain a fresh capless read_inventory_view after every dispatched attempt, including a no-op or ordinary refusal, before another shared observation or new action. read_tactical_context cannot replace this read. If the required view is unavailable, stop for manual reconciliation; never probe another view or replay Unload. An exact duplicate only queries status and never rearms a fulfilled verification barrier; conflicting command-ID reuse fails.',
                             'inputSchema': {'type': 'object', 'properties': {
                                 'capability': dict(opaque), 'control_epoch': dict(opaque), 'command_id': dict(opaque)},
                                 'required': ['capability', 'control_epoch', 'command_id'], 'additionalProperties': False},
                             'annotations': {'readOnlyHint': False, 'destructiveHint': True,
                                             'idempotentHint': True, 'openWorldHint': False}}])
    if agent_control:
        from agent_control import definitions as agent_definitions
        result.extend(agent_definitions())
    return result


class Server:
    def __init__(self, hud_dir=None, cursor_dir=None, presentation=None, mixed_audio=None, supervised_stance=None, agent_control=None):
        if agent_control is not None and supervised_stance is not None:
            raise ValueError('Distinct control modes cannot be combined')
        self.agent_control = agent_control
        self.state = 'new'
        self.presentation = presentation
        self.mixed_audio = mixed_audio
        self.supervised_stance = supervised_stance
        self.supervised_selection_enabled = (supervised_stance is not None
                                             and getattr(supervised_stance, 'selection_enabled', False) is True)
        self.supervised_turn_enabled = (supervised_stance is not None
                                       and getattr(supervised_stance, 'turn_enabled', False) is True)
        self.supervised_map_left_click_enabled = (self.supervised_turn_enabled
            and getattr(supervised_stance, 'map_left_click_enabled', False) is True)
        self.supervised_inventory_enabled = (self.supervised_turn_enabled
            and getattr(supervised_stance, 'inventory_enabled', False) is True)
        self.supervised_inventory_close_enabled = (self.supervised_inventory_enabled
            and getattr(supervised_stance, 'inventory_close_enabled', False) is True)
        self.supervised_inventory_ground_enabled = (self.supervised_inventory_close_enabled
            and getattr(supervised_stance, 'inventory_ground_enabled', False) is True)
        self.supervised_inventory_click_enabled = (self.supervised_inventory_close_enabled
            and getattr(supervised_stance, 'inventory_click_enabled', False) is True)
        self.supervised_inventory_right_click_enabled = (self.supervised_inventory_close_enabled
            and getattr(supervised_stance, 'inventory_right_click_enabled', False) is True)
        self.supervised_inventory_unload_enabled = (self.supervised_inventory_close_enabled
            and getattr(supervised_stance, 'inventory_unload_enabled', False) is True)
        self.presented_hud_enabled = presentation is not None and getattr(presentation, 'version', 1) == 2
        self.paths = {'hud': hud_dir, 'cursor': cursor_dir}

    def handle(self, msg):
        if not isinstance(msg, dict):
            return error(None, -32600, 'Invalid Request')
        ident = msg.get('id')
        request = 'id' in msg
        if (msg.get('jsonrpc') != '2.0' or not isinstance(msg.get('method'), str)
                or set(msg) - {'jsonrpc', 'id', 'method', 'params'}
                or (request and (type(ident) not in (int, str) or
                    (type(ident) is int and abs(ident) > 9007199254740991)))
                or ('params' in msg and not isinstance(msg['params'], dict))):
            return error(None, -32600, 'Invalid Request') if request or not isinstance(msg.get('method'), str) else None
        method, params = msg['method'], msg.get('params', {})
        if not request:
            if method == 'notifications/initialized' and self.state == 'initializing' and not params:
                self.state = 'ready'
            return None
        if method == 'ping':
            result = {}
        elif method == 'initialize':
            ci = params.get('clientInfo')
            if (self.state != 'new' or not isinstance(params.get('protocolVersion'), str)
                    or not isinstance(params.get('capabilities'), dict)
                    or not isinstance(ci, dict) or not isinstance(ci.get('name'), str)
                    or not isinstance(ci.get('version'), str)):
                return error(ident, -32602, 'Invalid initialization')
            self.state = 'initializing'
            result = {'protocolVersion': PROTOCOL, 'capabilities': {'tools': {}},
                      'serverInfo': {'name': ('openxcom-semantic-agent-control' if self.agent_control is not None else
                                              'openxcom-supervised-stance-pilot' if self.supervised_stance is not None
                                              else 'openxcom-readonly-scaffold'),
                                     'version': '0.9.0' if self.agent_control is not None else '0.1.0'}}
        elif self.state != 'ready':
            return error(ident, -32000, 'Not initialized')
        elif method == 'tools/list':
            if params:
                return error(ident, -32602, 'Invalid params')
            result = {'tools': definitions(self.presentation is not None, self.presented_hud_enabled,
                                          self.mixed_audio is not None, self.supervised_stance is not None,
                                          self.supervised_selection_enabled, self.supervised_turn_enabled,
                                          self.supervised_map_left_click_enabled, self.supervised_inventory_enabled,
                                          self.supervised_inventory_close_enabled, self.supervised_inventory_ground_enabled,
                                          self.supervised_inventory_click_enabled, self.supervised_inventory_right_click_enabled,
                                          self.supervised_inventory_unload_enabled, self.agent_control is not None)}
        elif (method == 'tools/call' and self.agent_control is not None
                and params.get('name') in ('read_ui_state', 'execute_ui_action', 'read_ui_action_result', 'read_public_events')):
            from agent_control import token, validate_action_arguments, validate_event_arguments
            from snapshots import Unavailable, exact
            arguments = params.get('arguments', {})
            try:
                if set(params) - {'name', 'arguments', '_meta'} or type(arguments) is not dict:
                    raise Unavailable('Unavailable')
                if params['name'] == 'execute_ui_action':
                    validate_action_arguments(arguments)
                elif params['name'] == 'read_public_events':
                    validate_event_arguments(arguments)
                elif params['name'] == 'read_ui_action_result':
                    exact(arguments, 'command_id'); token(arguments['command_id'])
                else:
                    exact(arguments, '')
            except Unavailable:
                return error(ident, -32602, 'Invalid params')
            result = self.agent(params['name'], arguments)
        elif (method == 'tools/call' and self.supervised_stance is not None
                and (params.get('name') in ('read_stance_context', 'request_stance', 'read_stance_status')
                     or (params.get('name') == 'request_selection' and self.supervised_selection_enabled)
                     or (params.get('name') in ('read_tactical_context', 'request_turn')
                         and self.supervised_turn_enabled)
                     or (params.get('name') == 'request_map_left_click'
                         and self.supervised_map_left_click_enabled)
                     or (params.get('name') in ('request_open_inventory', 'read_inventory_view')
                         and self.supervised_inventory_enabled)
                     or (params.get('name') in ('request_close_inventory', 'read_inventory_close_context')
                         and self.supervised_inventory_close_enabled)
                     or (params.get('name') in ('request_inventory_ground', 'read_inventory_ground_context')
                         and self.supervised_inventory_ground_enabled)
                     or (params.get('name') in ('request_inventory_click', 'read_inventory_click_context')
                         and self.supervised_inventory_click_enabled)
                     or (params.get('name') in ('request_inventory_right_click', 'read_inventory_right_click_context')
                         and self.supervised_inventory_right_click_enabled)
                     or (params.get('name') in ('request_inventory_unload', 'read_inventory_unload_context')
                         and self.supervised_inventory_unload_enabled))):
            from supervised_stance import (token, validate_arguments, validate_selection_arguments,
                                           validate_turn_arguments, validate_map_left_click_arguments,
                                           validate_open_inventory_arguments, validate_close_inventory_arguments,
                                           validate_inventory_ground_arguments, validate_inventory_click_arguments,
                                           validate_inventory_right_click_arguments, validate_inventory_unload_arguments)
            from snapshots import Unavailable, exact
            arguments = params.get('arguments', {})
            try:
                if set(params) - {'name', 'arguments', '_meta'} or type(arguments) is not dict:
                    raise Unavailable('Unavailable')
                if params['name'] == 'request_stance':
                    validate_arguments(arguments)
                elif params['name'] == 'request_selection':
                    validate_selection_arguments(arguments)
                elif params['name'] == 'request_turn':
                    validate_turn_arguments(arguments)
                elif params['name'] == 'request_map_left_click':
                    validate_map_left_click_arguments(arguments)
                elif params['name'] == 'request_open_inventory':
                    validate_open_inventory_arguments(arguments)
                elif params['name'] == 'request_close_inventory':
                    validate_close_inventory_arguments(arguments)
                elif params['name'] == 'request_inventory_ground':
                    validate_inventory_ground_arguments(arguments)
                elif params['name'] == 'request_inventory_click':
                    validate_inventory_click_arguments(arguments)
                elif params['name'] == 'request_inventory_right_click':
                    validate_inventory_right_click_arguments(arguments)
                elif params['name'] == 'request_inventory_unload':
                    validate_inventory_unload_arguments(arguments)
                elif params['name'] == 'read_stance_status':
                    exact(arguments, 'command_id'); token(arguments['command_id'])
                else:
                    exact(arguments, '')
            except Unavailable:
                return error(ident, -32602, 'Invalid params')
            result = self.stance(params['name'], arguments)
        elif (method == 'tools/call' and self.mixed_audio is not None
                and params.get('name') == 'read_mixed_audio'):
            if (set(params) - {'name', 'arguments', '_meta'}
                    or type(params.get('arguments', {})) is not dict
                    or params.get('arguments', {})):
                return error(ident, -32602, 'Invalid params')
            result = self.mixed()
        elif (method == 'tools/call' and self.presentation is not None
                and (params.get('name') == 'read_presented_cursor'
                     or (params.get('name') == 'read_presented_hud' and self.presented_hud_enabled))):
            if set(params) - {'name', 'arguments', '_meta'} or params.get('arguments', {}) != {}:
                return error(ident, -32602, 'Invalid params')
            result = self.presented('hud' if params['name'] == 'read_presented_hud' else 'cursor')
        elif method == 'tools/call':
            if (set(params) - {'name', 'arguments', '_meta'} or params.get('name') not in NAMES
                    or type(params.get('arguments', {})) is not dict
                    or set(params.get('arguments', {})) - {'require_current'}
                    or type(params.get('arguments', {}).get('require_current', False)) is not bool):
                return error(ident, -32602, 'Invalid params')
            if params.get('arguments', {}).get('require_current', False):
                result = UNAVAILABLE
            else:
                result = self.historical('hud' if params['name'] == NAMES[0] else 'cursor')
        else:
            return error(ident, -32601, 'Method not found')
        return {'jsonrpc': '2.0', 'id': ident, 'result': result}


    def agent(self, name, arguments):
        from agent_control import ActionOutcomeUnknown
        from snapshots import Unavailable
        try:
            if name == 'read_ui_state':
                obj = self.agent_control.observe()
            elif name == 'execute_ui_action':
                obj = self.agent_control.execute(**arguments)
            elif name == 'read_public_events':
                obj = self.agent_control.read_events(**arguments)
            else:
                obj = self.agent_control.read_result(**arguments)
            return {'content': [{'type': 'text', 'text': json.dumps(obj, separators=(',', ':'))}],
                    'structuredContent': obj, 'isError': False}
        except ActionOutcomeUnknown as failure:
            obj = failure.public_error()
            return {'content': [{'type': 'text', 'text': json.dumps(obj, separators=(',', ':'))}],
                    'structuredContent': obj, 'isError': True}
        except Unavailable:
            return {'content': [{'type': 'text', 'text': '{"error":"unavailable"}'}],
                    'structuredContent': {'error': 'unavailable'}, 'isError': True}

    def stance(self, name, arguments):
        from supervised_stance import (PUBLIC_CONTEXT_SCHEMA, PUBLIC_TACTICAL_CONTEXT_SCHEMA,
                                       PUBLIC_STATUS_SCHEMA, PUBLIC_INVENTORY_VIEW_SCHEMA,
                                       PUBLIC_INVENTORY_CLOSE_CONTEXT_SCHEMA, PUBLIC_INVENTORY_GROUND_CONTEXT_SCHEMA,
                                       PUBLIC_INVENTORY_CLICK_CONTEXT_SCHEMA, PUBLIC_INVENTORY_RIGHT_CLICK_CONTEXT_SCHEMA,
                                       PUBLIC_INVENTORY_UNLOAD_CONTEXT_SCHEMA,
                                       image_content, tactical_image_content)
        from snapshots import Unavailable
        try:
            image = None
            if name == 'read_stance_context':
                obj, rgb = self.supervised_stance.observe()
                meta = {k: obj[k] for k in ('challenge', 'control_epoch', 'capability', 'semantics')}
                meta['schema'] = PUBLIC_CONTEXT_SCHEMA
                meta['panel'] = {'width': obj['panel']['width'], 'height': obj['panel']['height'],
                                 'format': 'image/png'}
                image = image_content(rgb)
            elif name == 'read_tactical_context':
                obj, rgb = self.supervised_stance.observe_tactical()
                meta = {k: obj[k] for k in ('challenge', 'control_epoch', 'capability', 'semantics')}
                meta['schema'] = PUBLIC_TACTICAL_CONTEXT_SCHEMA
                meta['view'] = {'width': obj['view']['width'], 'height': obj['view']['height'],
                                'format': 'image/png'}
                image = tactical_image_content(rgb)
            elif name == 'read_inventory_close_context':
                obj, rgb = self.supervised_stance.observe_inventory_close()
                meta = {k: obj[k] for k in ('challenge', 'control_epoch', 'capability', 'semantics')}
                meta['schema'] = PUBLIC_INVENTORY_CLOSE_CONTEXT_SCHEMA
                meta['view'] = {'width': obj['view']['width'], 'height': obj['view']['height'],
                                'format': 'image/png'}
                image = tactical_image_content(rgb)
            elif name == 'read_inventory_ground_context':
                obj, rgb = self.supervised_stance.observe_inventory_ground()
                meta = {k: obj[k] for k in ('challenge', 'control_epoch', 'capability', 'semantics')}
                meta['schema'] = PUBLIC_INVENTORY_GROUND_CONTEXT_SCHEMA
                meta['view'] = {'width': obj['view']['width'], 'height': obj['view']['height'],
                                'format': 'image/png'}
                image = tactical_image_content(rgb)
            elif name == 'read_inventory_unload_context':
                obj, rgb = self.supervised_stance.observe_inventory_unload()
                meta = {k: obj[k] for k in ('challenge', 'control_epoch', 'capability', 'semantics')}
                meta['schema'] = PUBLIC_INVENTORY_UNLOAD_CONTEXT_SCHEMA
                meta['view'] = {'width': obj['view']['width'], 'height': obj['view']['height'],
                                'format': 'image/png'}
                image = tactical_image_content(rgb)
            elif name == 'read_inventory_click_context':
                obj, rgb = self.supervised_stance.observe_inventory_click()
                meta = {k: obj[k] for k in ('challenge', 'control_epoch', 'capability', 'semantics')}
                meta['schema'] = PUBLIC_INVENTORY_CLICK_CONTEXT_SCHEMA
                meta['view'] = {'width': obj['view']['width'], 'height': obj['view']['height'],
                                'format': 'image/png'}
                image = tactical_image_content(rgb)
            elif name == 'read_inventory_right_click_context':
                obj, rgb = self.supervised_stance.observe_inventory_right_click()
                meta = {k: obj[k] for k in ('challenge', 'control_epoch', 'capability', 'semantics')}
                meta['schema'] = PUBLIC_INVENTORY_RIGHT_CLICK_CONTEXT_SCHEMA
                meta['view'] = {'width': obj['view']['width'], 'height': obj['view']['height'],
                                'format': 'image/png'}
                image = tactical_image_content(rgb)
            elif name == 'read_inventory_view':
                obj, rgb = self.supervised_stance.observe_inventory()
                meta = {k: obj[k] for k in ('challenge', 'semantics')}
                meta['schema'] = PUBLIC_INVENTORY_VIEW_SCHEMA
                meta['view'] = {'width': obj['view']['width'], 'height': obj['view']['height'],
                                'format': 'image/png'}
                image = tactical_image_content(rgb)
            else:
                if name == 'request_stance':
                    obj = self.supervised_stance.request(**arguments)
                elif name == 'request_selection':
                    obj = self.supervised_stance.request_selection(**arguments)
                elif name == 'request_turn':
                    obj = self.supervised_stance.request_turn(**arguments)
                elif name == 'request_map_left_click':
                    obj = self.supervised_stance.request_map_left_click(**arguments)
                elif name == 'request_open_inventory':
                    obj = self.supervised_stance.request_open_inventory(**arguments)
                elif name == 'request_close_inventory':
                    obj = self.supervised_stance.request_close_inventory(**arguments)
                elif name == 'request_inventory_ground':
                    obj = self.supervised_stance.request_inventory_ground(**arguments)
                elif name == 'request_inventory_unload':
                    obj = self.supervised_stance.request_inventory_unload(**arguments)
                elif name == 'request_inventory_click':
                    obj = self.supervised_stance.request_inventory_click(**arguments)
                elif name == 'request_inventory_right_click':
                    obj = self.supervised_stance.request_inventory_right_click(**arguments)
                else:
                    obj = self.supervised_stance.read_status(**arguments)
                meta = {'schema': PUBLIC_STATUS_SCHEMA, 'command_id': obj['command_id'],
                        'status': obj['status'], 'semantics': 'dispatch_status_not_success'}
            content = [{'type': 'text', 'text': json.dumps(meta, separators=(',', ':'))}]
            if image is not None:
                content.append(image)
            return {'content': content, 'structuredContent': meta, 'isError': False}
        except Unavailable:
            return {'content': [{'type': 'text', 'text': '{"error":"unavailable"}'}],
                    'structuredContent': {'error': 'unavailable'}, 'isError': True}

    def mixed(self):
        from mixed_audio import PUBLIC_SCHEMA, audio_content
        from snapshots import Unavailable
        try:
            obj, pcm = self.mixed_audio.observe()
            meta = {k: v for k, v in obj.items() if k not in ('pcm_hex', 'next_token')}
            meta['schema'] = PUBLIC_SCHEMA
            return {'content': [{'type': 'text', 'text': json.dumps(meta, separators=(',', ':'))},
                                audio_content(pcm)], 'isError': False}
        except Unavailable:
            return {'content': [{'type': 'text', 'text': '{"available":false,"reason":"observation_unavailable"}'}], 'isError': True}

    def presented(self, kind='cursor'):
        from snapshots import Unavailable, image_content, require
        try:
            require(kind in ('cursor', 'hud') and (kind != 'hud' or self.presented_hud_enabled))
            obj, candidate = self.presentation.observe()
            if kind == 'hud':
                require(obj['schema'] == 'experimental-presentation/2')
                # Fresh HUD comes only from this validated presentation reply;
                # historical snapshots are never read or substituted here.
                hud = {'schema': 'openxcom-mcp-presented-hud/1',
                       **{k: obj[k] for k in ('epoch', 'challenge', 'semantics', 'profile', 'hud')}}
                return {'content': [{'type': 'text', 'text': json.dumps(hud, separators=(',', ':'))}], 'isError': False}
            image = image_content(candidate)
            obj = {k: v for k, v in obj.items() if k not in ('next_token', 'hud')}
            obj['schema'] = 'openxcom-mcp-presented-cursor/1'
            obj['crop'] = {k: v for k, v in obj['crop'].items() if k not in ('pixels', 'format')}
            obj['crop']['format'] = 'image/png'
            return {'content': [{'type': 'text', 'text': json.dumps(obj, separators=(',', ':'))}, image], 'isError': False}
        except Unavailable:
            return {'content': [{'type': 'text', 'text': '{"available":false,"reason":"observation_unavailable"}'}], 'isError': True}

    def historical(self, kind):
        from snapshots import Unavailable, read_candidate, image_content
        # Fixed filenames only; paths are trusted launch configuration, not MCP input.
        try:
            obj = read_candidate(self.paths[kind], kind)
            if kind == 'cursor' and obj['crop'] is not None:
                image = image_content(obj)
                obj['crop'] = {k: v for k, v in obj['crop'].items() if k not in ('pixels', 'format')}
                obj['crop']['format'] = 'image/png'
                obj['source_schema'] = obj['schema']
                obj['schema'] = 'openxcom-mcp-last-cursor/0'
                content = [{'type': 'text', 'text': json.dumps(obj, separators=(',', ':'))}, image]
            else:
                if kind == 'cursor':
                    obj['source_schema'] = obj['schema']
                    obj['schema'] = 'openxcom-mcp-last-cursor/0'
                content = [{'type': 'text', 'text': json.dumps(obj, separators=(',', ':'))}]
            return {'content': content, 'isError': False}
        except Unavailable:
            return {'content': [{'type': 'text', 'text':
                '{"available":false,"freshness":"unverified","reason":"observation_unavailable"}'}], 'isError': True}


def serve(source, sink, hud_dir=None, cursor_dir=None, presentation=None, mixed_audio=None, supervised_stance=None, agent_control=None):
    server = Server(hud_dir, cursor_dir, presentation, mixed_audio, supervised_stance, agent_control)
    while True:
        raw = source.readline(MAX_MESSAGE + 1)
        if not raw:
            return
        if len(raw) > MAX_MESSAGE:
            # Close instead of buffering/draining an attacker-controlled oversized line.
            reply = error(None, -32600, 'Message too large')
            sink.write(json.dumps(reply, separators=(',', ':')).encode() + b'\n')
            sink.flush()
            return
        try:
            msg = strict_json(raw.decode('utf-8'))
            reply = server.handle(msg)
        except (ValueError, UnicodeError, RecursionError):
            reply = error(None, -32700, 'Parse error')
        if reply is not None:
            out = json.dumps(reply, separators=(',', ':'), ensure_ascii=True).encode() + b'\n'
            if len(out) > MAX_RESPONSE:
                raise RuntimeError('response bound exceeded')
            sink.write(out)
            sink.flush()


if __name__ == '__main__':
    import argparse
    class QuietArgumentParser(argparse.ArgumentParser):
        def error(self, message):
            self.exit(2, 'Invalid observer arguments; use --help.\n')
    parser = QuietArgumentParser(description=__doc__)
    parser.add_argument('--hud-dir')
    parser.add_argument('--cursor-dir')
    config = parser.parse_args()
    try:
        serve(sys.stdin.buffer, sys.stdout.buffer, config.hud_dir, config.cursor_dir)
    except BrokenPipeError:
        silence_broken_stdout()
    except KeyboardInterrupt:
        pass
    except Exception:
        # Do not expose private paths or arbitrary exception payloads.
        try:
            sys.stderr.write('Observer transport unavailable.\n')
            sys.stderr.flush()
        except (OSError, ValueError):
            pass
        sys.exit(1)
