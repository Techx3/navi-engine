/**************************************************************************/
/*  ai_action_manager.cpp                                                 */
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

#include "ai_action_manager.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/templates/hash_set.h"
#include "core/variant/dictionary.h"
#include "editor/file_system/editor_file_system.h"

bool AIActionManager::_is_safe_resource_path(const String &p_path, String *r_error) {
	const String path = p_path.strip_edges().simplify_path();
	if (!path.begins_with("res://")) {
		if (r_error) {
			*r_error = "AI actions may only edit files inside res://.";
		}
		return false;
	}

	if (path.contains("..") || path == "res://" || path.get_file().is_empty()) {
		if (r_error) {
			*r_error = "AI action path is not safe: " + p_path;
		}
		return false;
	}

	if (path.begins_with("res://.godot/") || path == "res://project.godot") {
		if (r_error) {
			*r_error = "AI actions cannot edit project metadata directly.";
		}
		return false;
	}

	return true;
}

Error AIActionManager::_write_file(const String &p_path, const String &p_content, String *r_error) {
	if (!_is_safe_resource_path(p_path, r_error)) {
		return ERR_INVALID_PARAMETER;
	}

	const String path = p_path.strip_edges().simplify_path();
	ProjectSettings *project_settings = ProjectSettings::get_singleton();
	ERR_FAIL_NULL_V(project_settings, ERR_UNCONFIGURED);

	const String absolute_base_dir = project_settings->globalize_path(path.get_base_dir());
	const Error dir_err = DirAccess::make_dir_recursive_absolute(absolute_base_dir);
	if (dir_err != OK) {
		if (r_error) {
			*r_error = "NAVI could not create the target directory for " + path + ".";
		}
		return dir_err;
	}

	Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE);
	if (file.is_null()) {
		if (r_error) {
			*r_error = "NAVI could not write " + path + ".";
		}
		return ERR_CANT_OPEN;
	}

	file->store_string(p_content);
	if (file->get_error() != OK) {
		if (r_error) {
			*r_error = "NAVI failed while writing " + path + ".";
		}
		return file->get_error();
	}

	if (EditorFileSystem::get_singleton()) {
		EditorFileSystem::get_singleton()->update_file(path);
		EditorFileSystem::get_singleton()->scan_changes();
	}

	return OK;
}

Error AIActionManager::_replace_text(const String &p_path, const String &p_old_text, const String &p_new_text, String *r_error) {
	if (!_is_safe_resource_path(p_path, r_error)) {
		return ERR_INVALID_PARAMETER;
	}

	if (p_old_text.is_empty()) {
		if (r_error) {
			*r_error = "replace_text requires a non-empty old_text value.";
		}
		return ERR_INVALID_PARAMETER;
	}

	const String path = p_path.strip_edges().simplify_path();
	Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ);
	if (file.is_null()) {
		if (r_error) {
			*r_error = "NAVI could not read " + path + ".";
		}
		return ERR_CANT_OPEN;
	}

	const String current_text = file->get_as_text();
	if (!current_text.contains(p_old_text)) {
		if (r_error) {
			*r_error = "NAVI could not find the requested text in " + path + ".";
		}
		return ERR_DOES_NOT_EXIST;
	}

	return _write_file(path, current_text.replace_first(p_old_text, p_new_text), r_error);
}

Array AIActionManager::extract_actions_from_response(const String &p_response) {
	const PackedStringArray markers = { "```navi-actions", "```json" };

	for (int i = 0; i < markers.size(); i++) {
		const int marker_pos = p_response.find(markers[i]);
		if (marker_pos < 0) {
			continue;
		}

		const int body_start = p_response.find("\n", marker_pos);
		if (body_start < 0) {
			continue;
		}

		const int body_end = p_response.find("```", body_start + 1);
		if (body_end < 0) {
			continue;
		}

		const String json_text = p_response.substr(body_start + 1, body_end - body_start - 1).strip_edges();
		const Variant parsed = JSON::parse_string(json_text);
		if (parsed.get_type() != Variant::DICTIONARY) {
			continue;
		}

		const Dictionary parsed_dictionary = parsed;
		if (parsed_dictionary.has("actions") && Variant(parsed_dictionary["actions"]).get_type() == Variant::ARRAY) {
			return parsed_dictionary["actions"];
		}
	}

	const Variant parsed = JSON::parse_string(p_response.strip_edges());
	if (parsed.get_type() == Variant::DICTIONARY) {
		const Dictionary parsed_dictionary = parsed;
		if (parsed_dictionary.has("actions") && Variant(parsed_dictionary["actions"]).get_type() == Variant::ARRAY) {
			return parsed_dictionary["actions"];
		}
	}

	return Array();
}

Vector<String> AIActionManager::collect_action_paths(const Array &p_actions) {
	Vector<String> paths;
	HashSet<String> seen;

	for (int i = 0; i < p_actions.size(); i++) {
		if (Variant(p_actions[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}

		const Dictionary action = p_actions[i];
		if (!action.has("path")) {
			continue;
		}

		const String path = String(action["path"]).strip_edges().simplify_path();
		if (!path.is_empty() && !seen.has(path)) {
			seen.insert(path);
			paths.push_back(path);
		}
	}

	return paths;
}

String AIActionManager::describe_actions(const Array &p_actions) {
	if (p_actions.is_empty()) {
		return String();
	}

	PackedStringArray descriptions;
	for (int i = 0; i < p_actions.size(); i++) {
		if (Variant(p_actions[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}

		const Dictionary action = p_actions[i];
		const String type = action.has("type") ? String(action["type"]) : String();
		const String path = action.has("path") ? String(action["path"]) : String();
		if (!type.is_empty() && !path.is_empty()) {
			descriptions.push_back(vformat("%s: %s", type, path));
		}
	}

	if (descriptions.is_empty()) {
		return String();
	}

	return String("\n").join(descriptions);
}

Error AIActionManager::apply_actions(const Array &p_actions, String *r_message) {
	if (p_actions.is_empty()) {
		if (r_message) {
			*r_message = "There are no NAVI actions to apply.";
		}
		return ERR_INVALID_PARAMETER;
	}

	int applied_count = 0;
	for (int i = 0; i < p_actions.size(); i++) {
		if (Variant(p_actions[i]).get_type() != Variant::DICTIONARY) {
			if (r_message) {
				*r_message = "NAVI action is not an object.";
			}
			return ERR_INVALID_DATA;
		}

		const Dictionary action = p_actions[i];
		const String type = action.has("type") ? String(action["type"]) : String();
		const String path = action.has("path") ? String(action["path"]) : String();

		Error err = OK;
		if (type == "write_file") {
			if (!action.has("content")) {
				if (r_message) {
					*r_message = "write_file action is missing content.";
				}
				return ERR_INVALID_DATA;
			}
			err = _write_file(path, String(action["content"]), r_message);
		} else if (type == "replace_text") {
			if (!action.has("old_text") || !action.has("new_text")) {
				if (r_message) {
					*r_message = "replace_text action is missing old_text or new_text.";
				}
				return ERR_INVALID_DATA;
			}
			err = _replace_text(path, String(action["old_text"]), String(action["new_text"]), r_message);
		} else {
			if (r_message) {
				*r_message = "Unsupported NAVI action type: " + type + ".";
			}
			return ERR_UNAVAILABLE;
		}

		if (err != OK) {
			return err;
		}
		applied_count++;
	}

	if (r_message) {
		*r_message = vformat("Applied %d NAVI action(s).", applied_count);
	}
	return OK;
}
