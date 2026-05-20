/**************************************************************************/
/*  ai_assistant_dock.cpp                                                 */
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

#include "ai_assistant_dock.h"

#include "ai_chat_service.h"
#include "ai_context_builder.h"
#include "ai_project_checkpoint_manager.h"

#include "core/object/callable_mp.h"
#include "editor/settings/editor_settings.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/check_box.h"
#include "scene/gui/label.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/option_button.h"
#include "scene/gui/rich_text_label.h"
#include "scene/gui/separator.h"
#include "scene/gui/text_edit.h"

void AIAssistantDock::_bind_methods() {
}

void AIAssistantDock::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			_sync_from_settings();
		} break;

		case NOTIFICATION_THEME_CHANGED: {
			send_button->set_button_icon(get_editor_theme_icon(SNAME("Play")));
			checkpoint_button->set_button_icon(get_editor_theme_icon(SNAME("VCSCommit")));
			refresh_ollama_button->set_button_icon(get_editor_theme_icon(SNAME("Reload")));
		} break;
	}
}

String AIAssistantDock::_get_selected_provider() const {
	const int selected = provider_selector->get_selected();
	if (selected < 0) {
		return "Ollama";
	}
	return provider_selector->get_item_text(selected);
}

void AIAssistantDock::_provider_selected(int p_index) {
	_save_provider_settings();

	const String provider = provider_selector->get_item_text(p_index);
	const String setting_path = "ai/providers/" + provider.to_lower() + "/model";
	if (EditorSettings::get_singleton()->has_setting(setting_path)) {
		model_edit->set_text(EDITOR_GET(setting_path));
	}
	_update_provider_controls();
}

void AIAssistantDock::_save_provider_settings() {
	const String provider = _get_selected_provider();
	const String provider_setting = "ai/provider";
	const String model_setting = "ai/providers/" + provider.to_lower() + "/model";

	EditorSettings::get_singleton()->set_setting(provider_setting, provider);
	EditorSettings::get_singleton()->set_setting(model_setting, model_edit->get_text().strip_edges());
	EditorSettings::get_singleton()->set_setting("ai/agent/enabled_for_project", agent_mode->is_pressed());
	EditorSettings::get_singleton()->set_setting("ai/context/include_current_scene", include_scene->is_pressed());
	EditorSettings::get_singleton()->set_setting("ai/context/include_current_script", include_script->is_pressed());
	EditorSettings::get_singleton()->save();
}

void AIAssistantDock::_checkpoint_pressed() {
	String commit_hash;
	String message;
	const Error err = AIProjectCheckpointManager::create_checkpoint(TTR("Manual AI checkpoint"), &commit_hash, &message);

	conversation->append_text("[b]NAVI[/b]\n");
	if (err == OK) {
		if (!commit_hash.is_empty()) {
			conversation->append_text(vformat(TTR("Checkpoint ready: %s"), commit_hash).xml_escape() + "\n\n");
		} else {
			conversation->append_text(message.xml_escape() + "\n\n");
		}
	} else {
		conversation->append_text("[color=red]" + message.xml_escape() + "[/color]\n\n");
	}
}

void AIAssistantDock::_refresh_ollama_models_pressed() {
	String error_message;
	const Error err = chat_service->refresh_ollama_models(&error_message);
	if (err != OK) {
		_ai_request_failed(error_message);
		return;
	}

	refresh_ollama_button->set_disabled(true);
}

void AIAssistantDock::_ollama_models_received(const PackedStringArray &p_models) {
	refresh_ollama_button->set_disabled(false);

	conversation->append_text("[b]NAVI[/b]\n");
	if (p_models.is_empty()) {
		conversation->append_text(TTR("Ollama is reachable, but no local models were returned.") + String("\n\n"));
		return;
	}

	if (model_edit->get_text().strip_edges().is_empty()) {
		model_edit->set_text(p_models[0]);
		_save_provider_settings();
	}

	conversation->append_text(TTR("Local Ollama models:") + String("\n"));
	for (int i = 0; i < p_models.size(); i++) {
		conversation->append_text("- " + String(p_models[i]).xml_escape() + "\n");
	}
	conversation->append_text("\n");
}

void AIAssistantDock::_ai_response_received(const String &p_response) {
	conversation->append_text("[b]NAVI[/b]\n");
	conversation->append_text(p_response.xml_escape() + "\n\n");
	send_button->set_disabled(false);
	refresh_ollama_button->set_disabled(false);
}

void AIAssistantDock::_ai_request_failed(const String &p_message) {
	conversation->append_text("[b]NAVI[/b]\n");
	conversation->append_text("[color=red]" + p_message.xml_escape() + "[/color]\n\n");
	send_button->set_disabled(false);
	refresh_ollama_button->set_disabled(false);
}

void AIAssistantDock::_send_pressed() {
	const String prompt = prompt_edit->get_text().strip_edges();
	if (prompt.is_empty()) {
		return;
	}

	_save_provider_settings();

	const Dictionary editor_context = AIContextBuilder::build_context(include_scene->is_pressed(), include_script->is_pressed());
	const String context_summary = AIContextBuilder::summarize_context(editor_context);

	conversation->append_text("[b]You[/b]\n");
	conversation->append_text(prompt.xml_escape() + "\n\n");
	conversation->append_text("[color=gray]" + context_summary.xml_escape() + "[/color]\n\n");

	String error_message;
	const Error err = chat_service->send_chat(_get_selected_provider(), model_edit->get_text(), prompt, editor_context, &error_message);
	if (err != OK) {
		_ai_request_failed(error_message);
		return;
	}

	send_button->set_disabled(true);
	prompt_edit->clear();
}

void AIAssistantDock::_sync_from_settings() {
	const String provider = EDITOR_GET("ai/provider");
	for (int i = 0; i < provider_selector->get_item_count(); i++) {
		if (provider_selector->get_item_text(i) == provider) {
			provider_selector->select(i);
			break;
		}
	}

	const String model_setting = "ai/providers/" + _get_selected_provider().to_lower() + "/model";
	if (EditorSettings::get_singleton()->has_setting(model_setting)) {
		model_edit->set_text(EDITOR_GET(model_setting));
	}

	agent_mode->set_pressed(EDITOR_GET("ai/agent/enabled_for_project"));
	include_scene->set_pressed(EDITOR_GET("ai/context/include_current_scene"));
	include_script->set_pressed(EDITOR_GET("ai/context/include_current_script"));
	_update_provider_controls();
}

void AIAssistantDock::_update_provider_controls() {
	if (refresh_ollama_button) {
		refresh_ollama_button->set_visible(_get_selected_provider() == "Ollama");
	}
}

AIAssistantDock::AIAssistantDock() {
	set_title(TTRC("NAVI AI"));
	set_layout_key("NaviAI");
	set_icon_name("Node");
	set_default_slot(EditorDock::DOCK_SLOT_RIGHT_BL);
	set_available_layouts(EditorDock::DOCK_LAYOUT_VERTICAL | EditorDock::DOCK_LAYOUT_FLOATING);

	VBoxContainer *root = memnew(VBoxContainer);
	root->set_h_size_flags(SIZE_EXPAND_FILL);
	root->set_v_size_flags(SIZE_EXPAND_FILL);
	add_child(root);

	chat_service = memnew(AIChatService);
	chat_service->connect("response_received", callable_mp(this, &AIAssistantDock::_ai_response_received));
	chat_service->connect("request_failed", callable_mp(this, &AIAssistantDock::_ai_request_failed));
	chat_service->connect("ollama_models_received", callable_mp(this, &AIAssistantDock::_ollama_models_received));
	add_child(chat_service);

	Label *title_label = memnew(Label);
	title_label->set_text(TTR("NAVI AI"));
	title_label->add_theme_font_size_override(SceneStringName(font_size), 18 * EDSCALE);
	root->add_child(title_label);

	HBoxContainer *provider_row = memnew(HBoxContainer);
	root->add_child(provider_row);

	provider_selector = memnew(OptionButton);
	provider_selector->set_h_size_flags(SIZE_EXPAND_FILL);
	provider_selector->add_item("OpenAI");
	provider_selector->add_item("Anthropic");
	provider_selector->add_item("Gemini");
	provider_selector->add_item("Ollama");
	provider_selector->connect(SceneStringName(item_selected), callable_mp(this, &AIAssistantDock::_provider_selected));
	provider_row->add_child(provider_selector);

	model_edit = memnew(LineEdit);
	model_edit->set_h_size_flags(SIZE_EXPAND_FILL);
	model_edit->set_placeholder(TTR("Model"));
	provider_row->add_child(model_edit);

	root->add_child(memnew(HSeparator));

	agent_mode = memnew(CheckBox);
	agent_mode->set_text(TTR("Agent mode for this project"));
	root->add_child(agent_mode);

	checkpoint_button = memnew(Button);
	checkpoint_button->set_text(TTR("Create checkpoint"));
	checkpoint_button->connect(SceneStringName(pressed), callable_mp(this, &AIAssistantDock::_checkpoint_pressed));
	root->add_child(checkpoint_button);

	refresh_ollama_button = memnew(Button);
	refresh_ollama_button->set_text(TTR("Refresh Ollama models"));
	refresh_ollama_button->connect(SceneStringName(pressed), callable_mp(this, &AIAssistantDock::_refresh_ollama_models_pressed));
	root->add_child(refresh_ollama_button);

	include_scene = memnew(CheckBox);
	include_scene->set_text(TTR("Include current scene"));
	root->add_child(include_scene);

	include_script = memnew(CheckBox);
	include_script->set_text(TTR("Include current script"));
	root->add_child(include_script);

	conversation = memnew(RichTextLabel);
	conversation->set_h_size_flags(SIZE_EXPAND_FILL);
	conversation->set_v_size_flags(SIZE_EXPAND_FILL);
	conversation->set_use_bbcode(true);
	conversation->set_fit_content(false);
	conversation->append_text("[b]NAVI[/b]\n");
	conversation->append_text(TTR("Ask about the project, request scene/script changes, or inspect errors. Provider clients and action execution are the next implementation step.") + String("\n\n"));
	root->add_child(conversation);

	prompt_edit = memnew(TextEdit);
	prompt_edit->set_custom_minimum_size(Size2(0, 92 * EDSCALE));
	prompt_edit->set_placeholder(TTR("Ask NAVI to inspect, explain, generate, or modify this project..."));
	root->add_child(prompt_edit);

	send_button = memnew(Button);
	send_button->set_text(TTR("Send"));
	send_button->connect(SceneStringName(pressed), callable_mp(this, &AIAssistantDock::_send_pressed));
	root->add_child(send_button);
}
