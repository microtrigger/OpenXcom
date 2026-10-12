# SPDX-License-Identifier: GPL-3.0-or-later
# New contribution maintained in this OpenXcom fork, 2026-10-10.
# Supervised stance integration added 2026-10-11, GPL-3.0-or-later.
# See repository LICENSE.txt and LICENSE-NOTICES.md.
"""Launch one game with independent opt-in observation and supervised control channels.
Command arguments are trusted operator configuration, never MCP arguments.
"""
import argparse
from contextlib import ExitStack
import os
import socket
import subprocess
import sys
import warnings
from adapter import serve, silence_broken_stdout
from presentation import PresentationClient
from mixed_audio import MixedAudioClient
from supervised_stance import StanceClient


class QuietArgumentParser(argparse.ArgumentParser):
    def error(self, message):
        # argparse normally echoes unrecognized arguments and paths.
        self.exit(2, 'Invalid observer launch arguments; use --help.\n')


def main():
    # Leaving a live game running on protocol EOF is intentional. Python's
    # Popen destructor must not echo process IDs or library paths as warnings.
    with warnings.catch_warnings():
        warnings.filterwarnings('ignore', category=ResourceWarning, module='subprocess')
        try:
            return run()
        except BrokenPipeError:
            silence_broken_stdout()
            return 0
        except KeyboardInterrupt:
            return 0
        except Exception:
            # Neither child data nor exception text belongs on a model-visible channel.
            try:
                sys.stderr.write('Observer launch or transport unavailable.\n')
                sys.stderr.flush()
            except (OSError, ValueError):
                pass
            return 1


def run():
    parser = QuietArgumentParser(description=__doc__)
    parser.add_argument('--hud-dir')
    parser.add_argument('--cursor-dir')
    parser.add_argument('--presented-hud', action='store_true',
                        help='explicitly select presentation version 2 for displayed HUD reads')
    parser.add_argument('--mixed-audio', action='store_true',
                        help='also enable the independent fixed-window mixed-audio channel')
    parser.add_argument('--supervised-stance', action='store_true',
                        help='enable the experimental local-UI-confirmed stance pilot')
    parser.add_argument('--supervised-selection', action='store_true',
                        help='also enable local-UI-confirmed next/previous eligible player unit selection; requires --supervised-stance')
    parser.add_argument('--supervised-turn', action='store_true',
                        help='also enable local-UI-confirmed ordinary tactical right-click (turn or open a door); requires --supervised-stance')
    parser.add_argument('--supervised-map-left-click', action='store_true',
                        help='also enable local-UI-confirmed ordinary map left-click (may move, select or do nothing); requires --supervised-stance and --supervised-turn')
    parser.add_argument('--audio-only', action='store_true',
                        help='enable mixed audio without a presentation channel')
    parser.add_argument('command', nargs=argparse.REMAINDER)
    args = parser.parse_args()
    command = args.command
    if command and command[0] == '--':
        command = command[1:]
    if not command:
        parser.error('an explicit game command is required after --')
    if args.supervised_selection and not args.supervised_stance:
        parser.error('--supervised-selection requires --supervised-stance')
    if args.supervised_turn and not args.supervised_stance:
        parser.error('--supervised-turn requires --supervised-stance')
    if args.supervised_map_left_click and not (args.supervised_stance and args.supervised_turn):
        parser.error('--supervised-map-left-click requires --supervised-stance and --supervised-turn')
    if args.audio_only and (args.presented_hud or args.supervised_stance):
        parser.error('--audio-only cannot be combined with visual channels')
    env = os.environ.copy()
    # Ambient descriptors and versions cannot silently enable another stream.
    for key in ('OPENXCOM_PRESENTATION_FD', 'OPENXCOM_PRESENTATION_VERSION',
                'OPENXCOM_MIXED_AUDIO_FD', 'OPENXCOM_STANCE_FD', 'OPENXCOM_SELECTION_ENABLED',
                'OPENXCOM_TURN_ENABLED', 'OPENXCOM_MAP_LEFT_CLICK_ENABLED'):
        env.pop(key, None)
    with ExitStack() as cleanup:
        children = []
        client = audio = stance = None
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
        if args.supervised_stance:
            parent, child = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
            cleanup.callback(parent.close); cleanup.callback(child.close)
            children.append(child)
            env['OPENXCOM_STANCE_FD'] = str(child.fileno())
            if args.supervised_selection:
                env['OPENXCOM_SELECTION_ENABLED'] = '1'
            if args.supervised_turn:
                env['OPENXCOM_TURN_ENABLED'] = '1'
            if args.supervised_map_left_click:
                env['OPENXCOM_MAP_LEFT_CLICK_ENABLED'] = '1'
            stance = StanceClient(parent, selection_enabled=args.supervised_selection,
                                  turn_enabled=args.supervised_turn,
                                  map_left_click_enabled=args.supervised_map_left_click)
            cleanup.callback(stance.close)
        # Discard both diagnostics streams at the OS boundary, including startup,
        # logger fallback and shutdown. No pipe, decoding, reader or log buffer.
        # The native producer separately qualifies effective engine options.
        process = subprocess.Popen(command, env=env, pass_fds=tuple(c.fileno() for c in children),
                                   stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
                                   stderr=subprocess.DEVNULL)
        for child in children:
            child.close()
        try:
            serve(sys.stdin.buffer, sys.stdout.buffer, args.hud_dir, args.cursor_dir, client, audio, stance)
        finally:
            process.poll() # Reap only if already exited; never wait for or stop the game.
    # EOF ends observation only. Never terminate the normally running game.


if __name__ == '__main__':
    sys.exit(main())
