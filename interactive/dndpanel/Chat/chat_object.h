namespace Chat
{
	struct chat_object
	{
		const char* id;
		size_t idLength;
	};

	struct chat_control : public chat_object
	{
		const char* kind;
		size_t kindLength;
	};
}