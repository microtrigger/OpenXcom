# SPDX-License-Identifier: GPL-3.0-or-later
# Created 2026-10-10. See the OpenXcom project's LICENSE.txt for full terms.
"""Offline usage collector for explicitly supplied Responses API objects.
No credentials, network calls, session-log access, or fabricated actual usage.
"""
from copy import deepcopy

FIELDS = ('input_tokens', 'output_tokens', 'total_tokens')


def extract_usage(response):
    """Accept dict or SDK object; retain numeric usage only, never response text/IDs."""
    if hasattr(response, 'model_dump'):
        response = response.model_dump()
    usage = response.get('usage')
    if not isinstance(usage, dict):
        return None
    result = {key: usage.get(key) for key in FIELDS}
    for detail, key in (('input_tokens_details', 'cached_tokens'),
                        ('input_tokens_details', 'cache_write_tokens'),
                        ('output_tokens_details', 'reasoning_tokens')):
        result[key] = (usage.get(detail) or {}).get(key)
    for value in result.values():
        if value is not None and (type(value) is not int or value < 0):
            raise ValueError('usage must contain nonnegative integer counts or null')
    if all(result[k] is not None for k in FIELDS):
        if result['total_tokens'] != result['input_tokens'] + result['output_tokens']:
            raise ValueError('inconsistent token total')
    for sub, total in (('cached_tokens', 'input_tokens'), ('reasoning_tokens', 'output_tokens')):
        if result[sub] is not None and result[total] is not None and result[sub] > result[total]:
            raise ValueError('token subset exceeds total')
    return result


class UsageLedger:
    """Call begin BEFORE every network attempt; call finish even on failure.
    Disable opaque SDK retries or instrument each attempt. Never sum aggregate
    turn usage together with its individual call usage. Local ordinal IDs only.
    """
    def __init__(self):
        self.calls = []

    def begin(self):
        self.calls.append({'ordinal': len(self.calls), 'status': 'pending', 'usage': None})
        return len(self.calls) - 1

    def finish(self, ordinal, response=None, status='completed'):
        if self.calls[ordinal]['status'] != 'pending':
            raise ValueError('attempt already finished')
        if status not in ('completed', 'incomplete', 'failed', 'cancelled'):
            raise ValueError('invalid terminal status')
        self.calls[ordinal].update(status=status, usage=extract_usage(response or {}))

    def report(self):
        # Mandatory input, output, total, cached. Reasoning/cache writes preserved
        # when reported; absent stays null, never invented as zero.
        required = FIELDS + ('cached_tokens',)
        complete = bool(self.calls) and all(c['status'] != 'pending' and c['usage'] is not None
                        and all(c['usage'][k] is not None for k in required) for c in self.calls)
        keys = FIELDS + ('cached_tokens', 'cache_write_tokens', 'reasoning_tokens')
        totals = {k: sum(c['usage'][k] for c in self.calls)
                  if self.calls and all(c['usage'] is not None and c['usage'][k] is not None for c in self.calls)
                  else None for k in keys}
        return {'schema':'openxcom-model-usage-ledger/1', 'usage_complete':complete,
                'calls':deepcopy(self.calls), 'totals':totals,
                'bill_cost':None, 'model_compute':None,
                'scope':'recorded attempts only; caller must attest all model calls/retries are instrumented'}
