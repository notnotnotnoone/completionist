import asyncio
import contextlib
import subprocess
import sys
import time
from uuid import uuid4

import pytest

from completionist_engine.client import EngineClient
from completionist_engine.config import Config
from completionist_engine.engine import Engine
from completionist_engine.probe import main as probe_main
from completionist_engine.server import start_server
from completionist_engine.vocabulary import load_wordfreq_vocabulary
from completionist_engine.words import WordCompleter

VOCAB = [("world", 5.8), ("work", 6.0), ("worry", 5.0)]


def small_engine() -> Engine:
    return Engine(WordCompleter(VOCAB), Config(block=frozenset(), allow=frozenset()))


def unique_pipe_name() -> str:
    return rf"\\.\pipe\completionist-test-{uuid4().hex}"


@contextlib.asynccontextmanager
async def serving(engine: Engine):
    name = unique_pipe_name()
    server = await start_server(engine, name)
    try:
        yield name
    finally:
        server.close()


def keystroke(request_id: int, before: str) -> dict:
    return {"id": request_id, "event": "keystroke", "app": "discord.exe", "before": before}


def test_keystroke_over_the_pipe_gets_a_word_reply():
    async def scenario():
        async with serving(small_engine()) as name:
            client = await EngineClient.connect(name)
            reply = await client.request(keystroke(1, "hello wor"))
            await client.close()
        return reply

    assert asyncio.run(scenario()) == {"id": 1, "type": "words", "replace": 3, "words": ["work", "world", "worry"]}


def test_one_connection_serves_many_requests_in_order():
    async def scenario():
        async with serving(small_engine()) as name:
            client = await EngineClient.connect(name)
            for i, text in enumerate(["w", "wo", "wor", "worl"]):
                await client.send(keystroke(i, text))
            replies = [await client.receive() for _ in range(4)]
            await client.close()
        return replies

    replies = asyncio.run(scenario())
    assert [r["id"] for r in replies] == [0, 1, 2, 3]
    assert replies[3]["words"] == ["world"]


def test_two_clients_are_served_independently():
    async def scenario():
        async with serving(small_engine()) as name:
            first = await EngineClient.connect(name)
            second = await EngineClient.connect(name)
            a = await first.request(keystroke(1, "wor"))
            b = await second.request(keystroke(2, "worr"))
            await first.close()
            await second.close()
        return a, b

    a, b = asyncio.run(scenario())
    assert (a["id"], b["id"]) == (1, 2)
    assert b["words"] == ["worry"]


def test_a_bad_frame_drops_only_that_client():
    async def scenario():
        async with serving(small_engine()) as name:
            bad = await EngineClient.connect(name)
            await bad.send_raw(b"\xff\xff\xff\xff")  # length far over the frame limit
            dropped = await bad.at_eof(timeout=2.0)
            good = await EngineClient.connect(name)
            reply = await good.request(keystroke(5, "wor"))
            await good.close()
        return dropped, reply

    dropped, reply = asyncio.run(scenario())
    assert dropped
    assert reply["id"] == 5


def test_events_without_a_reply_do_not_stall_the_connection():
    async def scenario():
        async with serving(small_engine()) as name:
            client = await EngineClient.connect(name)
            await client.send({"id": 1, "event": "dismiss"})
            reply = await client.request(keystroke(2, "wor"))
            await client.close()
        return reply

    assert asyncio.run(scenario())["id"] == 2


def test_round_trip_with_the_real_vocabulary_is_under_10ms_at_p95():
    engine = Engine(WordCompleter(load_wordfreq_vocabulary()), Config())
    sentence = "I think we should recommend this approach to everyone because it works"

    async def scenario():
        async with serving(engine) as name:
            client = await EngineClient.connect(name)
            timings = []
            for i in range(1, len(sentence) + 1):
                start = time.perf_counter()
                await client.request(keystroke(i, sentence[:i]))
                timings.append(time.perf_counter() - start)
            await client.close()
        return timings

    timings = sorted(asyncio.run(scenario()))
    p95 = timings[int(len(timings) * 0.95)]
    assert p95 < 0.010, f"p95 round trip took {p95 * 1000:.2f} ms"


def test_round_trip_with_the_fuzzy_index_on_is_under_10ms_at_p95(tmp_path):
    from completionist_engine.assemble import assemble_engine

    assembled = assemble_engine(Config(data_dir=tmp_path), load_wordfreq_vocabulary())
    sentence = "I think we should recommend this approach to everyone because it works"
    typos = ["moutian", "definately", "recieve"]

    async def scenario():
        async with serving(assembled.engine) as name:
            client = await EngineClient.connect(name)
            timings = []
            request_id = 0
            for text in [sentence, *typos]:
                for n in range(1, len(text) + 1):
                    request_id += 1
                    start = time.perf_counter()
                    await client.request(keystroke(request_id, text[:n]))
                    timings.append(time.perf_counter() - start)
            await client.close()
        return timings

    try:
        timings = sorted(asyncio.run(scenario()))
    finally:
        assembled.close()
    p95 = timings[int(len(timings) * 0.95)]
    assert p95 < 0.010, f"p95 round trip with fuzzy took {p95 * 1000:.2f} ms"


@pytest.fixture
def engine_process(tmp_path):
    name = unique_pipe_name()
    # Own data folder, so the test never reads or writes the real n-gram and personal files.
    config = tmp_path / "config.toml"
    data_dir = (tmp_path / "data").as_posix()
    config.write_text(f"[data]\ndir = '{data_dir}'\n", encoding="utf-8")
    process = subprocess.Popen([sys.executable, "-m", "completionist_engine", "--pipe", name, "--config", str(config), "--no-tray"])
    try:
        yield name
    finally:
        process.terminate()
        process.wait(timeout=10)


def test_engine_process_answers_the_probe(engine_process, capsys):
    exit_code = probe_main(["--pipe", engine_process, "--timeout", "20", "I'd like to recomm"])
    assert exit_code == 0
    assert capsys.readouterr().out.splitlines()[0] == "recommend"


def test_words_typed_over_one_connection_are_learned():
    from completionist_engine.personal import PersonalStore

    async def scenario():
        personal = PersonalStore()
        engine = Engine(WordCompleter(VOCAB, personal=personal), Config(block=frozenset(), allow=frozenset()), personal=personal)
        async with serving(engine) as name:
            client = await EngineClient.connect(name)
            for i, text in enumerate(["hey", "hey ", "hey l", "hey li", "hey lin", "hey linq", "hey linqi", "hey linqi "], start=1):
                await client.request(keystroke(i, text))
            await client.close()
        return personal

    personal = asyncio.run(scenario())
    assert dict(personal.counts((), "lin").words) == {"linqi": 1}
