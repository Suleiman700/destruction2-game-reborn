# Save the PNG from an REA capture_web_screenshot result file.
import json, sys, base64
d = json.load(open(sys.argv[1]))
def find(o):
    if isinstance(o, dict):
        for v in o.values():
            r = find(v)
            if r: return r
    elif isinstance(o, list):
        for v in o:
            r = find(v)
            if r: return r
    elif isinstance(o, str) and o.startswith('iVBOR'):
        return o
print(open(sys.argv[2], 'wb').write(base64.b64decode(find(d))))
