import sys
sys.path.insert(0,'tools')
import mantle_text as T
rows=[l.rstrip('\n').split('\t') for l in open(f'Saved/Mantles/{sys.argv[1]}/batch.tsv')][1:]
for r in rows:
    m=T.read(open(r[11]).read(), r[11])
    w={i.key[6:]:i.body.split('=',1)[1].strip() for i in m.items if i.kind=='symbol'}
    oath=(w.get('oath') or '').split(' | ')[0]; say=(w.get('saying') or '').split(' | ')[0]
    print(f'- {m.id} {m.title}: {m.persona.replace("#self.first# ","",1)} Oath "{oath}"; saying "{say}".')
