/**************************************************************************/
/*  ai_context_builder.cpp                                                */
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

#include "ai_context_builder.h"

#include "core/io/resource.h"
#include "core/object/object.h"
#include "core/string/translation.h"
#include "core/templates/list.h"
#include "core/variant/array.h"
#include "editor/editor_data.h"
#include "editor/editor_interface.h"
#include "editor/script/script_editor_base.h"
#include "editor/script/script_editor_plugin.h"
#include "scene/gui/text_edit.h"
#include "scene/main/node.h"

Dictionary AIContextBuilder::_build_scene_context() {
	Dictionary scene;

	EditorInterface *editor_interface = EditorInterface::get_singleton();
	if (!editor_interface) {
		return scene;
	}

	Node *scene_root = editor_interface->get_edited_scene_root();
	if (!scene_root) {
		return scene;
	}

	scene["name"] = scene_root->get_name();
	scene["type"] = scene_root->get_class();
	scene["path"] = scene_root->get_scene_file_path();
	scene["child_count"] = scene_root->get_child_count();

	Array selected_nodes;
	EditorSelection *selection = editor_interface->get_selection();
	if (selection) {
		List<Node *> selected_list = selection->get_full_selected_node_list();
		for (Node *selected_node : selected_list) {
			if (!selected_node) {
				continue;
			}

			Dictionary node_context;
			node_context["name"] = selected_node->get_name();
			node_context["type"] = selected_node->get_class();
			node_context["path"] = String(selected_node->get_path());
			if (selected_node->is_inside_tree() && scene_root->is_ancestor_of(selected_node)) {
				node_context["scene_path"] = String(scene_root->get_path_to(selected_node));
			}
			selected_nodes.push_back(node_context);
		}
	}

	scene["selected_nodes"] = selected_nodes;
	return scene;
}

Dictionary AIContextBuilder::_build_script_context() {
	Dictionary script;

	EditorInterface *editor_interface = EditorInterface::get_singleton();
	if (!editor_interface) {
		return script;
	}

	ScriptEditor *script_editor = editor_interface->get_script_editor();
	if (!script_editor) {
		return script;
	}

	ScriptEditorBase *current_editor = script_editor->get_current_editor();
	if (!current_editor) {
		return script;
	}

	Ref<Resource> edited_resource = current_editor->get_edited_resource();
	if (edited_resource.is_valid()) {
		script["path"] = edited_resource->get_path();
		script["type"] = edited_resource->get_class();
	}

	TextEditorBase *text_editor = Object::cast_to<TextEditorBase>(current_editor);
	if (text_editor && text_editor->get_code_editor()) {
		TextEdit *text_edit = text_editor->get_code_editor()->get_text_editor();
		if (text_edit) {
			const int caret_line = text_edit->get_caret_line();
			const int caret_column = text_edit->get_caret_column();
			script["line_count"] = text_edit->get_line_count();
			script["caret_line"] = caret_line + 1;
			script["caret_column"] = caret_column + 1;

			if (text_edit->has_selection()) {
				script["selected_text"] = text_edit->get_selected_text();
			} else if (caret_line >= 0 && caret_line < text_edit->get_line_count()) {
				script["current_line"] = text_edit->get_line(caret_line);
			}
		}
	}

	return script;
}

Dictionary AIContextBuilder::build_context(bool p_include_scene, bool p_include_script) {
	Dictionary context;

	if (p_include_scene) {
		context["scene"] = _build_scene_context();
	}

	if (p_include_script) {
		context["script"] = _build_script_context();
	}

	return context;
}

String AIContextBuilder::summarize_context(const Dictionary &p_context) {
	PackedStringArray summary;

	if (p_context.has("scene")) {
		const Dictionary scene = p_context["scene"];
		if (scene.has("name") && !String(scene["name"]).is_empty()) {
			String scene_line = vformat(TTR("Scene: %s"), String(scene["name"]));
			if (scene.has("path") && !String(scene["path"]).is_empty()) {
				scene_line += " (" + String(scene["path"]) + ")";
			}
			if (scene.has("selected_nodes")) {
				const Array selected_nodes = scene["selected_nodes"];
				scene_line += vformat(TTR(", selected nodes: %d"), selected_nodes.size());
			}
			summary.push_back(scene_line);
		}
	}

	if (p_context.has("script")) {
		const Dictionary script = p_context["script"];
		if (script.has("path") && !String(script["path"]).is_empty()) {
			String script_line = vformat(TTR("Script: %s"), String(script["path"]));
			if (script.has("caret_line")) {
				script_line += vformat(TTR(", line %d"), int(script["caret_line"]));
			}
			if (script.has("selected_text")) {
				script_line += TTR(", selection included");
			}
			summary.push_back(script_line);
		}
	}

	if (summary.is_empty()) {
		return TTR("Context: no active scene or script captured.");
	}

	return TTR("Context captured: ") + String(", ").join(summary);
}
