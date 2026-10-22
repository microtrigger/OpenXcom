# SPDX-License-Identifier: GPL-3.0-or-later
"""Privacy-bounded tool-failure evidence; not a retry policy.
The closed vocabulary is checked against the runtime contract by unit tests.
"""
CODES = {'transport_timeout': ('transport', 'error', 'stop'),
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
 'legacy_unavailable': ('unknown', 'error', 'stop')}
METHODS = frozenset(('read_ui_state','execute_ui_action','read_ui_action_result','read_public_events'))
PHASES = frozenset(('read','pre_send','send_or_receipt','result_query','unknown'))
OUTCOMES = frozenset(('not_applicable','not_sent','unknown'))
STATUSES = frozenset(('outcome_unknown','unknown_after_dispatch','unavailable','dispatched','unknown'))

def safe_tool_failure(name, result):
    evidence = {'method':name if isinstance(name,str) and name in METHODS else 'other', 'automatic_replay':False}
    if not isinstance(result,dict):return evidence
    evidence['is_error'] = result.get('isError') is True
    obj=result.get('structuredContent')
    if not isinstance(obj,dict):return evidence
    if obj.get('error')=='unavailable':evidence['error']='unavailable'
    if isinstance(obj.get('status'),str) and obj['status'] in STATUSES:evidence['status']=obj['status']
    failure=obj.get('failure')
    if not isinstance(failure,dict):return evidence
    code,phase,outcome=(failure.get(k) for k in ('code','phase','action_outcome'))
    if not all(isinstance(v,str) for v in (code,phase,outcome)):return evidence
    if code not in CODES or phase not in PHASES or outcome not in OUTCOMES:return evidence
    category,severity,recovery=CODES[code]
    if outcome=='unknown':severity,recovery='critical','reconcile'
    evidence['failure']=dict(code=code,category=category,severity=severity,
                            phase=phase,action_outcome=outcome,recovery=recovery)
    return evidence
