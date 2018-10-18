#pragma once

namespace Chat
{
	// Interactive event types in priority order.
	enum chat_event_type
	{
		chat_event_type_error,
		chat_event_type_state_change,
		chat_event_type_http_response,
		chat_event_type_http_request,
		chat_event_type_rpc_reply,
		chat_event_type_rpc_method,
		chat_event_type_rpc_event,
	};

	struct chat_event_internal
	{
		const chat_event_type type;
		chat_event_internal(const chat_event_type type);
	};
}