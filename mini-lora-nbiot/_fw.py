import os, sys
BASE = sys.argv[1]
relpath = sys.argv[2]
with open(sys.argv[3], 'r', encoding='utf-8') as src:
    content = src.read()
path = os.path.join(BASE, relpath)
os.makedirs(os.path.dirname(path), exist_ok=True)
with open(path, 'w', encoding='utf-8') as dst:
    dst.write(content)
print(f'Copied to {relpath}: {len(content.splitlines())} lines')
