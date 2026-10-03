# V2 Completionist viewer redesign brief

Status: proposed implementation brief for the viewer track. This is a presentation and interaction redesign of the existing local viewer; it adds no endpoints, settings, persistence, or frontend framework.

## Recommendation

Keep the four existing destinations—Words, Stats, Requests, and Settings—as the primary navigation, with Phrases retained as the closely related learned-data view. On wide screens, use a compact fixed-width content column and a clear horizontal section bar under the Evergreen brand header. On narrow screens, keep the same destinations in a horizontally scrollable, keyboard-operable tab strip with a visible active item and clear focus; keep the current 44px minimum targets and safe-area padding. Avoid a new dashboard home or a collapsible hamburger menu: the former duplicates summary data, while the latter hides the viewer's main destinations and adds state without improving this five-view app.

Give each page one clear first task and a short explanation, followed by the relevant controls and content. Words and Phrases lead with search, sort and a count; the count stays close to the search and makes the loaded-versus-total state clear. Stats lead with the date range and a small set of meaningful totals, followed by daily activity, app breakdown and provider timing. Requests lead with searchable/filterable recent activity and an explicit in-memory privacy note; each row presents time, app, outcome, answer/model and concise latency. Settings keep current groups and advanced disclosures, but clarify which controls save for later and which runtime actions apply immediately. Keep the existing color roles, Segoe UI/system font stack, dark mode, reduced-motion behavior and plain HTML/CSS/JS delivery.

For Requests, make the selected item a focused request receipt. Present an at-a-glance header (time, app, outcome, model attempts and timing), then clearly separated sections for text sent to the model, any suffix, window context read, the subset sent, screenshot, returned suggestion, settings and event history. Label the distinction between screen text read and context actually sent; omit absent material with an explicit explanation. Retain the existing on-demand screenshot fetch into an in-memory blob, token header, blob URL cleanup and no-store behavior. Keep the log's ephemeral/last-200/clear behavior visible near its filters.

## Interaction and safety requirements

- Preserve the current hash navigation and direct links, dirty-settings confirmation, focus-visible treatment, roving tab keyboard behavior, and focus return from the request receipt. On mobile, allow the tab strip to scroll without trapping page scrolling; ensure the selected tab is scrolled into view after keyboard navigation.
- Make the receipt a semantic modal dialog when open: announce its title, mark the background inert, contain keyboard focus, close on Escape, and return focus to the originating request control. On small screens it may occupy the full viewport; on wide screens retain a right-side sheet. Do not let clicks on a non-button table row be the only way to open a receipt.
- Preserve latest-result-wins behavior for word/phrase searches. Add the same sequencing guard to overlapping request-list refreshes and request-detail/screenshot loads so a slower earlier response cannot replace the user's newer filter or selected request. Ignore completion from a closed/replaced receipt and revoke any superseded blob URL. Keep polling only while Requests is visible and the document is visible; avoid rebuilding unchanged rows so focus and pointer state remain stable.
- Keep Settings as the authority for persistent configuration. Do not expose an API key value, copy it into labels/errors, or change its write-only interaction. Preserve exact context preview invalidation and the runtime guard that disables Send when paused/private. Requests may show only the already-recorded request detail; the redesign must not increase capture, retention, or transmission.
- Use semantic landmarks, headings, labelled controls, table headers, live status/count announcements and outcome text that does not rely on color alone. At 200% zoom and narrow widths, controls must remain reachable without horizontal page overflow; wide request/stat tables may scroll within their own labelled region. Respect reduced motion and ensure no content depends on hover.

## Existing implementation boundaries

Primary implementation surface is `engine/src/completionist_engine/viewer.html`, which includes its own styles and scripts because the viewer is served as one local page under a restrictive Content Security Policy. Existing JSON routes in `engine/src/completionist_engine/viewer.py` already supply the words, trigrams, counts-only stats, request list/detail/screenshots, runtime/context state and settings. Keep those contracts and the loopback, token, Host and Origin checks intact. The request detail route already returns prompt, suffix, settings, attempts, events and screen metadata; no new backend field is needed for the receipt.

Relevant existing coverage is in `engine/tests/test_viewer.py` and `engine/tests/test_viewer_runtime.py`: route authentication, write-only API-key behavior, list filtering/deletion, statistics, request detail/screenshots, context/runtime actions and settings validation. These tests protect the data and security boundary; visual and interaction behavior needs browser-level validation because current tests do not exercise the page layout.

## Alternatives and tradeoffs

1. **Recommended: preserve the section tabs and refine hierarchy/responsiveness.** Lowest risk, maintains direct-link and keyboard conventions, and gives the existing four core areas prominence without inventing a new landing surface.
2. **Dashboard-first landing page.** Could summarize usage and recent requests, but duplicates Stats and Requests, weakens the direct task model, and makes every opening choose between a dashboard and the actual tools.
3. **Mobile hamburger/drawer navigation.** Saves header space, but hides destinations and adds open/close/focus state. The current five short labels fit a scrollable tab strip and remain discoverable.

## Validation plan

- Run `uv run pytest tests/test_viewer.py tests/test_viewer_runtime.py` from `engine/` to protect existing route, data, security, settings and runtime contracts. Run the full engine test suite if shared API behavior changes; this design does not call for API changes.
- In a real browser, inspect light and dark themes at desktop width, 560px, and a narrow phone width, then at 200% zoom. Confirm no page-level horizontal overflow, tab visibility/scrolling, readable tables, and reachable Settings controls.
- Exercise keyboard-only navigation through all tabs, the request list and its receipt. Confirm arrow/Home/End tab behavior, Enter/Space activation, dialog focus containment, Escape close, focus restoration and screen-reader announcements for result counts/outcomes.
- With a delayed network response, rapidly change request filters and open two receipts; confirm only the latest list/detail/screenshot is shown, closing/replacing a receipt drops stale work, and polling stops when Requests is hidden or the document is backgrounded.
- Verify request receipt content against a request with screenshot/context and one without them; distinguish read context from sent context and keep the API key absent. Confirm clearing removes the list and screenshot, and that preview send remains unavailable while private mode or pause is active.

## Open implementation concern

The request list polling path currently compares a response signature to avoid rebuilding unchanged rows, but does not sequence concurrent requests when filters or polling overlap. The receipt's detail fetch is also not tied to a selection generation, and screenshot loads only check whether the containing box remains connected. Address these races locally in the page before calling the redesigned Requests interaction complete; no backend change is indicated.
