"""test_doom_frame_pipeline.py:

Integration tests for the DOOM frame pipeline: DoomEngine ->
FrameDownsampler -> FrameTlmProcessor. Run with the GDS pytest plugin
against a running ReferenceDeployment, e.g.:

    fprime-gds &
    pytest lib/fprime-stress/Doom/test/int --dictionary <dict.json>
"""

import time

import pytest

DOOM = "DoomSubtopology.doom"
DOWNSAMPLER = "DoomSubtopology.frameDownsampler"
TLM_PROC = "DoomSubtopology.frameTlmProcessor"

FULL_WIDTH = 640
FULL_HEIGHT = 400

# X1/X2 row volume outpaces the ground-side ingest rate, so the e2e suite
# exercises the pipeline at X8/X16; low factors are covered by unit tests.
BASE_LABEL = "X8"
BASE_FACTOR = 8


def set_downsample(fprime_test_api, factor_label, factor):
    """Set the DOWNSAMPLE parameter and confirm the row width rescales.

    Command completion events can be dropped under full row-telemetry load,
    so the parameter change is confirmed via the emitted row width instead.
    """
    fprime_test_api.send_command(DOWNSAMPLER + ".DOWNSAMPLE_PRM_SET", [factor_label])
    expected = FULL_WIDTH // factor
    # Downlink can lag well behind real time under full row load; allow
    # generous settling time for the new width to reach the ground.
    deadline = time.time() + 300
    width = None
    while time.time() < deadline:
        width = int(await_row(fprime_test_api, 0)["width"])
        if width == expected:
            return
        time.sleep(0.5)
    assert width == expected, "row width did not rescale to {}".format(expected)


def await_row(fprime_test_api, row, timeout=10):
    """Await a fresh FrameRow sample for the given scanline index."""
    channel = "{}.FrameRow{:03d}".format(TLM_PROC, row)
    result = fprime_test_api.await_telemetry(channel, timeout=timeout)
    assert result is not None, "no {} sample within {}s".format(channel, timeout)
    return result.get_val()


_engine_started = False


@pytest.fixture(autouse=True)
def doom_running(fprime_test_api):
    """Ensure the engine is running before the first test of the run.

    Start is idempotent from the test's perspective (AlreadyRunning if a
    prior run left it going), and completion events can be dropped under
    full row-telemetry load, so running is confirmed via row telemetry.
    """
    global _engine_started
    if not _engine_started:
        # Set the factor before Start so the default X2 volume never floods
        # the ground-side ingest queues.
        fprime_test_api.send_command(
            DOWNSAMPLER + ".DOWNSAMPLE_PRM_SET", [BASE_LABEL]
        )
        fprime_test_api.send_command(DOOM + ".Start")
        set_downsample(fprime_test_api, BASE_LABEL, BASE_FACTOR)
        _engine_started = True
    yield


def test_rows_flow_at_base_factor(fprime_test_api):
    """Rows must flow with a consistent width and full-depth coverage."""
    first = await_row(fprime_test_api, 0)
    width = int(first["width"])
    assert width == FULL_WIDTH // BASE_FACTOR
    height = FULL_HEIGHT * width // FULL_WIDTH
    # The bottom row of the downsampled frame must be emitted...
    last = await_row(fprime_test_api, height - 1)
    assert int(last["width"]) == width
    assert int(last["row"]) == height - 1
    assert len(last["pixels"]) == FULL_WIDTH


def test_factor_change_scales_width(fprime_test_api):
    """Changing DOWNSAMPLE at runtime must rescale the emitted rows."""
    set_downsample(fprime_test_api, "X16", 16)
    row = await_row(fprime_test_api, 0)
    assert int(row["width"]) == FULL_WIDTH // 16
    set_downsample(fprime_test_api, BASE_LABEL, BASE_FACTOR)


def test_palette_flows(fprime_test_api):
    """The palette must be forwarded through the pipeline."""
    result = fprime_test_api.await_telemetry(TLM_PROC + ".PaletteOut", timeout=10)
    assert result is not None


def test_row_and_frame_numbers_consistent(fprime_test_api):
    """Row metadata must carry the scanline index it was emitted on."""
    row = await_row(fprime_test_api, 5)
    assert int(row["row"]) == 5
    assert int(row["frame"]) > 0
