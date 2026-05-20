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
#include "core/os/os.h"

String AIProjectCheckpointManager::_get_project_path() {
	ProjectSettings *project_settings = ProjectSettings::get_singleton();
	if (!project_settings) {
		return String();
	}
	return project_settings->get_resource_path();
}

Error AIProjectCheckpointManager::_run_git(const List<String> &p_arguments, String *r_output, int *r_exitcode) {
	const String project_path = _get_project_path();
	ERR_FAIL_COND_V_MSG(project_path.is_empty(), ERR_UNCONFIGURED, "NAVI AI checkpoints require an open project.");

	List<String> arguments;
	arguments.push_back("-C");
	arguments.push_back(project_path);

	for (const String &argument : p_arguments) {
		arguments.push_back(argument);
	}

	String output;
	int exitcode = 0;
	const Error err = OS::get_singleton()->execute("git", arguments, &output, &exitcode, true);
	if (r_output) {
		*r_output = output.strip_edges();
	}
	if (r_exitcode) {
		*r_exitcode = exitcode;
	}

	if (err != OK) {
		return err;
	}

	return exitcode == 0 ? OK : FAILED;
}

Error AIProjectCheckpointManager::_ensure_git_identity() {
	List<String> args;
	String output;
	int exitcode = 0;

	args.push_back("config");
	args.push_back("user.name");
	if (_run_git(args, &output, &exitcode) != OK || output.is_empty()) {
		args.clear();
		args.push_back("config");
		args.push_back("user.name");
		args.push_back("NAVI AI");
		ERR_FAIL_COND_V(_run_git(args) != OK, FAILED);
	}

	args.clear();
	args.push_back("config");
	args.push_back("user.email");
	output.clear();
	exitcode = 0;
	if (_run_git(args, &output, &exitcode) != OK || output.is_empty()) {
		args.clear();
		args.push_back("config");
		args.push_back("user.email");
		args.push_back("navi-ai@local.invalid");
		ERR_FAIL_COND_V(_run_git(args) != OK, FAILED);
	}

	return OK;
}

Error AIProjectCheckpointManager::_ensure_gitignore() {
	const String project_path = _get_project_path();
	ERR_FAIL_COND_V(project_path.is_empty(), ERR_UNCONFIGURED);

	const String gitignore_path = project_path.path_join(".gitignore");
	if (FileAccess::exists(gitignore_path)) {
		return OK;
	}

	Ref<FileAccess> gitignore = FileAccess::open(gitignore_path, FileAccess::WRITE);
	ERR_FAIL_COND_V(gitignore.is_null(), ERR_CANT_CREATE);

	gitignore->store_line("# NAVI Engine project checkpoints");
	gitignore->store_line(".godot/");
	gitignore->store_line(".import/");
	gitignore->store_line("*.tmp");
	gitignore->store_line(".mono/temp/");
	return OK;
}

bool AIProjectCheckpointManager::is_project_repository_available() {
	List<String> args;
	args.push_back("rev-parse");
	args.push_back("--is-inside-work-tree");

	String output;
	return _run_git(args, &output) == OK && output == "true";
}

Error AIProjectCheckpointManager::ensure_project_repository(String *r_message) {
	if (is_project_repository_available()) {
		if (r_message) {
			*r_message = "Project Git repository is ready.";
		}
		return OK;
	}

	List<String> args;
	args.push_back("init");

	String output;
	if (_run_git(args, &output) != OK) {
		if (r_message) {
			*r_message = output.is_empty() ? "Git is not available or the project repository could not be initialized." : output;
		}
		return FAILED;
	}

	if (_ensure_gitignore() != OK || _ensure_git_identity() != OK) {
		if (r_message) {
			*r_message = "Project Git repository was initialized, but NAVI could not finish checkpoint configuration.";
		}
		return FAILED;
	}

	if (r_message) {
		*r_message = "Project Git repository initialized for NAVI checkpoints.";
	}
	return OK;
}

Error AIProjectCheckpointManager::create_checkpoint(const String &p_reason, String *r_commit_hash, String *r_message) {
	String message;
	if (ensure_project_repository(&message) != OK) {
		if (r_message) {
			*r_message = message;
		}
		return FAILED;
	}

	if (_ensure_git_identity() != OK) {
		if (r_message) {
			*r_message = "NAVI could not configure the local Git identity for checkpoints.";
		}
		return FAILED;
	}

	List<String> args;
	args.push_back("status");
	args.push_back("--porcelain");

	String status_output;
	if (_run_git(args, &status_output) != OK) {
		if (r_message) {
			*r_message = "NAVI could not read the project Git status.";
		}
		return FAILED;
	}

	if (status_output.is_empty()) {
		args.clear();
		args.push_back("rev-parse");
		args.push_back("--short");
		args.push_back("HEAD");
		String head_hash;
		if (_run_git(args, &head_hash) == OK && r_commit_hash) {
			*r_commit_hash = head_hash;
		}
		if (r_message) {
			*r_message = "No project file changes to checkpoint.";
		}
		return OK;
	}

	args.clear();
	args.push_back("add");
	args.push_back("-A");
	ERR_FAIL_COND_V(_run_git(args) != OK, FAILED);

	args.clear();
	args.push_back("commit");
	args.push_back("-m");
	args.push_back("NAVI checkpoint: " + p_reason.strip_edges());

	String commit_output;
	if (_run_git(args, &commit_output) != OK) {
		if (r_message) {
			*r_message = commit_output.is_empty() ? "NAVI could not create the checkpoint commit." : commit_output;
		}
		return FAILED;
	}

	args.clear();
	args.push_back("rev-parse");
	args.push_back("--short");
	args.push_back("HEAD");

	String commit_hash;
	if (_run_git(args, &commit_hash) == OK && r_commit_hash) {
		*r_commit_hash = commit_hash;
	}

	if (r_message) {
		*r_message = "Checkpoint created.";
	}
	return OK;
}
