"""Build gift/topic text highlighting only; no jade data or effects."""
from pathlib import Path
import sys
ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT.parent/'모드공통'))
from build_native import build

if __name__ == '__main__':
    build(ROOT, 'LOH_GiftHighlight', 'gift.c', ['test_rules'])
