# NAVI Engine Roadmap

NAVI Engine is a Godot-based editor fork with a built-in AI agent, modernized editor interface,
local/offline documentation support, multi-provider model selection, project-scoped agent permissions,
voice-ready architecture, and built-in version checkpoints.

## Product Decisions

- Final product name: NAVI Engine.
- GitHub repository: `https://github.com/Techx3/navi-engine`.
- Base engine: Godot 4.x fork.
- Distribution: public distributable fork, with Godot MIT license notices preserved.
- Branding: custom NAVI Engine branding and logo. Do not use the official Godot logo as product branding.
- Primary symbol asset: `misc/branding/navi/navi-symbol.png`.
- Primary logo lockup asset: `misc/branding/navi/navi-logo-lockup.png`.
- Editor integration: built into the compiled editor, not only as a project addon.
- UI direction: moderate visual refresh that keeps Godot's existing workflows recognizable.
- Agent scope: project-only by default (`res://`), with exceptions reviewed case by case.
- Agent permissions: enabled per project, with action history and revert support.
- Documentation: offline indexed docs as primary source; online docs as update/fallback source.
- Languages: Spanish and English by default.
- Voice: architecture prepared from the start; local Whisper/other local STT/TTS added after text MVP.
- API keys: saved in local editor configuration only, never committed to projects.

## MVP Order

1. Visual foundation for NAVI Engine branding and a modernized editor shell.
2. Functional AI dock visible by default but hideable.
3. Provider/model settings for OpenAI, Anthropic, Gemini, and Ollama.
4. Version checkpoint system before AI edits.
5. Project context reader for current scene, selection, current script, project files, and errors.
6. Script and scene edits with preview, apply, action history, and revert.
7. Offline documentation index.
8. Game execution/debug-output reading and fix suggestions.

## Version Control Strategy

NAVI Engine needs two related but separate version systems:

1. Engine repository versioning.
   - Keep upstream Godot as reference.
   - Work on a NAVI Engine branch or separate remote.
   - Preserve license and copyright files.

2. User project checkpoints.
   - If the user's project is already a Git repo, create AI checkpoints as commits or stash-like snapshots.
   - If the user's project is not a Git repo, create local snapshots under `.navi_ai/snapshots`.
   - Every agent edit session gets an action log and revert metadata.

## AI Provider Requirements

### OpenAI

- API key field in local editor settings.
- Model list refresh when available.
- Manual model id entry.
- Base URL override for compatible gateways.
- Tool/function calling support.

### Anthropic

- API key field in local editor settings.
- API version setting.
- Model list refresh when available.
- Manual model id entry.
- Tool/function calling support.

### Gemini

- API key field in local editor settings.
- Model list refresh when available.
- Manual model id entry.

### Ollama

- Default URL: `http://localhost:11434`.
- Editable base URL.
- Refresh installed local models.
- Editable model id.
- Pull model by name.
- Runtime options: temperature, top_p, num_ctx, keep_alive, seed.
- Streaming support.

## Editor AI Surfaces

### AI Dock

- Chat transcript.
- Provider/model selector.
- Context toggles.
- Agent mode toggle per project.
- Apply/revert controls.
- Voice controls when voice support lands.

### Context Panel

- Current scene.
- Current selection.
- Current script and cursor/selection.
- Errors and warnings.
- Relevant docs snippets.
- Pending action plan.

### Change Preview

- Script diff.
- Scene action list.
- Resource edits.
- Project settings changes.
- Risk labels.
- Apply and revert buttons.

## Required Engine Areas To Implement

- `editor/ai`: AI dock, provider clients, context builder, action executor, docs index.
- `editor/docks`: integration through `EditorDock`.
- `editor/plugins`: integration through `EditorPlugin`.
- `editor/settings`: local provider/model/API-key settings.
- `editor/script`: current script context and script edit application.
- `editor/scene`: scene/node action execution.
- `editor/debugger`: debugger/output context.
- `editor/themes`: NAVI visual refresh.

## Safety Rules

- Read-only inspection is allowed by default.
- Sending cloud context requires visible context summary and user consent.
- AI edits must create a checkpoint first.
- Destructive actions always require confirmation.
- Secrets are redacted from cloud prompts unless explicitly allowed.
- Agent mode is project-scoped and can be disabled.

## Open Questions

- Final custom logo asset path.
- Whether the engine repo remote should be replaced now or after the first MVP.
- Whether NAVI Engine should maintain upstream Godot sync as a branch strategy.
- Exact visual direction for the first editor refresh pass.
