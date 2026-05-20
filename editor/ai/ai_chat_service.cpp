/**************************************************************************/
/*  ai_chat_service.cpp                                                   */
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

#include "ai_chat_service.h"

#include "core/io/http_client.h"
#include "core/io/json.h"
#include "core/object/callable_mp.h"
#include "editor/settings/editor_settings.h"
#include "scene/main/http_request.h"

void AIChatService::_bind_methods() {
	ADD_SIGNAL(MethodInfo("response_received", PropertyInfo(Variant::STRING, "response")));
	ADD_SIGNAL(MethodInfo("request_failed", PropertyInfo(Variant::STRING, "message")));
	ADD_SIGNAL(MethodInfo("ollama_models_received", PropertyInfo(Variant::PACKED_STRING_ARRAY, "models")));
}

String AIChatService::_format_context_for_prompt(const Dictionary &p_context) const {
	if (p_context.is_empty()) {
		return String();
	}

	return JSON::stringify(p_context, "\t", false);
}

String AIChatService::_build_system_prompt(const Dictionary &p_context) const {
	String system_prompt = "You are NAVI, the integrated AI assistant inside NAVI Engine, a Godot-based editor. Answer concisely, prefer safe project-scoped changes, and use the provided editor context when relevant.";
	system_prompt += "\n\nWhen the user asks for code or file changes, explain the change briefly and include a fenced ```navi-actions JSON block. Supported actions are write_file with path/content and replace_text with path/old_text/new_text. Paths must stay inside res://. Example: ```navi-actions\n{\"actions\":[{\"type\":\"replace_text\",\"path\":\"res://player.gd\",\"old_text\":\"old\",\"new_text\":\"new\"}]}\n```";

	const String context_text = _format_context_for_prompt(p_context);
	if (!context_text.is_empty()) {
		system_prompt += "\n\nCurrent editor context:\n" + context_text;
	}

	return system_prompt;
}

String AIChatService::_build_openai_payload(const String &p_model, const String &p_prompt, const Dictionary &p_context) const {
	Dictionary payload;
	payload["model"] = p_model;
	payload["input"] = _build_system_prompt(p_context) + "\n\nUser request:\n" + p_prompt;
	payload["max_output_tokens"] = EDITOR_GET("ai/providers/openai/max_output_tokens");
	return JSON::stringify(payload);
}

String AIChatService::_build_anthropic_payload(const String &p_model, const String &p_prompt, const Dictionary &p_context) const {
	Dictionary payload;
	payload["model"] = p_model;
	payload["max_tokens"] = EDITOR_GET("ai/providers/anthropic/max_tokens");
	payload["system"] = _build_system_prompt(p_context);

	Array messages;
	Dictionary user_message;
	user_message["role"] = "user";
	user_message["content"] = p_prompt;
	messages.push_back(user_message);
	payload["messages"] = messages;

	return JSON::stringify(payload);
}

String AIChatService::_build_gemini_payload(const String &p_model, const String &p_prompt, const Dictionary &p_context) const {
	Dictionary payload;

	Dictionary system_instruction;
	Array system_parts;
	Dictionary system_text;
	system_text["text"] = _build_system_prompt(p_context);
	system_parts.push_back(system_text);
	system_instruction["parts"] = system_parts;
	payload["systemInstruction"] = system_instruction;

	Array contents;
	Dictionary user_content;
	user_content["role"] = "user";
	Array user_parts;
	Dictionary user_text;
	user_text["text"] = p_prompt;
	user_parts.push_back(user_text);
	user_content["parts"] = user_parts;
	contents.push_back(user_content);
	payload["contents"] = contents;

	Dictionary generation_config;
	generation_config["maxOutputTokens"] = EDITOR_GET("ai/providers/gemini/max_output_tokens");
	payload["generationConfig"] = generation_config;

	return JSON::stringify(payload);
}

String AIChatService::_build_ollama_payload(const String &p_model, const String &p_prompt, const Dictionary &p_context) const {
	Dictionary payload;
	payload["model"] = p_model;
	payload["stream"] = false;

	Dictionary options;
	options["temperature"] = EDITOR_GET("ai/providers/ollama/temperature");
	options["top_p"] = EDITOR_GET("ai/providers/ollama/top_p");
	options["num_ctx"] = EDITOR_GET("ai/providers/ollama/num_ctx");
	payload["options"] = options;
	payload["keep_alive"] = EDITOR_GET("ai/providers/ollama/keep_alive");

	Array messages;

	Dictionary system_message;
	system_message["role"] = "system";
	system_message["content"] = _build_system_prompt(p_context);
	messages.push_back(system_message);

	Dictionary user_message;
	user_message["role"] = "user";
	user_message["content"] = p_prompt;
	messages.push_back(user_message);

	payload["messages"] = messages;
	return JSON::stringify(payload);
}

String AIChatService::_extract_text_from_openai_response(const Dictionary &p_response) const {
	if (p_response.has("output_text")) {
		const String output_text = p_response["output_text"];
		if (!output_text.is_empty()) {
			return output_text;
		}
	}

	if (!p_response.has("output") || Variant(p_response["output"]).get_type() != Variant::ARRAY) {
		return String();
	}

	String text;
	const Array output = p_response["output"];
	for (int i = 0; i < output.size(); i++) {
		if (Variant(output[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}

		const Dictionary output_item = output[i];
		if (!output_item.has("content") || Variant(output_item["content"]).get_type() != Variant::ARRAY) {
			continue;
		}

		const Array content = output_item["content"];
		for (int j = 0; j < content.size(); j++) {
			if (Variant(content[j]).get_type() != Variant::DICTIONARY) {
				continue;
			}

			const Dictionary content_item = content[j];
			if (content_item.has("text")) {
				text += String(content_item["text"]);
			}
		}
	}

	return text;
}

String AIChatService::_extract_text_from_anthropic_response(const Dictionary &p_response) const {
	if (!p_response.has("content") || Variant(p_response["content"]).get_type() != Variant::ARRAY) {
		return String();
	}

	String text;
	const Array content = p_response["content"];
	for (int i = 0; i < content.size(); i++) {
		if (Variant(content[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}

		const Dictionary content_item = content[i];
		if (content_item.has("text")) {
			text += String(content_item["text"]);
		}
	}

	return text;
}

String AIChatService::_extract_text_from_gemini_response(const Dictionary &p_response) const {
	if (!p_response.has("candidates") || Variant(p_response["candidates"]).get_type() != Variant::ARRAY) {
		return String();
	}

	const Array candidates = p_response["candidates"];
	if (candidates.is_empty() || Variant(candidates[0]).get_type() != Variant::DICTIONARY) {
		return String();
	}

	const Dictionary first_candidate = candidates[0];
	if (!first_candidate.has("content") || Variant(first_candidate["content"]).get_type() != Variant::DICTIONARY) {
		return String();
	}

	const Dictionary content = first_candidate["content"];
	if (!content.has("parts") || Variant(content["parts"]).get_type() != Variant::ARRAY) {
		return String();
	}

	String text;
	const Array parts = content["parts"];
	for (int i = 0; i < parts.size(); i++) {
		if (Variant(parts[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}

		const Dictionary part = parts[i];
		if (part.has("text")) {
			text += String(part["text"]);
		}
	}

	return text;
}

void AIChatService::_finish_with_error(const String &p_message) {
	request_in_flight = false;
	active_operation.clear();
	emit_signal(SNAME("request_failed"), p_message);
}

void AIChatService::_handle_ollama_models_response(int p_response_code, const String &p_response_text) {
	request_in_flight = false;
	active_operation.clear();

	if (p_response_code < 200 || p_response_code >= 300) {
		emit_signal(SNAME("request_failed"), vformat("Ollama returned HTTP %d while listing models: %s", p_response_code, p_response_text));
		return;
	}

	const Variant parsed_response = JSON::parse_string(p_response_text);
	if (parsed_response.get_type() != Variant::DICTIONARY) {
		emit_signal(SNAME("request_failed"), "Ollama model list response is not valid JSON.");
		return;
	}

	const Dictionary response = parsed_response;
	if (!response.has("models") || Variant(response["models"]).get_type() != Variant::ARRAY) {
		emit_signal(SNAME("request_failed"), "Ollama response did not include a model list.");
		return;
	}

	PackedStringArray model_names;
	const Array models = response["models"];
	for (int i = 0; i < models.size(); i++) {
		if (Variant(models[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}

		const Dictionary model = models[i];
		if (model.has("model")) {
			model_names.push_back(String(model["model"]));
		} else if (model.has("name")) {
			model_names.push_back(String(model["name"]));
		}
	}

	emit_signal(SNAME("ollama_models_received"), model_names);
}

void AIChatService::_request_completed(int p_result, int p_response_code, const PackedStringArray &p_headers, const PackedByteArray &p_body) {
	request_in_flight = false;

	if (p_result != HTTPRequest::RESULT_SUCCESS) {
		active_operation.clear();
		emit_signal(SNAME("request_failed"), vformat("AI request failed with HTTPRequest result %d.", p_result));
		return;
	}

	const String response_text = String::utf8((const char *)p_body.ptr(), p_body.size());
	if (active_operation == "ollama_tags") {
		_handle_ollama_models_response(p_response_code, response_text);
		return;
	}

	active_operation.clear();
	if (p_response_code < 200 || p_response_code >= 300) {
		emit_signal(SNAME("request_failed"), vformat("AI provider returned HTTP %d: %s", p_response_code, response_text));
		return;
	}

	const Variant parsed_response = JSON::parse_string(response_text);
	if (parsed_response.get_type() != Variant::DICTIONARY) {
		emit_signal(SNAME("request_failed"), "AI provider returned a response that is not valid JSON.");
		return;
	}

	const Dictionary response = parsed_response;
	String content;

	if (active_provider == "OpenAI") {
		content = _extract_text_from_openai_response(response);
	} else if (active_provider == "Anthropic") {
		content = _extract_text_from_anthropic_response(response);
	} else if (active_provider == "Gemini") {
		content = _extract_text_from_gemini_response(response);
	}

	if (!content.is_empty()) {
		emit_signal(SNAME("response_received"), content);
		return;
	}

	if (active_provider == "Ollama") {
		if (!response.has("message")) {
			emit_signal(SNAME("request_failed"), "Ollama response did not include a message.");
			return;
		}

		const Variant message_variant = response["message"];
		if (message_variant.get_type() != Variant::DICTIONARY) {
			emit_signal(SNAME("request_failed"), "Ollama response message is not an object.");
			return;
		}

		const Dictionary message = message_variant;
		content = message.has("content") ? String(message["content"]) : String();
		if (content.is_empty()) {
			emit_signal(SNAME("request_failed"), "Ollama returned an empty response.");
			return;
		}

		emit_signal(SNAME("response_received"), content);
		return;
	}

	emit_signal(SNAME("request_failed"), "AI response parser is not implemented for this provider yet.");
}

Error AIChatService::send_chat(const String &p_provider, const String &p_model, const String &p_prompt, const Dictionary &p_context, String *r_error) {
	if (request_in_flight) {
		if (r_error) {
			*r_error = "NAVI is already waiting for an AI response.";
		}
		return ERR_BUSY;
	}

	const String model = p_model.strip_edges();
	if (model.is_empty()) {
		if (r_error) {
			*r_error = "Choose a model first.";
		}
		return ERR_INVALID_PARAMETER;
	}

	String base_url;
	String endpoint;
	String payload;
	Vector<String> headers;
	headers.push_back("Content-Type: application/json");

	if (p_provider == "OpenAI") {
		const String api_key = String(EDITOR_GET("ai/providers/openai/api_key")).strip_edges();
		if (api_key.is_empty()) {
			if (r_error) {
				*r_error = "OpenAI API key is empty.";
			}
			return ERR_INVALID_PARAMETER;
		}
		base_url = String(EDITOR_GET("ai/providers/openai/base_url")).strip_edges().trim_suffix("/");
		endpoint = "/responses";
		headers.push_back("Authorization: Bearer " + api_key);
		payload = _build_openai_payload(model, p_prompt, p_context);
	} else if (p_provider == "Anthropic") {
		const String api_key = String(EDITOR_GET("ai/providers/anthropic/api_key")).strip_edges();
		if (api_key.is_empty()) {
			if (r_error) {
				*r_error = "Anthropic API key is empty.";
			}
			return ERR_INVALID_PARAMETER;
		}
		base_url = String(EDITOR_GET("ai/providers/anthropic/base_url")).strip_edges().trim_suffix("/");
		endpoint = "/v1/messages";
		headers.push_back("x-api-key: " + api_key);
		headers.push_back("anthropic-version: 2023-06-01");
		payload = _build_anthropic_payload(model, p_prompt, p_context);
	} else if (p_provider == "Gemini") {
		const String api_key = String(EDITOR_GET("ai/providers/gemini/api_key")).strip_edges();
		if (api_key.is_empty()) {
			if (r_error) {
				*r_error = "Gemini API key is empty.";
			}
			return ERR_INVALID_PARAMETER;
		}
		base_url = String(EDITOR_GET("ai/providers/gemini/base_url")).strip_edges().trim_suffix("/");
		const String gemini_model = model.begins_with("models/") ? model : "models/" + model;
		endpoint = "/v1beta/" + gemini_model + ":generateContent";
		headers.push_back("x-goog-api-key: " + api_key);
		payload = _build_gemini_payload(model, p_prompt, p_context);
	} else if (p_provider == "Ollama") {
		base_url = String(EDITOR_GET("ai/providers/ollama/base_url")).strip_edges().trim_suffix("/");
		endpoint = "/api/chat";
		payload = _build_ollama_payload(model, p_prompt, p_context);
	} else {
		if (r_error) {
			*r_error = "Unknown AI provider.";
		}
		return ERR_INVALID_PARAMETER;
	}

	if (base_url.is_empty()) {
		if (r_error) {
			*r_error = "AI provider base URL is empty.";
		}
		return ERR_INVALID_PARAMETER;
	}

	active_provider = p_provider;
	active_operation = "chat";
	const Error err = http_request->request(base_url + endpoint, headers, HTTPClient::METHOD_POST, payload);
	if (err != OK) {
		if (r_error) {
			*r_error = "NAVI could not start the AI HTTP request.";
		}
		return err;
	}

	request_in_flight = true;
	return OK;
}

Error AIChatService::refresh_ollama_models(String *r_error) {
	if (request_in_flight) {
		if (r_error) {
			*r_error = "NAVI is already waiting for an AI response.";
		}
		return ERR_BUSY;
	}

	const String base_url = String(EDITOR_GET("ai/providers/ollama/base_url")).strip_edges().trim_suffix("/");
	if (base_url.is_empty()) {
		if (r_error) {
			*r_error = "Ollama base URL is empty.";
		}
		return ERR_INVALID_PARAMETER;
	}

	active_provider = "Ollama";
	active_operation = "ollama_tags";
	const Error err = http_request->request(base_url + "/api/tags");
	if (err != OK) {
		active_operation.clear();
		if (r_error) {
			*r_error = "NAVI could not request the local Ollama model list.";
		}
		return err;
	}

	request_in_flight = true;
	return OK;
}

AIChatService::AIChatService() {
	http_request = memnew(HTTPRequest);
	http_request->set_use_threads(true);
	http_request->set_timeout(120);
	http_request->connect("request_completed", callable_mp(this, &AIChatService::_request_completed));
	add_child(http_request);
}
