# SPDX-License-Identifier: GPL-3.0-or-later
"""Closed, public failure classifications. Never include exception text or payloads."""
CODES = {
    'transport_timeout': ('transport', 'error', 'stop'),
    'read_recovery_required': ('transport', 'warning', 'refresh_observation'),
    'transport_eof': ('transport', 'error', 'stop'),
    'transport_framing': ('protocol', 'error', 'stop'),
    'transport_io': ('transport', 'error', 'stop'),
    'channel_closed': ('transport', 'error', 'stop'),
    'unexpected_packet': ('protocol', 'error', 'stop'),
    'parse_error': ('protocol', 'error', 'stop'),
    'validation_error': ('protocol', 'error', 'stop'),
    'stale_authority': ('admission', 'warning', 'refresh_observation'),
    'action_not_offered': ('admission', 'warning', 'refresh_observation'),
    'target_not_offered': ('admission', 'warning', 'refresh_observation'),
    'command_conflict': ('request', 'error', 'stop'),
    'resource_limit': ('resource', 'error', 'stop'),
    'concurrent_request': ('concurrency', 'warning', 'stop'),
    'native_unavailable_unspecified': ('native', 'warning', 'stop'),
    'native_result_unknown': ('native', 'critical', 'reconcile'),
    'native_result_unknown_after_dispatch': ('native', 'critical', 'reconcile'),
    'native_result_unavailable': ('native', 'warning', 'reconcile'),
    'invalid_arguments': ('request', 'warning', 'correct_request'),
    'legacy_unavailable': ('unknown', 'error', 'stop'),
}
PHASES = frozenset(('read', 'pre_send', 'send_or_receipt', 'result_query', 'unknown'))
OUTCOMES = frozenset(('not_applicable', 'not_sent', 'unknown'))

def detail(code, phase='read', action_outcome='not_applicable'):
    if type(code) is not str or code not in CODES: code = 'legacy_unavailable'
    if type(phase) is not str or phase not in PHASES: phase = 'unknown'
    if type(action_outcome) is not str or action_outcome not in OUTCOMES: action_outcome = 'not_applicable'
    category, severity, recovery = CODES[code]
    if action_outcome == 'unknown': severity, recovery = 'critical', 'reconcile'
    return dict(code=code, category=category, severity=severity, phase=phase,
                action_outcome=action_outcome, recovery=recovery)

def classified(code):
    from snapshots import Unavailable
    return Unavailable('Unavailable', failure_code=code)

def ensure(condition, code):
    if not condition: raise classified(code)

def code_for(error, fallback='legacy_unavailable'):
    code = getattr(error, 'failure_code', None)
    if type(code) is str and code in CODES: return code
    return 'transport_io' if isinstance(error, OSError) else fallback
