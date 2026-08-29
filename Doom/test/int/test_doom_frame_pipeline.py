"""test_doom_frame_pipeline.py:

Integration tests for the DOOM frame pipeline: DoomEngine ->
FrameDownsampler -> FrameTlmProcessor. Run with the GDS pytest plugin
against a running ReferenceDeployment, e.g.:

    fprime-gds &
    pytest lib/fprime-stress/Doom/test/int --dictionary <dict.json>

The downsample factor is a compile-time configuration value
(Doom.DOWNSAMPLE_FACTOR in Doom/DoomConfig/DoomConfig.fpp); the tests
derive the configured dimensions from the received telemetry itself.
"""

import pytest

DOOM = "DoomSubtopology.doom"
TLM_PROC = "DoomSubtopology.frameTlmProcessor"

FULL_WIDTH = 640
FULL_HEIGHT = 400


def await_row(fprime_test_api, row, timeout=30):
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
        fprime_test_api.send_command(DOOM + ".Start")
        await_row(fprime_test_api, 0, timeout=60)
        _engine_started = True
    yield


def test_rows_flow_at_configured_factor(fprime_test_api):
    """Rows must flow at the configured width with full-depth coverage."""
    first = await_row(fprime_test_api, 0)
    width = int(first["width"])
    assert FULL_WIDTH % width == 0, "width {} does not divide 640".format(width)
    # The row payload is sized exactly to the configured width.
    assert len(first["pixels"]) == width
    factor = FULL_WIDTH // width
    height = FULL_HEIGHT // factor
    # The bottom row of the downsampled frame must be emitted...
    last = await_row(fprime_test_api, height - 1)
    assert int(last["width"]) == width
    assert int(last["row"]) == height - 1
    assert len(last["pixels"]) == width


def test_palette_flows(fprime_test_api):
    """The palette must be forwarded through the pipeline."""
    result = fprime_test_api.await_telemetry(TLM_PROC + ".PaletteOut", timeout=10)
    assert result is not None


def test_row_and_frame_numbers_consistent(fprime_test_api):
    """Row metadata must carry the scanline index it was emitted on."""
    row = await_row(fprime_test_api, 5)
    assert int(row["row"]) == 5
    assert int(row["frame"]) > 0
