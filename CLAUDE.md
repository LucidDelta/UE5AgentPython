# CLAUDE.md

Context for Claude (or any AI assistant) continuing work on this plugin. Read this before making non-trivial changes — it covers architectural quirks that are easy to trip over.

## What this plugin is

A UE 5.5 **Editor** plugin (not Runtime) that:
1. Renders a Slate panel docked as a nomad tab.
2. Takes a plain-English prompt + auto-injected editor context.
3. Sends it over HTTPS to one of three AI providers.
4. Executes the returned Python via `IPythonScriptPlugin`.
5. Logs everything to `<Project>/Saved/UE5AgentPython/session.log`.

Scope is deliberately **power-tool for senior UE devs** — batch/bulk content-browser and outliner work. Not gameplay logic.

## Module layout

```
Source/UE5AgentPython/
├── Public/
│   └── UE5AgentPython.h                  ← IModuleInterface, tab spawner
└── Private/
    ├── UE5AgentPython.cpp                ← StartupModule, menu registration
    ├── UE5AgentPythonStyle.h/.cpp        ← FSlateStyleSet for custom SVG icons
    ├── SUE5AgentPythonPanel.h/.cpp       ← the Slate panel (most of the UI logic)
    ├── UE5AgentRunner.h/.cpp             ← provider-agnostic HTTP layer
    └── (providers inlined in UE5AgentRunner.cpp via switch on EAIProvider)
```

## Key architectural decisions

### Context injection

Every `Execute` click silently prepends a **context block** to the user's prompt containing:
- Viewport selected actors (capped at 50): name, class, mobility, location
- Content browser selected assets (capped at 50): name, class, package path
- Content browser active folder

The user never sees this — the model disambiguates pronouns like "these actors" / "this folder" from it. Implementation: `SUE5AgentPythonPanel::BuildContextBlock()`.

**Don't** surface the context block in the UI. It's deliberately invisible — chips, dropdowns, and confirmation steps were all considered and rejected as friction.

### Conversation history

When conversation mode is on, prior `(prompt, response)` pairs are included in each request. The **original prompt** (no context block) is stored in history — not the one that was sent. Rationale: fresh context on every turn stays relevant; stale context from three turns ago is just noise.

### Running indicator timing

`RunningIndicator` (an `SBox` wrapping an `SCircularThrobber` + "Running…" text) becomes visible on `OnExecuteClicked` and is hidden **only after `ExecutePythonFromResponse` returns** — i.e. it covers the HTTP round-trip *plus* the local Python execution. If you refactor the execute flow, keep this invariant or the user sees the UI lie about when work is done.

### Log viewer

The in-panel log uses `SMultiLineEditableText` with `IsReadOnly(true)` inside an `SScrollBox`. Not `STextBlock` — that's not selectable. If you "simplify" to a text block, you break Ctrl+C.

## Non-obvious gotchas

### Menu registration (duplicate-entry trap)

The nomad tab spawner **auto-registers itself** in the Tools menu via UE's dynamic `TabManagerSection`. If you also manually add a menu entry (which we do), you get **two** entries. Fix: `.SetMenuType(ETabSpawnerMenuType::Hidden)` on the tab spawner. This is load-bearing — don't remove it.

We also `UnregisterOwnerByName` at the start of `RegisterMenus` to defuse live-coding duplicates. Stable FName owner (`"UE5AgentPythonMenuOwner"`), not `this`, so unregister works across hot reloads.

### Menu section name

We use `FindOrAddSection(TEXT("Tools"))` — deliberately the same section other AI-tool plugins use (e.g. UE5AgentInsight), so our entry lands alongside them instead of in its own subsection. Another installed plugin (IGToolsTCP) relabels that section to "IG Tools"; we don't fight it.

### SVG icons & nanosvg

Custom SVGs in `Resources/` are loaded via `FSlateVectorImageBrush`. nanosvg is picky:
- Root `<svg>` must have explicit `width` and `height` attributes.
- `fill-rule="evenodd"` is supported.
- Path data on a single line works reliably; multi-line `d` attributes sometimes don't.

**But don't fight this if you don't have to.** The log button now uses `FAppStyle::GetBrush("Icons.Details")` — an engine brush — rather than a custom SVG. Only the menu/tab icon is custom. If you need another icon, prefer an `FAppStyle` brush first; custom SVG only if nothing fits.

### `WeakThis` shadow warning

`SCompoundWidget` (via `TSharedFromThis`) has a member named `WeakThis`. If you write:
```cpp
TWeakPtr<SUE5AgentPythonPanel> WeakThis = SharedThis(this);
```
you get a C4458 shadow warning. Always name it `WeakPanel` (or anything else) in this class.

### `LogPath` shadow warning

`EngineLogs.h` exposes a global `LogPath`. Local variables named `LogPath` in `.cpp` files including it throw C4459. Use `SessionLogPath` or similar.

### `SComboBox::RefreshOptions`, not `RequestListRefresh`

`SComboBox` only exposes `RefreshOptions()` — don't call `RequestListRefresh()`.

### `SThrobber` include path

UE 5.5: `#include "Widgets/Images/SThrobber.h"`. Older docs list it under `Widgets/Notifications/` — wrong in 5.5.

## HTTP & provider quirks

### Anthropic vs OpenAI/Gemini message shape

Anthropic requires `system` as a top-level field + `messages[]`. OpenAI and Gemini (via OpenAI-compat endpoint) take the system prompt as the first message with `role: "system"`. `UE5AgentRunner::SendCompletion` branches on `EAIProvider` — don't try to unify the shape.

### Markdown fence stripping

Models regularly return ```` ```python ... ``` ```` even when told not to. `StripMarkdownFences()` strips leading/trailing fences defensively. Don't remove it just because the system prompt "says" not to emit them.

### HTTP callbacks run on the game thread

`FHttpModule` bindings fire on the game thread, so direct Slate widget mutation from `OnProcessRequestComplete` is safe. Don't wrap in `AsyncTask(ENamedThreads::GameThread, ...)` — that adds latency for no reason.

## Settings persistence

Keys and last-used model per provider go to `GEditorPerProjectIni` under `[UE5AgentPython]`. Three keys (`AnthropicAPIKey`, `OpenAIAPIKey`, `GeminiAPIKey`) and three models (`AnthropicModel`, `OpenAIModel`, `GeminiModel`). Load/save lives in `LoadSettings` / `SaveSettings` on the panel.

## Build dependencies

`UE5AgentPython.Build.cs` public deps: Core, CoreUObject, Engine, Slate, SlateCore, InputCore. Private deps include: EditorStyle, LevelEditor, ToolMenus, ContentBrowser, ContentBrowserData, UnrealEd, PythonScriptPlugin, Projects, HTTP, Json, JsonUtilities, AssetRegistry, AssetTools, EditorFramework. Adding a new dep? Update the Build.cs, not a random header.

## What NOT to do

- ❌ Don't add gameplay-logic-style prompts to the system prompt. Scope is batch/content tooling.
- ❌ Don't add multi-round-trip planning ("ask the model for a plan, then execute"). Rejected for latency with no reliability gain.
- ❌ Don't surface the context block in the UI. Invisible is the feature.
- ❌ Don't commit anything under `Saved/` or `Binaries/`. Keys live there.
- ❌ Don't remove `.SetMenuType(ETabSpawnerMenuType::Hidden)`.
- ❌ Don't rewrite the log widget as an `STextBlock`.

## What's fair game

- ✅ Adding more providers — slot into `EAIProvider` + switch branches in `UE5AgentRunner`.
- ✅ Tightening the system prompt for specific batch patterns.
- ✅ Adding a prompt-template picker (user-configurable snippets).
- ✅ Log filtering / search in the log panel.
- ✅ Exporting the last session's prompts+responses as a `.md` for sharing.
