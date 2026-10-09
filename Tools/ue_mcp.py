"""Compact client for the editor's MCP server (VibeUE, port 8000): prints only results, not tool catalogs.

  python Tools/ue_mcp.py tools                 one line per top-level tool
  python Tools/ue_mcp.py py "print(1)"         run Python in the editor, print its output
  python Tools/ue_mcp.py pyfile script.py      same, from a file
  python Tools/ue_mcp.py shot out.png [source] [max_width]   screenshot (source: editor/viewport/game/window...)
  python Tools/ue_mcp.py call <tool> '<json args>'           any top-level tool, text output only
"""
import base64
import json
import os
import sys
import urllib.request

URL = os.environ.get('UE_MCP_URL', 'http://127.0.0.1:8000/mcp')


def post(body, session=None):
    headers = {'Content-Type': 'application/json', 'Accept': 'application/json, text/event-stream'}
    if session:
        headers['Mcp-Session-Id'] = session
    req = urllib.request.Request(URL, json.dumps(body).encode(), headers)
    with urllib.request.urlopen(req, timeout=600) as r:
        text = r.read().decode('utf-8', 'replace')
        sid = r.headers.get('Mcp-Session-Id')
    if text.startswith('event:') or text.startswith('data:'):
        text = ''.join(l[5:] for l in text.splitlines() if l.startswith('data:'))
    return (json.loads(text) if text.strip() else None), sid


def session():
    _, sid = post({'jsonrpc': '2.0', 'id': 1, 'method': 'initialize', 'params': {
        'protocolVersion': '2025-06-18', 'capabilities': {}, 'clientInfo': {'name': 'glaze', 'version': '1'}}})
    post({'jsonrpc': '2.0', 'method': 'notifications/initialized'}, sid)
    return sid


def call(name, args):
    sid = session()
    res, _ = post({'jsonrpc': '2.0', 'id': 2, 'method': 'tools/call', 'params': {'name': name, 'arguments': args}}, sid)
    if 'error' in res:
        raise SystemExit('MCP error: ' + json.dumps(res['error'])[:2000])
    return res['result']


def print_text(result):
    for c in result.get('content', []):
        if c.get('type') == 'text':
            t = c['text']
            try:  # tools often wrap output in JSON
                j = json.loads(t)
                if isinstance(j, dict):
                    out = j.get('output') or j.get('result') or j.get('stdout')
                    err = j.get('error') or j.get('error_message') or j.get('stderr')
                    if out is not None or err:
                        if out:
                            print(out if isinstance(out, str) else json.dumps(out, ensure_ascii=False))
                        if err:
                            print('ERROR:', err)
                        continue
            except ValueError:
                pass
            print(t)
        elif c.get('type') == 'image':
            print('[image %s]' % c.get('mimeType'))


def main():
    a = sys.argv[1:]
    if not a or a[0] == 'tools':
        sid = session()
        res, _ = post({'jsonrpc': '2.0', 'id': 2, 'method': 'tools/list'}, sid)
        for t in res['result']['tools']:
            print(t['name'], list(t.get('inputSchema', {}).get('properties', {}).keys()))
    elif a[0] in ('py', 'pyfile'):
        code = open(a[1], encoding='utf-8').read() if a[0] == 'pyfile' else a[1]
        print_text(call('execute_python_code', {'code': code}))
    elif a[0] == 'shot':
        args = {'source': a[2] if len(a) > 2 else 'editor'}
        if len(a) > 3:
            args['max_width'] = int(a[3])
        res = call('capture_image', args)
        for c in res.get('content', []):
            if c.get('type') == 'image':
                open(a[1], 'wb').write(base64.b64decode(c['data']))
                print('saved', a[1])
                return
        print_text(res)
    elif a[0] == 'call':
        print_text(call(a[1], json.loads(a[2]) if len(a) > 2 else {}))


if __name__ == '__main__':
    sys.stdout.reconfigure(encoding='utf-8')
    main()
