from datetime import date

import pytest

from typer_engine.budget import DailyBudget, Prices, Usage, cost

PRICES = Prices(input_per_m=0.30, cached_per_m=0.006, output_per_m=1.20)


def test_cost_prices_cached_uncached_and_output_tokens_separately():
    usage = Usage(cached=1_000_000, uncached=1_000_000, output=1_000_000)
    assert cost(usage, PRICES) == pytest.approx(0.006 + 0.30 + 1.20)


def test_a_request_with_a_warm_cache_is_far_cheaper():
    cold = cost(Usage(cached=0, uncached=2500, output=30), PRICES)
    warm = cost(Usage(cached=2400, uncached=100, output=30), PRICES)
    assert warm < cold / 5


def test_nothing_spent_means_not_exhausted():
    budget = DailyBudget(0.50, PRICES)
    assert not budget.exhausted
    assert budget.spent_today == 0


def test_spending_accumulates_and_the_cap_stops_further_requests():
    budget = DailyBudget(0.001, PRICES)
    assert budget.record(Usage(uncached=1000, output=100)) == pytest.approx(0.0003 + 0.00012)
    assert not budget.exhausted
    budget.record(Usage(uncached=3000, output=300))
    assert budget.exhausted
    assert budget.spent_today > 0.001


def test_the_budget_resets_the_next_day():
    day = [date(2026, 9, 29)]
    budget = DailyBudget(0.0001, PRICES, today=lambda: day[0])
    budget.record(Usage(uncached=1000))
    assert budget.exhausted
    day[0] = date(2026, 9, 30)
    assert not budget.exhausted
    assert budget.spent_today == 0


def test_cache_hit_rate_and_request_count_are_tracked():
    budget = DailyBudget(1.0, PRICES)
    budget.record(Usage(cached=900, uncached=100, output=10))
    budget.record(Usage(cached=100, uncached=900, output=10))
    assert budget.requests_today == 2
    assert budget.cache_hit_rate == pytest.approx(0.5)


def test_spend_survives_a_restart(tmp_path):
    path = tmp_path / "spend.json"
    first = DailyBudget(0.50, PRICES, path=path)
    first.record(Usage(uncached=10_000, output=500))
    spent = first.spent_today
    second = DailyBudget(0.50, PRICES, path=path)
    assert second.spent_today == pytest.approx(spent)
    assert second.requests_today == 1


def test_yesterdays_spend_is_not_loaded(tmp_path):
    path = tmp_path / "spend.json"
    DailyBudget(0.50, PRICES, path=path, today=lambda: date(2026, 9, 28)).record(Usage(uncached=10_000))
    assert DailyBudget(0.50, PRICES, path=path, today=lambda: date(2026, 9, 29)).spent_today == 0


def test_a_corrupt_spend_file_starts_fresh(tmp_path):
    path = tmp_path / "spend.json"
    path.write_text("{not json", encoding="utf-8")
    budget = DailyBudget(0.50, PRICES, path=path)
    assert budget.spent_today == 0
    budget.record(Usage(uncached=100))
    assert DailyBudget(0.50, PRICES, path=path).requests_today == 1


def test_a_zero_limit_disables_phrases_entirely():
    assert DailyBudget(0.0, PRICES).exhausted


def test_the_file_holds_only_totals():
    import json
    import tempfile
    from pathlib import Path

    with tempfile.TemporaryDirectory() as folder:
        path = Path(folder) / "spend.json"
        DailyBudget(0.5, PRICES, path=path).record(Usage(cached=1, uncached=2, output=3))
        assert set(json.loads(path.read_text())) == {"date", "spent", "requests", "cached", "uncached", "output"}
