# SPDX-License-Identifier: GPL-3.0-or-later
# New contribution maintained in this OpenXcom fork, 2026-10-10.
# See repository LICENSE.txt and LICENSE-NOTICES.md.
"""Launch one opt-in game with a transient inherited presentation channel.
Command arguments are trusted operator configuration, never MCP arguments.
"""
import argparse
import os
import socket
import subprocess
import sys
from adapter import serve
from presentation import PresentationClient


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--hud-dir')
    parser.add_argument('--cursor-dir')
    parser.add_argument('--presented-hud', action='store_true',
                        help='explicitly select presentation version 2 for displayed HUD reads')
    parser.add_argument('command', nargs=argparse.REMAINDER)
    args = parser.parse_args()
    command = args.command
    if command and command[0] == '--':
        command = command[1:]
    if not command:
        parser.error('an explicit game command is required after --')
    parent, child = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
    env = os.environ.copy()
    env['OPENXCOM_PRESENTATION_FD'] = str(child.fileno())
    # Ambient configuration must not silently opt the child into another version.
    env.pop('OPENXCOM_PRESENTATION_VERSION', None)
    if args.presented_hud:
        env['OPENXCOM_PRESENTATION_VERSION'] = '2'
    # Child diagnostics cannot corrupt the MCP stdout stream. Existing display
    # settings and operator game arguments are inherited without modification.
    try:
        subprocess.Popen(command, env=env, pass_fds=(child.fileno(),),
                         stdin=subprocess.DEVNULL, stdout=sys.stderr)
    finally:
        child.close()
    client = PresentationClient(parent, version=2 if args.presented_hud else 1)
    try:
        serve(sys.stdin.buffer, sys.stdout.buffer, args.hud_dir, args.cursor_dir, client)
    finally:
        client.close()
    # EOF ends observation only. Never terminate the normally running game.


if __name__ == '__main__':
    main()
