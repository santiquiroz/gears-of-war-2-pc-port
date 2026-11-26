import importlib.util
from pathlib import Path
import struct
import pytest
spec = importlib.util.spec_from_file_location('checkpoint_message', Path(__file__).with_name('checkpoint_message.py'))
m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
def string(s, wide=False):
    raw = (s + '\0').encode('utf-16-le' if wide else 'latin-1')
    return struct.pack('>i', -(len(raw)//2) if wide else len(raw)) + raw
def bundle(value=m.OLD, wide=False, count=1):
    entries = string('Other') + string('Unrelated text', wide)
    for _ in range(count): entries += string('SavingContent_Message') + string(value, wide)
    return (struct.pack('>i',1) + string('..\\GearGame\\Localization\\INT\\GearGameUI.int') +
            struct.pack('>i',1) + string('UpdatingStrings') + struct.pack('>i',count+1) + entries + bytes(12))
def test_ascii():
    b = bundle(); out = m.patch(b)
    assert len(out) == len(b) and m.records(out)[1][3].rstrip() == m.NEW
    assert m.records(out)[0] == m.records(b)[0]
def test_unicode():
    b = bundle(wide=True); out = m.patch(b)
    assert len(out) == len(b) and m.records(out)[1][3].rstrip() == m.NEW
    assert m.records(out)[1][-1] < 0
def test_idempotent():
    b = m.patch(bundle()); assert m.patch(b) == b
def test_unknown_revision():
    with pytest.raises(ValueError): m.patch(bundle('Another message'))
def test_missing():
    with pytest.raises(ValueError): m.patch(bundle(count=0))
def test_ambiguous():
    with pytest.raises(ValueError): m.patch(bundle(count=2))
def test_truncated():
    with pytest.raises(ValueError): m.patch(bundle()[:20])
