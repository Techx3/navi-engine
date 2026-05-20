/**************************************************************************/
/*  ai_chat_service.h                                                     */
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

#include "core/error/error_list.h"
#include "core/string/ustring.h"
#include "core/variant/dictionary.h"
#include "scene/main/node.h"

class HTTPRequest;

class AIChatService : public Node {
	GDCLASS(AIChatService, Node);

	HTTPRequest *http_request = nullptr;
	bool request_in_flight = false;
	String active_provider;
	String active_operation;

	String _build_system_prompt(const Dictionary &p_context) const;
	String _build_openai_payload(const String &p_model, const String &p_prompt, const Dictionary &p_context) const;
	String _build_anthropic_payload(const String &p_model, const String &p_prompt, const Dictionary &p_context) const;
	String _build_gemini_payload(const String &p_model, const String &p_prompt, const Dictionary &p_context) const;
	String _build_ollama_payload(const String &p_model, const String &p_prompt, const Dictionary &p_context) const;
	String _format_context_for_prompt(const Dictionary &p_context) const;
	String _extract_text_from_openai_response(const Dictionary &p_response) const;
	String _extract_text_from_anthropic_response(const Dictionary &p_response) const;
	String _extract_text_from_gemini_response(const Dictionary &p_response) const;
	void _handle_ollama_models_response(int p_response_code, const String &p_response_text);
	void _request_completed(int p_result, int p_response_code, const PackedStringArray &p_headers, const PackedByteArray &p_body);
	void _finish_with_error(const String &p_message);

protected:
	static void _bind_methods();

public:
	Error send_chat(const String &p_provider, const String &p_model, const String &p_prompt, const Dictionary &p_context, String *r_error = nullptr);
	Error refresh_ollama_models(String *r_error = nullptr);
	bool is_request_in_flight() const { return request_in_flight; }

	AIChatService();
};
