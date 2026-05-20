/**************************************************************************/
/*  ai_project_checkpoint_manager.cpp                                     */
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

#include "ai_project_checkpoint_manager.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/os/time.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"

String AIProjectCheckpointManager::_get_project_path() {
	ProjectSettings *project_settings = ProjectSettings::get_singleton();
	if (!project_settings) {
		return String();
	}
	return project_settings->get_resource_path();
}

Error AIProjectCheckpointManager::_ensure_checkpoint_store() {
	const String project_path = _get_project_path();
	ERR_FAIL_COND_V_MSG(project_path.is_empty(), ERR_UNCONFIGURED, "NAVI checkpoints require an open project.");

	const String navi_dir = project_path.path_join(".navi");
	const Error navi_dir_err = DirAccess::make_dir_recursive_absolute(navi_dir);
	ERR_FAIL_COND_V(navi_dir_err != OK, navi_dir_err);

	const String gdignore_path = navi_dir.path_join(".gdignore");
	if (!FileAccess::exists(gdignore_path)) {
		Ref<FileAccess> gdignore = FileAccess::open(gdignore_path, FileAccess::WRITE);
		ERR_FAIL_COND_V(gdignore.is_null(), ERR_CANT_CREATE);
		gdignore->store_line("");
	}

	const String checkpoint_dir = navi_dir.path_join("checkpoints");
	const Error checkpoint_dir_err = DirAccess::make_dir_recursive_absolute(checkpoint_dir);
	ERR_FAIL_COND_V(checkpoint_dir_err != OK, checkpoint_dir_err);

	return OK;
}

String AIProjectCheckpointManager::_make_snapshot_id() {
	String timestamp = Time::get_singleton()->get_datetime_string_from_system(false, false).remove_chars("-T:");
	timestamp += "_" + itos(Time::get_singleton()->get_ticks_usec());
	return timestamp;
}

String AIProjectCheckpointManager::_get_snapshot_file_path(const String &p_snapshot_id, const String &p_resource_path) {
	const String project_path = _get_project_path();
	const String relative_path = p_resource_path.simplify_path().trim_prefix("res://");
	return project_path.path_join(".navi/checkpoints").path_join(p_snapshot_id).path_join("files").path_join(relative_path);
}

Error AIProjectCheckpointManager::create_checkpoint(const String &p_reason, const Vector<String> &p_resource_paths, String *r_snapshot_id, String *r_message) {
	const Error store_err = _ensure_checkpoint_store();
	if (store_err != OK) {
		if (r_message) {
			*r_message = "NAVI could not create the project checkpoint store.";
		}
		return store_err;
	}

	ProjectSettings *project_settings = ProjectSettings::get_singleton();
	ERR_FAIL_NULL_V(project_settings, ERR_UNCONFIGURED);

	const String project_path = _get_project_path();
	const String snapshot_id = _make_snapshot_id();
	const String snapshot_dir = project_path.path_join(".navi/checkpoints").path_join(snapshot_id);
	const Error snapshot_dir_err = DirAccess::make_dir_recursive_absolute(snapshot_dir.path_join("files"));
	if (snapshot_dir_err != OK) {
		if (r_message) {
			*r_message = "NAVI could not create the checkpoint folder.";
		}
		return snapshot_dir_err;
	}

	Dictionary metadata;
	metadata["id"] = snapshot_id;
	metadata["reason"] = p_reason;
	metadata["created_at"] = Time::get_singleton()->get_datetime_string_from_system(false, false);

	Array files;
	for (const String &resource_path : p_resource_paths) {
		const String normalized_path = resource_path.strip_edges().simplify_path();
		if (!normalized_path.begins_with("res://") || normalized_path.contains("..")) {
			if (r_message) {
				*r_message = "NAVI checkpoint skipped unsafe project path: " + resource_path;
			}
			return ERR_INVALID_PARAMETER;
		}

		Dictionary file_metadata;
		file_metadata["path"] = normalized_path;
		file_metadata["existed"] = FileAccess::exists(normalized_path);

		if (FileAccess::exists(normalized_path)) {
			const String snapshot_file_path = _get_snapshot_file_path(snapshot_id, normalized_path);
			const Error snapshot_base_err = DirAccess::make_dir_recursive_absolute(snapshot_file_path.get_base_dir());
			if (snapshot_base_err != OK) {
				if (r_message) {
					*r_message = "NAVI could not create a checkpoint folder for " + normalized_path + ".";
				}
				return snapshot_base_err;
			}

			const Error copy_err = DirAccess::copy_absolute(project_settings->globalize_path(normalized_path), snapshot_file_path);
			if (copy_err != OK) {
				if (r_message) {
					*r_message = "NAVI could not snapshot " + normalized_path + ".";
				}
				return copy_err;
			}

			file_metadata["snapshot_file"] = snapshot_file_path.replace(project_path.path_join(".navi/checkpoints").path_join(snapshot_id).path_join("files").path_join(""), "");
		}

		files.push_back(file_metadata);
	}

	metadata["files"] = files;

	Ref<FileAccess> metadata_file = FileAccess::open(snapshot_dir.path_join("metadata.json"), FileAccess::WRITE);
	if (metadata_file.is_null()) {
		if (r_message) {
			*r_message = "NAVI could not write checkpoint metadata.";
		}
		return ERR_CANT_CREATE;
	}
	metadata_file->store_string(JSON::stringify(metadata, "\t", false));

	if (r_snapshot_id) {
		*r_snapshot_id = snapshot_id;
	}
	if (r_message) {
		*r_message = vformat("Checkpoint %s saved for %d project element(s).", snapshot_id, p_resource_paths.size());
	}
	return OK;
}
