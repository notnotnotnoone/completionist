from completionist_engine.screen_context import Foreground
from completionist_engine.accessible_context import AccessibleContext
from tests.test_screen_context import FakeReader, FakeSource, make, settled


def test_privacy_clear_forgets_capture_and_allows_fresh_capture():
    screen, *_ = make()
    settled(screen)
    assert screen.for_request() is not None
    screen.clear()
    assert screen.for_request() is None
    settled(screen)
    assert screen.for_request() is not None


def test_capture_finishing_after_permission_is_revoked_is_discarded():
    allowed = [True]
    class RevokingReader(FakeReader):
        def read(self, image):
            allowed[0] = False
            return "secret"
    screen, *_ = make(reader=RevokingReader(), wanted=lambda app: allowed[0])
    settled(screen)
    allowed[0] = True
    assert screen.for_request() is None


def test_accessible_context_reads_text_without_taking_a_screenshot():
    source = FakeSource()
    screen = AccessibleContext(source, lambda hwnd: "Nearby accessible text", wanted=lambda app: True)
    screen.poll()
    screen.poll()
    assert source.grabs == 0
    assert screen.for_request().text == "Nearby accessible text"
    assert screen.for_request().jpeg == b""
    source.fg = Foreground(2, "Elsewhere", "other.exe")
    assert screen.for_request() is None


def test_accessible_reader_failure_is_a_safe_empty_fallback():
    def broken(hwnd):
        raise RuntimeError("window vanished")
    screen = AccessibleContext(FakeSource(), broken, wanted=lambda app: True)
    screen.poll()
    screen.poll()
    assert screen.for_request() is None
