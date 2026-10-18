# Extract screenshots (as PNGs) and console/page events from an REA capture_browser_scenario result file.
import json, sys, base64
d = json.load(open(sys.argv[1])); prefix = sys.argv[2]
n = d['normalized_result']
for i, s in enumerate(n['steps']):
    shot = (s.get('artifacts') or {}).get('screenshot', {}).get('value', {}).get('data_base64')
    sid = s.get('step_id') or s.get('id') or i
    if shot:
        fn = f'{prefix}_{i:02d}.png'; open(fn, 'wb').write(base64.b64decode(shot))
        print(i, sid, s.get('status'), fn)
    else:
        print(i, sid, s.get('status'), s.get('error'))
for e in n['events']['items']:
    if 'favicon' in json.dumps(e.get('url', '')): continue
    print('EVENT', e.get('kind'), e.get('level'), (e.get('text') or '')[:300])
