#include "log.h"

extern loglevel_e loglevel;

const char* log_format = "|%s-%s| %s\n";


void log_t::debug(const char* format, ...)
{
	if (loglevel > DEBUG) return;

	va_list ap;
	va_start(ap, format);
	write_message("DEBUG", format, ap);
	va_end(ap);

}

void log_t::log(const char* format, ...)
{
	if (loglevel > INFO) return;

	va_list ap;
	va_start(ap, format);
	write_message("INFO ", format, ap);
	va_end(ap);
}

void log_t::warning(const char* format, ...)
{
	if (loglevel > WARNING) return;

	va_list ap;
	va_start(ap, format);
	write_message("WARN ", format, ap);
	va_end(ap);
}

void log_t::error(const char* format, ...)
{
	if (loglevel > ERROR) return;

	va_list ap;
	va_start(ap, format);
	write_message("ERROR", format, ap);
	va_end(ap);
}

std::unique_ptr<char> log_t::get_timestamp() {

	char* result;
	std::unique_ptr<char> buffer;
	buffer.reset((char*)malloc(BUFFER_SIZE));

	time_t t = time(NULL);
	tm* local_time = localtime(&t);

	if (local_time == NULL) {
		printf("Failed getting local time\n");
		return 0;
	}

	uint32_t written = strftime(buffer.get(), BUFFER_SIZE, "%x|%X", local_time);
	if (written == 0) {
		printf("Failed getting local time\n");
		return 0;
	}

	return buffer;
}

int log_t::write_message(const char* level, const char* format, va_list args) {
	std::unique_ptr<char> time = get_timestamp();
	char msg[BUFFER_SIZE];

	if (!time) {
		return 0;
	}

	uint32_t len = snprintf(msg, BUFFER_SIZE, log_format, time.get(), level, format);
	if (len < BUFFER_SIZE)
	{
		if (len < 0)
		{
			printf("Failed formatting log message");
		}
		return vprintf(msg, args);
	}

	std::unique_ptr<char> full_msg = get_timestamp();
	full_msg.reset((char*)malloc(len + 1));



	len = snprintf(full_msg.get(), len, log_format, time.get(), level, format);
	if (len < 0)
	{
		printf("Failed formatting log message");
	}

	return  vprintf(full_msg.get(), args);
}

