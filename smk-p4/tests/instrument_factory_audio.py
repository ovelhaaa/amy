"""Host-only observer inserted into a build copy; shared AMY stays untouched."""
from pathlib import Path
table=Path('src/clipping_lookup_table.h').read_text()
assert '#define FIRST_NONLIN 29491' in table and '#define NONLIN_RANGE 4914' in table, 'Update observer thresholds'
src=Path('src/amy.c').read_text()
needle='            // soft clipping.\n'
assert src.count(needle)==1
src=src.replace(needle,'            extern void factory_sample_observe(int32_t);\n            factory_sample_observe(S2L(fsample));\n'+needle)
Path('build/factory010/amy_observed.c').write_text(src)
