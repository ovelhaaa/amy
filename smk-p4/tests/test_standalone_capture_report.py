import unittest
from standalone_capture_report import report


def fixture():
    lines=[]
    for i in range(361):
        t=i*5000
        cycles=min(i//60,4)
        lines += [
            f"I ({t}) P4_AMY: AUDIO I2S_PRODUCING frames_written={t*48} blocks_rendered={t*48//256} render_avg_us=200 render_max_us=2600 render_deadlines=0 service_max_us=5600 refill_risks=0 write_errors=0 timeouts=0 stack_free_bytes=7000 error=ESP_OK DMA_underruns=UNMEASURED\n",
            f"I ({t}) P4_AMY: ENGINE ready=1 patch=128 notes=0 clipped=0 oom=0 overflow=0 expired=0 latency_max_ms=6 compute_deadlines=0 cmd_avg/max=4/1200 dsp_avg/max=200/2600 dma_avg/max=5000/5300\n",
            f"I ({t}) P4_AMY: USB state=2 connects={1+cycles} disconnects={cycles} errors=0 events={i} parse_errors=0 ignored_cable_packets={i} dropped=0 overflow=0 stack_free=4000 polyphony_rejected=0\n",
            f"I ({t}) P4_AMY: UI_BLUEPRINT stack_free_bytes=4000 screen_switches={i*2}\n",
            f"I ({t}) DISPLAY_TEST: LVGL free=40000 largest=35000 peak_used=99000 fragmentation=12%\n",
            f"I ({t}) SMK_P4_GATE0: heap internal free=240000 min=239000 largest=170000; PSRAM total=33554432 free=31000000 min=31000000 largest=30000000\n",
            f"I ({t}) DISPLAY_TEST: diag_buttons=0 touch_errors=0 flush_errors=0 refresh={i*300}\n"]
    return ''.join(lines)


class CaptureReportTest(unittest.TestCase):
    def test_library_requires_actual_capacity_in_every_selected_patch(self):
        text=fixture()
        import re
        text=re.sub(r'patch=128 notes=0',lambda m:'patch=138 notes=8',text)
        self.assertFalse(report(text,voices=8)['serial_checks_pass'])
        missing=report(text,voices=8,patches=(128,135,138,142),require_full_patches=True)
        self.assertFalse(missing['checks']['full_capacity_each_patch'])
        lines=text.splitlines(keepends=True)
        for index,patch in enumerate((128,135,142)):
            pos=next(i for i,line in enumerate(lines) if f'I ({index*5000}) P4_AMY: ENGINE' in line)
            lines[pos]=lines[pos].replace('patch=138',f'patch={patch}')
        pos=max(i for i,line in enumerate(lines) if 'P4_AMY: ENGINE' in line)
        lines[pos]=lines[pos].replace('notes=8','notes=0')
        result=report(''.join(lines),voices=8,patches=(128,135,138,142),require_full_patches=True)
        self.assertTrue(result['serial_checks_pass'])
        self.assertTrue(all(result['patch_capacity_samples'].values()))

    def test_complete_window(self):
        result=report(fixture())
        self.assertTrue(result['serial_checks_pass'])
        self.assertEqual(result['midi_event_delta'],360)
        self.assertEqual(result['disconnect_delta'],4)
        self.assertEqual(result['frames_per_second_by_count'],48000)
        self.assertIn('analog',result['limits'][0])

    def test_historical_counters_are_not_new_actions(self):
        text=fixture()
        import re
        text=re.sub(r'events=\d+','events=100',text)
        text=re.sub(r'connects=\d+','connects=5',text)
        text=re.sub(r'disconnects=\d+','disconnects=4',text)
        text=re.sub(r'screen_switches=\d+','screen_switches=50',text)
        result=report(text)
        for key in ('new_midi_events','hotplugs_in_window','ui_transitions_observed'):
            self.assertFalse(result['checks'][key])
        self.assertFalse(result['serial_checks_pass'])

    def test_reboot_clock_counter_reset_and_gaps(self):
        for text in (
            fixture().replace('I (10000) P4_AMY: AUDIO','I (1) P4_AMY: AUDIO',1),
            fixture().replace('frames_written=480000','frames_written=1',1),
            fixture().replace('I (10000) P4_AMY: AUDIO','ESP-ROM:esp32p4\nI (10000) P4_AMY: AUDIO',1),
            ''.join(line for line in fixture().splitlines(keepends=True) if not any(f'I ({t})' in line for t in (5000,10000))),
        ):
            self.assertFalse(report(text)['serial_checks_pass'])

    def test_missing_or_invalid_fields_fail_closed(self):
        for text in ('',fixture().replace('render_deadlines=0 ','',1),fixture().replace('write_errors=0','write_errors=BAD',1),fixture().replace('dsp_avg/max=200/2600','dsp_avg/max=BAD',1)):
            self.assertFalse(report(text)['serial_checks_pass'])

    def test_faults_ram_mode_and_short_run_rejected(self):
        for text in (fixture().replace('I2S_PRODUCING','RAM_ONLY'),fixture().replace('compute_deadlines=0','compute_deadlines=1',1),fixture().replace('render_max_us=2600','render_max_us=5334',1),fixture().replace('parse_errors=0','parse_errors=1',1),fixture()[:2000]):
            self.assertFalse(report(text)['serial_checks_pass'])

    def test_ansi_partial_tail_and_diagnostic_drops(self):
        text=fixture().replace('I (','\x1b[32mI (').replace('\n','\x1b[0m\n')
        text += 'I (1800000) P4_AMY: MIDI_TRACE trace_dropped=6000\nI (1805000) P4_AMY: AUDIO incomplete'
        self.assertTrue(report(text)['serial_checks_pass'])

    def test_boot_fragment_does_not_supply_another_records_timestamp(self):
        text=fixture().replace('I (0) DISPLAY_TEST:', 'I (171) esp_image: partial I (0) DISPLAY_TEST:',1)
        text=text.replace('I (0) P4_AMY: AUDIO','I (172) esp_image: partial I (0) P4_AMY: AUDIO',1)
        self.assertTrue(report(text)['serial_checks_pass'])

    def test_same_tag_fragment_cannot_supply_later_display_timestamp(self):
        for payload in ('LVGL free=40000', 'diag_buttons=0 touch_errors=0 flush_err'):
            text=fixture().replace('I (0) DISPLAY_TEST:',
                'I (999999) DISPLAY_TEST: '+payload+'I (0) DISPLAY_TEST:',1)
            result=report(text)
            self.assertTrue(result['serial_checks_pass'])
            self.assertEqual(result['span_seconds']['LVGL'],1800)
            self.assertEqual(result['max_gap_seconds']['DISPLAY'],5)
        # Keep a complete error record even when another record follows it.
        text=fixture().replace('I (0) DISPLAY_TEST:',
            'I (0) DISPLAY_TEST: diag_buttons=0 touch_errors=0 flush_errors=1 refresh=0 I (0) DISPLAY_TEST:',1)
        self.assertFalse(report(text)['checks']['DISPLAY_errors_zero'])

    def test_capacity_is_selected_without_accepting_extra_notes(self):
        six=fixture().replace('notes=0','notes=6')
        self.assertFalse(report(six)['serial_checks_pass'])
        self.assertTrue(report(six,voices=6)['serial_checks_pass'])
        self.assertFalse(report(six.replace('notes=6','notes=7'),voices=6)['serial_checks_pass'])
        eight=six.replace('notes=6','notes=8')
        self.assertTrue(report(eight,voices=8)['serial_checks_pass'])
        self.assertFalse(report(eight.replace('notes=8','notes=9'),voices=8)['serial_checks_pass'])
        with self.assertRaises(ValueError):
            report(six,voices=7)

    def test_legacy_has_no_blueprint_stack_requirement(self):
        text=''.join(line for line in fixture().splitlines(keepends=True) if 'UI_BLUEPRINT' not in line)
        self.assertFalse(report(text)['serial_checks_pass'])
        self.assertTrue(report(text,blueprint=False)['serial_checks_pass'])


if __name__=='__main__':
    unittest.main()
