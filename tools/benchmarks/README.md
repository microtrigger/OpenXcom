# Offline observation microbenchmark

```sh
python3 benchmark.py --adapter-dir ../mcp-observer --repeats 50 --out ./local-results
python3 -m unittest discover -s . -v
```

Run from this directory. Inputs are synthetic RGB or separately supplied lawful local observations. Do not commit generated results or game assets.

This measures encoding size and local processing time, not model tokens or end-to-end MCP-versus-GUI performance. That comparison has not run.
