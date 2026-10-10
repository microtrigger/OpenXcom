# SPDX-License-Identifier: GPL-3.0-or-later
# New contribution maintained in this OpenXcom fork, 2026-10-10.
# See repository LICENSE.txt and LICENSE-NOTICES.md.
"""Launch one opt-in game with independent transient observation channels.
Command arguments are trusted operator configuration, never MCP arguments.
"""
import argparse
from contextlib import ExitStack
import os
import socket
import subprocess
import sys
from adapter import serve
from presentation import PresentationClient
from mixed_audio import MixedAudioClient


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--hud-dir')
    parser.add_argument('--cursor-dir')
    parser.add_argument('--presented-hud', action='store_true',
                        help='explicitly select presentation version 2 for displayed HUD reads')
    parser.add_argument('--mixed-audio', action='store_true',
                        help='also enable the independent fixed-window mixed-audio channel')
    parser.add_argument('--audio-only', action='store_true',
                        help='enable mixed audio without a presentation channel')
    parser.add_argument('command', nargs=argparse.REMAINDER)
    args = parser.parse_args()
    command = args.command
    if command and command[0] == '--':
        command = command[1:]
    if not command:
        parser.error('an explicit game command is required after --')
    if args.audio_only and args.presented_hud:
        parser.error('--audio-only cannot be combined with --presented-hud')
    env = os.environ.copy()
    # Ambient descriptors and versions cannot silently enable another stream.
    for key in ('OPENXCOM_PRESENTATION_FD', 'OPENXCOM_PRESENTATION_VERSION',
                'OPENXCOM_MIXED_AUDIO_FD'):
        env.pop(key, None)
    with ExitStack() as cleanup:
        children = []
        client = audio = None
        if not args.audio_only:
            parent, child = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
            cleanup.callback(parent.close); cleanup.callback(child.close)
            children.append(child)
            env['OPENXCOM_PRESENTATION_FD'] = str(child.fileno())
            if args.presented_hud:
                env['OPENXCOM_PRESENTATION_VERSION'] = '2'
            client = PresentationClient(parent, version=2 if args.presented_hud else 1)
            cleanup.callback(client.close)
        if args.mixed_audio or args.audio_only:
            parent, child = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
            cleanup.callback(parent.close); cleanup.callback(child.close)
            children.append(child)
            env['OPENXCOM_MIXED_AUDIO_FD'] = str(child.fileno())
            audio = MixedAudioClient(parent)
            cleanup.callback(audio.close)
        # Child diagnostics cannot corrupt MCP stdout. Display/audio settings and
        # trusted operator game arguments are inherited without modification.
        subprocess.Popen(command, env=env, pass_fds=tuple(c.fileno() for c in children),
                         stdin=subprocess.DEVNULL, stdout=sys.stderr)
        for child in children:
            child.close()
        serve(sys.stdin.buffer, sys.stdout.buffer, args.hud_dir, args.cursor_dir, client, audio)
    # EOF ends observation only. Never terminate the normally running game.


if __name__ == '__main__':
    main()
