# Godot AI Assistant Skills

This document defines the capability map for an AI assistant embedded in the Godot editor.
The assistant is expected to choose the right skill automatically from the user's request, while
allowing the user to choose the model provider and model independently.

## Design Principles

- The assistant is an editor agent, not only a chat panel.
- It can inspect the whole open project: scenes, nodes, resources, scripts, settings, debugger output,
  filesystem, import state, export presets, and editor selection.
- It can edit project content through explicit tools and undo-aware editor operations.
- Risky changes require preview and confirmation unless the user enables trusted auto-apply.
- Provider selection is separate from skill selection.
- Local models through Ollama must support free-form model names, editable base URL, and optional custom
  model creation settings.

## Skill Router

The assistant should classify every request into one or more skills:

1. Intent classification.
2. Context collection.
3. Model/provider selection.
4. Tool planning.
5. Preview or execute.
6. Verify result.

Each skill exposes:

- `name`: stable skill id.
- `description`: what it does.
- `required_context`: data needed from the editor.
- `tools`: editor/project operations it may call.
- `risk_level`: read-only, reversible, file-editing, scene-editing, destructive.
- `confirmation_policy`: never, preview, always.

## Core Skills

### godot_docs_expert

Answers questions using Godot documentation and class reference.

Must understand:

- Nodes, scenes, resources, signals, groups, autoloads.
- GDScript, C#, C++, GDExtension.
- 2D, 3D, physics, navigation, UI, animation, audio.
- Shaders, rendering, materials, lighting.
- Import/export, platform deployment, project settings.
- Debugging, profiling, optimization, troubleshooting.

### project_observer

Builds a structured snapshot of the current project.

Must inspect:

- Open scenes and edited scene root.
- Selected nodes and inspector object.
- Node tree paths, types, owners, groups, signals.
- Current script, cursor, selection, unsaved state.
- Project files, addons, import files, autoloads.
- Project settings and input map.
- Debugger/output errors.

### scene_editor

Creates and edits scenes.

Must support:

- Add, remove, rename, duplicate, reparent nodes.
- Set node ownership correctly for scene saving.
- Change transforms, anchors, layout, physics layers, groups.
- Connect and disconnect signals.
- Instantiate PackedScenes.
- Create reusable scene components.
- Save scene changes with undo support.

### script_editor

Reads and edits scripts.

Must support:

- Generate GDScript, C#, and tool scripts.
- Refactor selected code.
- Fix parser/runtime errors.
- Insert code at cursor or replace selection.
- Create script files and attach them to nodes.
- Update signal callbacks.
- Respect Godot naming and lifecycle methods.

### resource_editor

Creates and edits resources.

Must support:

- Materials, themes, animations, curves, tile sets, input events.
- Texture/resource references.
- `.tres`, `.res`, `.tscn`, `.scn`, `.import`, and project resources.
- Safe save/reload behavior.

### inspector_operator

Manipulates the current inspector object.

Must support:

- Read visible and hidden properties.
- Set property values.
- Explain property meaning.
- Batch edit selected nodes.
- Use undo/redo for property edits.

### ui_builder

Builds Godot UI scenes.

Must support:

- Control nodes, containers, anchors, size flags, themes.
- Menus, dialogs, HUDs, settings panels, editor-like tools.
- Responsive layout behavior.
- Accessibility names and focus flow where relevant.

### game_2d_builder

Builds and modifies 2D game systems.

Must support:

- CharacterBody2D, Area2D, RigidBody2D, TileMap/TileSet.
- Cameras, parallax, collisions, ray casts.
- AnimationPlayer, AnimatedSprite2D.
- Input actions and controller support.

### game_3d_builder

Builds and modifies 3D game systems.

Must support:

- Node3D, MeshInstance3D, CharacterBody3D, Area3D, RigidBody3D.
- Cameras, lights, materials, environment, world settings.
- NavigationRegion3D, NavigationAgent3D.
- Skeletons, animation, import workflows.

### shader_author

Creates and debugs shaders.

Must support:

- CanvasItem, Spatial, Particles, Sky, Fog shaders.
- Uniforms, varyings, render modes.
- ShaderMaterial setup.
- Visual Shader guidance.

### animation_director

Creates and edits animation logic.

Must support:

- AnimationPlayer tracks.
- AnimationTree state machines/blend trees.
- Signals and callbacks.
- UI transitions and gameplay animations.

### audio_director

Creates and edits audio systems.

Must support:

- AudioStreamPlayer, 2D/3D audio players.
- Audio buses, effects, volume routing.
- Music loops and sound effects.

### input_mapper

Configures input.

Must support:

- Input Map actions.
- Keyboard, mouse, gamepad, touch.
- Runtime input handling examples.
- Rebinding UI.

### debugger

Diagnoses errors.

Must support:

- Parse output/debugger messages.
- Explain stack traces.
- Find referenced scripts/nodes.
- Suggest and apply fixes.
- Run validation after edits.

### performance_optimizer

Improves runtime/editor performance.

Must support:

- Profiler guidance.
- Draw calls, physics cost, script hot paths.
- Resource loading, threading, pooling.
- 2D/3D renderer tradeoffs.

### importer

Handles asset import workflows.

Must support:

- Texture, audio, 3D model import options.
- Reimport resources.
- Explain import warnings.
- Set import presets when available.

### exporter

Configures export.

Must support:

- Export presets.
- Platform settings.
- Icons, signing, permissions, templates.
- HTML5, desktop, mobile, XR considerations.

### addon_plugin_builder

Creates Godot editor plugins.

Must support:

- `addons/<plugin_name>/plugin.cfg`.
- `EditorPlugin` scripts.
- Custom docks, inspector plugins, import plugins.
- Tool scripts and main-screen plugins.

### gdextension_builder

Guides or creates GDExtension scaffolding.

Must support:

- `.gdextension` files.
- C++ class registration.
- Bind methods, properties, and signals.
- Build system guidance.

### version_control_assistant

Works with project version control.

Must support:

- Explain changed files.
- Generate commit messages.
- Identify generated/import files.
- Avoid overwriting user work.

### test_runner

Validates changes.

Must support:

- Run project or scene.
- Run available test commands.
- Validate scripts where possible.
- Report failures with actionable context.

## Model Provider Skills

### provider_openai

Uses OpenAI frontier, coding, realtime, audio, image, and embedding models through configurable model ids.

Requirements:

- API key setting.
- Base URL override for compatible gateways.
- Model list refresh via provider API when available.
- Manual model id entry.
- Tool/function calling support.
- Structured output support.

### provider_anthropic

Uses Anthropic Claude models through configurable model ids.

Requirements:

- API key setting.
- Anthropic API version setting.
- Model list refresh via provider API.
- Manual model id entry.
- Tool/function calling support.

### provider_gemini

Uses Google Gemini models through configurable model ids.

Requirements:

- API key setting.
- Model list refresh.
- Manual model id entry.
- Support model aliases such as latest/stable/preview patterns where the API supports them.

### provider_ollama

Uses local or Ollama-hosted models.

Requirements:

- Default base URL: `http://localhost:11434`.
- Editable base URL.
- Editable model id.
- Refresh local model list.
- Pull model by name.
- Optional create/customize model from an existing model.
- Runtime options: temperature, top_p, num_ctx, keep_alive, seed.
- Streaming support.
- Tool calling when the chosen local model supports it.

## Model Selection Policy

The user can choose:

- Provider.
- Model.
- Reasoning effort or equivalent setting when provider supports it.
- Temperature and token/output limits.
- Whether to prefer local models.

The assistant can recommend:

- A stronger model for agentic scene/script edits.
- A cheaper/faster model for documentation Q&A.
- A local Ollama model for private/offline work.

The assistant must not hardcode a permanent frontier model list. It should keep defaults in settings and
allow refresh/manual override because provider model availability changes.

## Permission Levels

### Read

Allowed by default:

- Inspect project files.
- Inspect open scenes.
- Read scripts/resources/settings.
- Read debugger/output.

### Suggest

Allowed by default:

- Generate explanations.
- Propose scene/script/resource changes.
- Show diffs or action plans.

### Edit

Requires confirmation by default:

- Modify scripts.
- Modify scenes/resources.
- Create files.
- Change project settings.
- Install addons.
- Run imports.

### Dangerous

Always requires confirmation:

- Delete files/nodes/resources.
- Overwrite scenes.
- Run external commands.
- Change export signing credentials.
- Send large project context to a cloud provider.

## Privacy Policy

Before sending context to cloud providers, the assistant must show or summarize what context will be sent.
Local Ollama can be marked as trusted local mode. Secrets, keys, export credentials, and `.env` content must
be redacted unless the user explicitly allows using them.

## Initial MVP

1. AI dock with chat.
2. Provider/model settings for OpenAI, Anthropic, Gemini, and Ollama.
3. Current-scene and current-script context.
4. Documentation Q&A.
5. Script edit preview and apply.
6. Scene edit action preview and apply.
7. Debugger/output explanation.

## Later Milestones

1. Full project semantic index.
2. Class-reference RAG.
3. Visual scene inspector for AI.
4. Multi-step agent plans.
5. Undo-aware batch edits.
6. Local embedding/index support.
7. Plugin marketplace/addon generation.
8. Export/deployment assistant.
