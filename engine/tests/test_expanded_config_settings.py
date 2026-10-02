import asyncio
import json
import pytest
from completionist_engine import config as cfg
from completionist_engine.config import ConfigError, load_config
from completionist_engine.settings import apply_settings, read_settings
from completionist_engine.phrase_provider import PhraseProvider, PhraseRequest, ProviderSettings
from tests.fake_provider import fake_provider

CHANGES = {
    'words': {'typo_correction': False},
    'popup': {'font_size': 12, 'width_scale': 1.5},
    'hotkeys': {'partial_accept': 'alt+right', 'dismiss': 'ctrl+backspace'},
    'privacy': {'private_mode': True, 'pause_minutes': 10},
    'apps': {'profiles': [{'app': 'NOTEPAD.EXE', 'phrase_mode': 'hotkey', 'completion_length': 'short', 'learning': False, 'word_limit': 8}]},
    'phrase': {'provider_sort': 'latency', 'allow_fallbacks': False, 'provider_ignore': ['Slow'], 'max_price_input': .2, 'max_price_output': .4, 'require_parameters': True, 'zdr': True,
        'mode': 'auto', 'min_chars': 8, 'trigger': 'sentence', 'dismiss_cooldown': 2, 'failure_limit': 5, 'failure_pause': 60,
        'completion_length': 'long', 'writing_style': 'professional', 'spelling': 'canadian', 'preserve_casing': False, 'multiline': True,
        'avoid_phrases': ['basically'], 'context_source': 'caret', 'context_apps': ['Notepad.exe'], 'preview_context': True, 'log_mode': 'timings', 'log_retention_minutes': 5}}

def test_expanded_settings_round_trip_and_profiles(tmp_path):
    path = tmp_path / 'config.toml'
    apply_settings(path, CHANGES)
    read = read_settings(path)
    for section, fields in CHANGES.items():
        for key, value in fields.items():
            if key in ('profiles', 'context_apps'): continue
            assert read[section][key] == value
    loaded = load_config(path)
    assert loaded.phrase.context_apps == frozenset({'notepad.exe'})
    effective = cfg.effective_config(loaded, 'Notepad.EXE')
    assert effective.word_limit == 8 and effective.learning is False
    assert effective.phrase.mode == 'hotkey' and effective.phrase.completion_length == 'short'
    assert effective.private_mode is True and effective.block == loaded.block
    assert cfg.effective_config(loaded, 'other.exe') == loaded

@pytest.mark.parametrize('section,key,value', [
    ('popup','font_size',6), ('popup','font_size',25), ('popup','width_scale',float('inf')),
    ('phrase','mode','bad'), ('hotkeys','dismiss','tab'), ('phrase','timeout',float('nan')),
    ('phrase','min_chars',True), ('phrase','max_price_input',-1), ('privacy','pause_minutes',True),
    ('apps','profiles',[{'app':'a.exe','word_limit':21}]), ('apps','profiles',[{'app':'a.exe','learning':1}]),
    ('apps','profiles',[{'app':'a.exe','unknown':True}]), ('apps','profiles',[{'app':'a.exe'},{'app':'A.exe'}])])
def test_invalid_changes_leave_file_untouched(tmp_path, section,key,value):
    path = tmp_path / 'config.toml'; path.write_text('# retained\n[words]\nlimit = 5\n')
    original = path.read_bytes()
    with pytest.raises(ConfigError): apply_settings(path, {section:{key:value}})
    assert path.read_bytes() == original

def test_profile_edit_handles_escaped_quotes_and_keeps_other_comments(tmp_path):
    path = tmp_path / 'config.toml'
    path.write_text('[apps]\n# keep this\nprofiles = [\n { app = "a\\\"[#.exe", spelling = "american" },\n]\nallow = ["notepad.exe"] # still here\n')
    apply_settings(path, {'apps': {'profiles': [{'app': 'b.exe', 'spelling': 'british'}]}})
    assert load_config(path).app_profiles[0].app == 'b.exe'
    assert '# keep this' in path.read_text() and '# still here' in path.read_text()

def test_key_is_write_only_with_expanded_settings(tmp_path):
    path = tmp_path / 'config.toml'
    apply_settings(path, {'phrase': {'api_key': 'test-secret', 'zdr': True}})
    assert 'test-secret' not in json.dumps(read_settings(path))

@pytest.mark.parametrize('key', ['timeout','temperature','debounce_ms'])
def test_config_loader_rejects_nonfinite_numbers(tmp_path,key):
    path = tmp_path / 'config.toml'; path.write_text(f'[phrase]\n{key} = inf\n')
    with pytest.raises(ConfigError): load_config(path)

def test_explicit_provider_sort_replaces_order_and_sends_preferences():
    async def scenario():
        async with fake_provider() as (url, received):
            s = ProviderSettings(base_url=url,provider_order=('Groq',),provider_sort='price',allow_fallbacks=False,
                provider_ignore=('Slow',),max_price_input=.2,max_price_output=.4,require_parameters=True,zdr=True)
            provider = PhraseProvider(s, 'key')
            try:
                _ = [event async for event in provider.stream(PhraseRequest('Hi'))]
                return received[0].body
            finally: await provider.aclose()
    assert asyncio.run(scenario())['provider'] == {'sort':'price','allow_fallbacks':False,'ignore':['Slow'],
        'max_price':{'input':.2,'output':.4},'require_parameters':True,'zdr':True}

@pytest.mark.parametrize('section,key,value', [
    ('phrase', 'debounce', 1e308), ('phrase', 'timeout', 10 ** 400),
    ('apps', 'profiles', [{'app': '.exe'}]), ('apps', 'profiles', [{'app': 'bad\n.exe'}]),
    ('apps', 'profiles', [{'app': 'C:/app.exe'}]),
])
def test_extreme_numbers_and_invalid_process_names_are_validation_errors(tmp_path, section, key, value):
    path = tmp_path / 'config.toml'
    path.write_text('[words]\nlimit = 5\n')
    original = path.read_bytes()
    with pytest.raises(ConfigError):
        apply_settings(path, {section: {key: value}})
    assert path.read_bytes() == original

def test_inline_comment_detection_does_not_copy_hash_inside_string(tmp_path):
    path = tmp_path / 'config.toml'
    path.write_text('[phrase]\ninstructions = "say \\\" # text"  # keep this comment\n')
    apply_settings(path, {'phrase': {'instructions': 'revised'}})
    assert load_config(path).phrase.instructions == 'revised'
    assert path.read_text().endswith('instructions = "revised"  # keep this comment\n')

def test_full_profile_inherits_unspecified_settings_and_keeps_protections(tmp_path):
    path = tmp_path / 'config.toml'
    apply_settings(path, {'apps': {'block': ['app.exe'], 'profiles': [{'app': 'app.exe',
        'writing_style': 'casual', 'spelling': 'british', 'context_source': 'accessible'}]}})
    effective = cfg.effective_config(load_config(path), 'APP.EXE')
    assert effective.block == frozenset({'app.exe'})
    assert effective.phrase.writing_style == 'casual'
    assert effective.phrase.spelling == 'british'
    assert effective.phrase.context_source == 'accessible'
    assert effective.phrase.mode == 'apps' and effective.word_limit == 5 and effective.learning is True
