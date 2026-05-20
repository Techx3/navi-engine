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

#include "ai_context_builder.h"

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
	conversation->append_text("[b]NAVI[/b]\n");
	conversation->append_text(TTR("The AI runtime is not connected yet. The editor context is now captured and ready for provider clients, checkpoints, and action execution.") + String("\n\n"));
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
