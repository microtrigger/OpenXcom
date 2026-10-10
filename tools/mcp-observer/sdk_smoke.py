# SPDX-License-Identifier: GPL-3.0-or-later
# New contribution maintained in this OpenXcom fork, 2026-10-10.
# This file is free software: you may redistribute and/or modify it under
# GNU GPL version 3 or, at your option, any later version.
# Provided without warranty; see the repository LICENSE.txt and LICENSE-NOTICES.md.
"""Optional integration check with installed official mcp==1.29.0; no network."""
import asyncio
import base64
import fcntl
import json
import os
import sys
import tempfile
from datetime import timedelta
from importlib.metadata import version
from pathlib import Path
from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client
from test_adapter import fixture, dump
from snapshots import encode_png

async def main():
    with tempfile.TemporaryDirectory() as directory:
        root=Path(directory)
        for name,payload in [('cursor.json',dump(fixture())), ('cursor.lock',b'')]:
            path=root/name; path.write_bytes(payload); path.chmod(0o600)
        fd=os.open(root/'cursor.lock',os.O_RDWR)
        try:
            fcntl.flock(fd,fcntl.LOCK_EX)
            params=StdioServerParameters(command=sys.executable,
                args=[str(Path(__file__).with_name('adapter.py')), '--cursor-dir', directory])
            async with stdio_client(params) as (read,write):
                async with ClientSession(read,write,read_timeout_seconds=timedelta(seconds=5)) as client:
                    init=await client.initialize()
                    listed=await client.list_tools()
                    assert len(listed.tools)==2
                    result=await client.call_tool('read_last_cursor',{})
                    assert not result.isError
                    assert json.loads(result.content[0].text)['freshness']=='unverified'
                    assert base64.b64decode(result.content[1].data)==encode_png(fixture())
                    denied=await client.call_tool('read_last_cursor',{'require_current':True})
                    assert denied.isError and len(denied.content)==1
                    print(json.dumps({'sdk_version':version('mcp'), 'protocol':init.protocolVersion,
                        'initialize':True,'list_tools':True,'historical_png':True,'current_denied':True}))
        finally: os.close(fd)

if __name__=='__main__':asyncio.run(main())
