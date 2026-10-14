import importlib.util
import os
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def load_launcher():
    spec = importlib.util.spec_from_file_location('bblauncher', ROOT / 'launcher.py')
    module = importlib.util.module_from_spec(spec)
    sys.modules['bblauncher'] = module
    spec.loader.exec_module(module)
    return module


L = load_launcher()


def setUpModule():
    os.environ.pop('BB_FSR411_DIR', None)  # the check honours it, tests must not inherit one


class UpscalerAvailabilityTests(unittest.TestCase):
    def test_built_in_upscalers_never_report_a_problem(self):
        for engine in ('', 'off', 'taa', 'fsr3'):
            self.assertIsNone(L.upscaler_problem(engine, {'fsr4': False, 'fsr411': False}))

    def test_missing_valve_extension_is_reported_for_fsr411(self):
        problem = L.upscaler_problem('fsr411', {'fsr4': True, 'fsr411': False})
        self.assertIn('VK_VALVE_shader_mixed_float_dot_product', problem)

    def test_unsupported_device_is_reported_for_fsr4(self):
        self.assertIn('FSR 4 INT8', L.upscaler_problem('fsr4', {'fsr4': False}))

    def test_unknown_capabilities_are_not_reported_as_unsupported(self):
        with tempfile.TemporaryDirectory() as tmp:
            # No probe result: the assets are the only thing we may complain about.
            problem = L.upscaler_problem('fsr4', {}, app_dir=Path(tmp))
            self.assertIn('fsr4_shaders', problem)

    def test_assets_are_checked_once_the_device_can_run_them(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            self.assertIn('fsr4_411', L.upscaler_problem('fsr411', {'fsr411': True}, app_dir=root))
            (root / 'fsr4_411').mkdir()
            # Output size and preset pick the model: 4K + Ultra Performance is t2160_m1.
            problem = L.upscaler_problem('fsr411', {'fsr411': True}, '3840x2160', '4', app_dir=root)
            self.assertIn('t2160_m1', problem)
            problem = L.upscaler_problem('fsr411', {'fsr411': True}, '1920x1080', '2', app_dir=root)
            self.assertIn('t1080_m0', problem)
            (root / 'fsr4_shaders').mkdir()
            self.assertIsNone(L.upscaler_problem('fsr4', {'fsr4': True}, app_dir=root))


class UpscalerChoiceTests(unittest.TestCase):
    def test_labels_map_to_the_engine_names(self):
        self.assertEqual('', L.upscaler_engine(L.UPSCALER_FROM_MENU))
        self.assertEqual('off', L.upscaler_engine('Off (native)'))
        self.assertEqual('fsr411', L.upscaler_engine('FSR 4.1.1'))
        self.assertEqual('', L.upscaler_engine('not a choice'))
        self.assertEqual('4', L.preset_value('Ultra Performance (x3, lite model)'))
        self.assertEqual('', L.preset_value(L.PRESET_FROM_MENU))

    def test_choices_only_use_names_the_engine_knows(self):
        self.assertEqual({'', 'off', 'taa', 'fsr3', 'fsr4', 'fsr411'},
                         {choice[1] for choice in L.UPSCALER_CHOICES})
        self.assertEqual({'', '0', '1', '2', '3', '4'},
                         {choice[1] for choice in L.PRESET_CHOICES})


class IniReaderTests(unittest.TestCase):
    def test_comments_and_blank_lines_are_ignored(self):
        with tempfile.TemporaryDirectory() as tmp:
            ini = Path(tmp) / 'bbport.ini'
            ini.write_text('# comment\n\noutput_res=3840x2160\npreset=2\nbroken\n', encoding='utf-8')
            values = L.read_bbport_ini(ini)
            self.assertEqual({'output_res': '3840x2160', 'preset': '2'}, values)

    def test_a_missing_file_is_empty(self):
        self.assertEqual({}, L.read_bbport_ini(Path(tempfile.gettempdir()) / 'no-such-bbport.ini'))


if __name__ == '__main__':
    unittest.main()
