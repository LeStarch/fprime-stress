"""test_doom_frame_pipeline.py:

Integration tests for the DOOM frame pipeline: DoomEngine ->
FrameDownsampler -> FrameTlmProcessor. Run with the GDS pytest plugin
against a running ReferenceDeployment, e.g.:

    fprime-gds &
    pytest lib/fprime-stress/Doom/test/int --dictionary <dict.json>

The downsample factor is a compile-time configuration value
(Doom.DOWNSAMPLE_FACTOR in Doom/DoomConfig/DoomConfig.fpp); the tests
read the configured width from the dictionary (FrameRow pixel array
length) and derive the height from the fixed 640x400 native frame,
never from the sample under test. FrameRow000..399 channels exist for
every factor; only the first height of them are emitted.
"""

import re

import pytest

DOOM = "DoomSubtopology.doom"
TLM_PROC = "DoomSubtopology.frameTlmProcessor"

FULL_WIDTH = 640
FULL_HEIGHT = 400
PALETTE_ENTRIES = 256
PALETTE_BYTES = 3 * PALETTE_ENTRIES


def configured_frame_size(fprime_test_api):
    """Return (width, height) implied by the dictionary's FrameRow type."""
    ch_dict = fprime_test_api.pipeline.dictionaries.channel_name
    row_pattern = re.compile(re.escape(TLM_PROC) + r"\.FrameRow(\d{3})$")
    rows = sorted(int(m.group(1)) for m in map(row_pattern.match, ch_dict) if m)
    assert rows == list(range(FULL_HEIGHT)), "FrameRow channels are not FrameRow000..399"
    members = dict((name, typ) for name, typ, _, _ in ch_dict[TLM_PROC + ".FrameRow000"].get_type_obj().MEMBER_LIST)
    width = members["pixels"].LENGTH
    assert FULL_WIDTH % width == 0, "width {} does not divide 640".format(width)
    factor = FULL_WIDTH // width
    assert factor in (1, 2, 4, 8, 16), "unsupported factor {}".format(factor)
    return width, FULL_HEIGHT // factor


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

    Start is idempotent from the test's perspective (StartRejected if a
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
    """Rows must match the dictionary-declared factor, top to bottom."""
    width, height = configured_frame_size(fprime_test_api)

    first = await_row(fprime_test_api, 0)
    assert int(first["width"]) == width
    assert int(first["row"]) == 0
    assert len(first["pixels"]) == width
    last = await_row(fprime_test_api, height - 1)
    assert int(last["width"]) == width
    assert int(last["row"]) == height - 1
    assert len(last["pixels"]) == width


def test_palette_flows(fprime_test_api):
    """The palette must arrive well-formed and carry real colour data."""
    result = fprime_test_api.await_telemetry(TLM_PROC + ".PaletteOut", timeout=10)
    assert result is not None
    palette = result.get_val()
    assert int(palette["generation"]) > 0
    rgb = [int(b) for b in palette["rgb"]]
    assert len(rgb) == PALETTE_BYTES
    assert all(0 <= b <= 255 for b in rgb)
    # PLAYPAL is never flat: a stuck or zeroed palette has one distinct colour.
    colours = set(tuple(rgb[i : i + 3]) for i in range(0, PALETTE_BYTES, 3))
    assert len(colours) > 16, "palette has only {} distinct colours".format(len(colours))


def test_row_and_frame_numbers_consistent(fprime_test_api):
    """Row metadata must carry the scanline index it was emitted on."""
    row = await_row(fprime_test_api, 5)
    assert int(row["row"]) == 5
    assert int(row["frame"]) > 0
