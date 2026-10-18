#!/usr/bin/env python3
"""Keep every known FAIL visible, and reject any newly failing ROM/hash."""
import argparse
import json
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--current',required=True,type=Path)
p.add_argument('--baseline',required=True,type=Path)
a=p.parse_args()
def tests(path):
    data=json.loads(path.read_text(encoding='utf-8'))
    rows=data.get('tests',data.get('results',[]))
    if not rows:raise SystemExit('Missing/empty test results: '+str(path))
    return {(r.get('suite',''),r['rom']):r for r in rows}
old,new=tests(a.baseline),tests(a.current)
errors=[]
for key,row in old.items():
    result=new.get(key)
    if result is None:errors.append(('missing',key));continue
    if row.get('sha256')!=result.get('sha256'):errors.append(('input hash changed',key));continue
    if row.get('expected_sha256')!=result.get('expected_sha256'):
        errors.append(('reference image hash changed',key));continue
    if row.get('model')!=result.get('model'):
        errors.append(('hardware model changed',key));continue
    if result['status']!='PASS' and result['status']!=row['status']:errors.append(('status regression',key))
    if row['status']=='PASS' and result['status']!='PASS':errors.append(('new failure',key))
for key,row in new.items():
    if key not in old and row['status']!='PASS':errors.append(('unreviewed new failure',key))
known=[key for key,row in new.items() if row['status']!='PASS']
print(json.dumps({'new_regressions':errors,'remaining_known_failures':known},ensure_ascii=False,indent=2))
raise SystemExit(1 if errors else 0)
