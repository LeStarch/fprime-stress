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


def set_downsample(fprime_test_api, factor_label):
    """Set the DOWNSAMPLE parameter and confirm command completion."""
    fprime_test_api.send_and_assert_command(
        DOWNSAMPLER + ".DOWNSAMPLE_PRM_SET", [factor_label], max_delay=2
    )


def await_row(fprime_test_api, row, timeout=10):
    """Await a fresh FrameRow sample for the given scanline index."""
    channel = "{}.FrameRow{:03d}".format(TLM_PROC, row)
    result = fprime_test_api.await_telemetry(channel, timeout=timeout)
    assert result is not None, "no {} sample within {}s".format(channel, timeout)
    return result.get_val()


@pytest.fixture(scope="module", autouse=True)
def doom_running(fprime_test_api):
    """Start the engine once for this module and stop it afterwards."""
    fprime_test_api.send_and_assert_command(DOOM + ".Start", max_delay=30)
    yield
    fprime_test_api.send_command(DOOM + ".Stop")


def test_rows_flow_at_default_factor(fprime_test_api):
    """Rows must flow with a consistent width and full-depth coverage."""
    set_downsample(fprime_test_api, "X2")
    time.sleep(1)  # let a full frame at the new factor flush through
    first = await_row(fprime_test_api, 0)
    width = int(first["width"])
    assert width == FULL_WIDTH // 2
    height = FULL_HEIGHT * width // FULL_WIDTH
    # The bottom row of the downsampled frame must be emitted...
    last = await_row(fprime_test_api, height - 1)
    assert int(last["width"]) == width
    assert int(last["row"]) == height - 1
    assert len(last["pixels"]) == FULL_WIDTH


def test_factor_change_scales_width(fprime_test_api):
    """Changing DOWNSAMPLE at runtime must rescale the emitted rows."""
    set_downsample(fprime_test_api, "X8")
    time.sleep(1)
    row = await_row(fprime_test_api, 0)
    assert int(row["width"]) == FULL_WIDTH // 8
    set_downsample(fprime_test_api, "X2")


def test_palette_flows(fprime_test_api):
    """The palette must be forwarded through the pipeline."""
    result = fprime_test_api.await_telemetry(TLM_PROC + ".PaletteOut", timeout=10)
    assert result is not None


def test_row_and_frame_numbers_consistent(fprime_test_api):
    """Row metadata must carry the scanline index it was emitted on."""
    row = await_row(fprime_test_api, 5)
    assert int(row["row"]) == 5
    assert int(row["frame"]) > 0
