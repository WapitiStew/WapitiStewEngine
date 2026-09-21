"""Keep the independently compiled C/C++ and managed ABI snapshots exhaustive."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]


def uncomment(text):
    return re.sub(r'//[^\n]*|/\*.*?\*/', '', text, flags=re.S)


class CapiLayoutContract(unittest.TestCase):
    def test_all_public_fields_are_snapshotted(self):
        headers = uncomment('\n'.join(p.read_text(encoding='utf-8')
            for p in (ROOT / 'api/wse/capi').glob('*.h')))
        actual = {}
        for name, body in re.findall(r'typedef struct (\w+)\s*\{(.*?)\}\s*\w+;', headers, re.S):
            actual[name] = re.findall(r'\b(\w+)\s*(?:\[[^]]+\])?\s*;', body)
        snapshot = (ROOT / 'test/support/CapiLayout.def').read_text(encoding='utf-8')
        expected = {name: [] for name in re.findall(r'CAPI_TYPE\((\w+),', snapshot)}
        for name, field in re.findall(r'CAPI_FIELD\((\w+),\s*(\w+),', snapshot):
            expected[name].append(field)
        self.assertEqual(actual, expected)

    def test_opaque_managed_inputs_retain_owners(self):
        headers = uncomment('\n'.join(p.read_text(encoding='utf-8')
            for p in (ROOT / 'api/wse/capi').glob('*.h')))
        owners = set(re.findall(r'typedef struct \w+\s*\*\s*(\w+);', headers))
        exports = dict(re.findall(r'WSE_CAPI_CALL\s+(\w+)\s*\((.*?)\)\s*;', headers, re.S))
        managed = (ROOT / 'lang/cs/Wse/NativeMethods.cs').read_text(encoding='utf-8')
        # Optional cancellation uses an explicit lease; the list is a private local
        # owner in Enumerate's try/finally and cannot be disposed by another caller.
        exempt = {('wse_capi_runtime_wait', 2), ('wse_capi_camera_device_list_count', 0),
                  ('wse_capi_camera_device_list_at', 0)}
        checked = 0
        for name, signature in re.findall(r'internal static extern \w+\s+(\w+)\((.*?)\);', managed, re.S):
            if name not in exports or name.endswith('_destroy'):
                continue
            cargs, args = exports[name].split(','), signature.split(',')
            self.assertEqual(len(cargs), len(args), name)
            for index, arg in enumerate(cargs):
                words = arg.strip().split()
                if words[0] in owners and '*' not in arg and (name, index) not in exempt:
                    self.assertRegex(args[index], r'\bSafeHandle\b', name)
                    checked += 1
        self.assertGreaterEqual(checked, 96)


if __name__ == '__main__':
    unittest.main()
