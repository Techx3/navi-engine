/**************************************************************************/
/*  ai_assistant_dock.h                                                   */
/**************************************************************************/
/*                         This file is part of:                          */
/*                              NAVI ENGINE                               */
/*                        https://github.com/Techx3/navi-engine           */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#pragma once

#include "editor/docks/editor_dock.h"
#include "core/variant/array.h"

class Button;
class CheckBox;
class AIChatService;
class LineEdit;
class OptionButton;
class RichTextLabel;
class TextEdit;

class AIAssistantDock : public EditorDock {
	GDCLASS(AIAssistantDock, EditorDock);

	OptionButton *provider_selector = nullptr;
	LineEdit *model_edit = nullptr;
	CheckBox *agent_mode = nullptr;
	CheckBox *include_scene = nullptr;
	CheckBox *include_script = nullptr;
	RichTextLabel *conversation = nullptr;
	TextEdit *prompt_edit = nullptr;
	Button *send_button = nullptr;
	Button *checkpoint_button = nullptr;
	Button *refresh_ollama_button = nullptr;
	Button *apply_actions_button = nullptr;
	Button *revert_checkpoint_button = nullptr;
	AIChatService *chat_service = nullptr;
	Array pending_actions;

	void _provider_selected(int p_index);
	void _checkpoint_pressed();
	void _refresh_ollama_models_pressed();
	void _apply_actions_pressed();
	void _revert_checkpoint_pressed();
	void _send_pressed();
	void _ai_response_received(const String &p_response);
	void _ai_request_failed(const String &p_message);
	void _ollama_models_received(const PackedStringArray &p_models);
	void _sync_from_settings();
	void _save_provider_settings();
	void _update_provider_controls();
	String _get_selected_provider() const;

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	AIAssistantDock();
};
