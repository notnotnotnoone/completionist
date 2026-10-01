import io

from PIL import Image

from completionist_engine.screen_context import Foreground, ScreenContext


class FakeSource:
    def __init__(self) -> None:
        self.fg: Foreground | None = Foreground(hwnd=1, title="Inbox - Mail", app="chrome.exe")
        self.image: Image.Image | None = Image.new("RGB", (2000, 1000), "white")
        self.grabs = 0

    def foreground(self) -> Foreground | None:
        return self.fg

    def grab(self, hwnd: int) -> Image.Image | None:
        self.grabs += 1
        return self.image


class FakeReader:
    def __init__(self, text: str = "Sam: can you send the invoice?\nPriya: working on it") -> None:
        self.text = text
        self.reads = 0
        self.fail = False

    def read(self, image: Image.Image) -> str:
        self.reads += 1
        if self.fail:
            raise RuntimeError("ocr broke")
        return self.text


class Clock:
    def __init__(self) -> None:
        self.now = 1000.0

    def __call__(self) -> float:
        return self.now


def make(source=None, reader=None, wanted=lambda app: True, clock=None, **kw):
    source = source or FakeSource()
    reader = reader or FakeReader()
    clock = clock or Clock()
    return ScreenContext(source, reader, wanted=wanted, clock=clock, **kw), source, reader, clock


def settled(screen: ScreenContext) -> None:
    screen.poll()  # first sight of the window
    screen.poll()  # still there: it has settled, so it is read


def test_a_window_is_read_once_it_has_stayed_in_front_for_two_looks():
    screen, source, reader, _ = make()
    screen.poll()
    assert source.grabs == 0 and screen.for_request() is None
    screen.poll()
    assert source.grabs == 1 and reader.reads == 1
    shot = screen.for_request()
    assert shot is not None and "invoice" in shot.text and shot.app == "chrome.exe"


def test_a_window_already_read_is_not_read_again():
    screen, source, _, _ = make()
    settled(screen)
    for _ in range(5):
        screen.poll()
    assert source.grabs == 1


def test_the_capture_carries_a_small_jpeg_of_the_screenshot():
    screen, *_ = make()
    settled(screen)
    shot = screen.for_request()
    assert shot.jpeg.startswith(b"\xff\xd8")
    image = Image.open(io.BytesIO(shot.jpeg))
    assert image.width <= 1280 and image.height == round(1000 * image.width / 2000)


def test_text_from_another_window_is_never_handed_over():
    screen, source, _, _ = make()
    settled(screen)
    source.fg = Foreground(hwnd=2, title="Notes", app="notepad.exe")  # switched, not read yet
    assert screen.for_request() is None
    settled(screen)
    assert screen.for_request().app == "notepad.exe"


def test_a_new_title_in_the_same_window_is_read_again():  # a browser tab or a chat changed
    screen, source, _, _ = make()
    settled(screen)
    source.fg = Foreground(hwnd=1, title="Other chat", app="chrome.exe")
    assert screen.for_request() is None
    settled(screen)
    assert source.grabs == 2 and screen.for_request().app == "chrome.exe"


def test_nothing_is_read_for_an_app_that_is_not_wanted():
    screen, source, reader, _ = make(wanted=lambda app: app != "chrome.exe")
    settled(screen)
    assert source.grabs == 0 and reader.reads == 0 and screen.for_request() is None


def test_nothing_is_handed_over_once_reading_is_switched_off():
    on = [True]
    screen, *_ = make(wanted=lambda app: on[0])
    settled(screen)
    assert screen.for_request() is not None
    on[0] = False
    assert screen.for_request() is None


def test_no_window_in_front_or_a_window_that_cannot_be_captured_is_fine():
    screen, source, reader, _ = make()
    source.fg = None
    settled(screen)
    source.fg = Foreground(hwnd=3, title="Minimised", app="chrome.exe")
    source.image = None
    settled(screen)
    assert reader.reads == 0 and screen.for_request() is None


def test_a_reader_that_fails_does_not_stop_anything_and_is_not_retried_every_look():
    screen, source, reader, _ = make()
    reader.fail = True
    settled(screen)
    for _ in range(5):
        screen.poll()
    assert screen.for_request() is None and reader.reads == 1


def test_a_screen_with_no_text_is_still_recorded_so_the_log_can_show_it():
    screen, *_ = make(reader=FakeReader(""))
    settled(screen)
    shot = screen.for_request()
    assert shot is not None and shot.text == "" and shot.jpeg


def test_only_the_end_of_a_long_reading_is_kept_from_a_whole_line():
    lines = [f"line {i:03d} of the thread" for i in range(200)]
    screen, *_ = make(reader=FakeReader("\n".join(lines)), cap=300)
    settled(screen)
    text = screen.for_request().text
    assert len(text) <= 300 and text.endswith("line 199 of the thread")
    assert all(line in lines for line in text.split("\n"))  # no line cut in half


def test_a_reading_that_has_gone_stale_is_refreshed_in_the_background():
    screen, source, reader, clock = make(stale=30.0)
    settled(screen)
    reader.text = "a newer message arrived"
    clock.now += 31
    assert "invoice" in screen.for_request().text  # this request still gets the old one...
    screen.poll()  # ...and the next look reads the window again
    assert source.grabs == 2 and screen.for_request().text == "a newer message arrived"


def test_a_very_old_reading_is_not_used():
    screen, _, _, clock = make(max_age=600.0)
    settled(screen)
    clock.now += 601
    assert screen.for_request() is None


def test_each_capture_has_its_own_id():
    screen, source, *_ = make()
    settled(screen)
    first = screen.for_request().id
    source.fg = Foreground(hwnd=2, title="Notes", app="notepad.exe")
    settled(screen)
    assert screen.for_request().id != first


# --- when reading is allowed ---------------------------------------------------------------------

from types import SimpleNamespace  # noqa: E402

from completionist_engine.app import screen_wanted  # noqa: E402


def stubs(*, available=True, screen_context=True, paused=False, block=("code.exe",)):
    phrases = SimpleNamespace(available=available, config=SimpleNamespace(screen_context=screen_context))
    engine = SimpleNamespace(paused=paused, config=SimpleNamespace(block=frozenset(block)))
    return screen_wanted(engine, phrases)


def test_reading_is_allowed_only_when_phrases_are_on_and_the_setting_is_on():
    assert stubs()("chrome.exe") is True
    assert stubs(available=False)("chrome.exe") is False  # no key: nothing would be sent, so nothing is read
    assert stubs(screen_context=False)("chrome.exe") is False


def test_reading_stops_while_paused_and_in_apps_the_person_blocked():
    assert stubs(paused=True)("chrome.exe") is False
    assert stubs()("code.exe") is False and stubs()("CODE.EXE") is False


def test_reading_follows_the_live_settings_not_the_ones_at_start():
    phrases = SimpleNamespace(available=True, config=SimpleNamespace(screen_context=True))
    engine = SimpleNamespace(paused=False, config=SimpleNamespace(block=frozenset()))
    wanted = screen_wanted(engine, phrases)
    assert wanted("chrome.exe") is True
    phrases.config = SimpleNamespace(screen_context=False)  # a config reload swaps the object
    assert wanted("chrome.exe") is False
