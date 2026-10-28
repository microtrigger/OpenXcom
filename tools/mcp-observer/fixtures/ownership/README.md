# Pinned ownership regression baselines

These are historical source fixtures, not executable test modules. The tests
extract only the original selected methods into controlled fake-owner cases.
They retain the original AST-delta and release-barrier assertions; they do not
claim socket timing, live gameplay, or a complete historical checkout.

Restoration used the original archived child source plus its original commit
patch, verified against the archive manifest, then reverse-applied only that
source diff. Both child and parent Git blob prefixes matched the archived patch.
The parent commit IDs are recorded by the original archive manifests; the full
commit objects were unavailable. No current candidate code generated a baseline.
The recovered whole-file SHA-256 below is checked on every test read, including
under Python -O. No Git executable or repository history is needed at test time.

## Projects
- Original path: `tools/mcp-observer/test_agent_projects.py`
- Parent commit: `639679d42155f27680356b5f9241106cab08c4b7`
- Child commit carrying the patch: `61a8ab2cc97143e13b78338b8f63c3b2eab33715`
- Parent blob SHA-1 (computed; archived prefix `663769e71`): `663769e712640b512d12c3d7ade4d28ff2b0eb46`
- Child blob SHA-1: `f22f48c74b3175015b832db6c76ef4834b53d95d`
- Fixture SHA-256: `9e079f7678ed591d07a8507972039c789d26a66fb6192c9dcc7dc6accb353c0b`
- Original commit.patch SHA-256: `0fcc85e807b255f34f80200ff667f0fdb2435a75d5ad582dffd4dc33b0c9b7f8`

## Transfer
- Original path: `tools/mcp-observer/test_agent_transfer.py`
- Parent commit: `61a8ab2cc97143e13b78338b8f63c3b2eab33715`
- Child commit carrying the patch: `ae1529f97edbb3ddbd7ae7528e37e94eb9b1ce54`
- Parent blob SHA-1 (computed; archived prefix `e6eba2b3a`): `e6eba2b3a5dfa619cc6e4355a9f52259d1bbe52d`
- Child blob SHA-1: `a5cbd2eb377eebb6b49c8a09d948eb683a1d2f9c`
- Fixture SHA-256: `7af69e5118cf7fdca3194f337285b8f394aafc494eae1b8f744c6878a4b06bba`
- Original commit.patch SHA-256: `f8c197057b559c892c7d9156ffec69a059aedb11cc026658f07172e145d1fd76`

