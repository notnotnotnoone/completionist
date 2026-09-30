# Adds the Typer keyboard to your English language(s) (or with -Remove, takes it away).
# This changes your Windows language settings, so you run it yourself. After adding it, switch to it
# with Win+Space (the language indicator in the taskbar shows "Typer"). Typer never changes what keys
# type; it only draws suggestions.
param([switch]$Remove)

$clsid = '{71B17AFC-9D1A-4E42-A7C3-2F4151AC6ABF}'
$profile = '{61BEEED0-FFE4-4E6C-AEC1-9333F155674B}'
$langIds = @{ 'en-US' = '0409'; 'en-CA' = '1009'; 'en-GB' = '0809' }  # the languages the DLL registers for

$list = Get-WinUserLanguageList
$changed = @()
foreach ($language in $list) {
    if (-not $langIds.ContainsKey($language.LanguageTag)) { continue }
    $tip = '{0}:{1}{2}' -f $langIds[$language.LanguageTag], $clsid, $profile
    if ($Remove) {
        if ($language.InputMethodTips.Remove($tip)) { $changed += $language.LanguageTag }
    } elseif (-not $language.InputMethodTips.Contains($tip)) {
        $language.InputMethodTips.Add($tip)
        $changed += $language.LanguageTag
    }
}
if (-not $changed) {
    if ($Remove) { 'Typer was not enabled for any language.' }
    else { 'Nothing to do: no en-US, en-CA or en-GB language found, or Typer is already enabled.' }
    return
}
Set-WinUserLanguageList $list -Force
$verb = if ($Remove) { 'removed from' } else { 'added to' }
"Typer keyboard $verb $($changed -join ', '). Switch keyboards with Win+Space."
