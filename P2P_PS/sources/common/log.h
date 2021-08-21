#pragma once
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>
#include <memory>
#include <experimental/memory_resource>


#define BUFFER_SIZE		1024


enum loglevel_e
{
	DEBUG,
	SUCCESS,
	INFO,
	WARNING,
	ERROR,
	STATISTIC,
	SILENT
};

class log_t
{
public:

	static void debug(const char* format, ...);
	static void success(const char* format, ...);
	static void log(const char* format, ...);
	static void warning(const char* format, ...);
	static void error(const char* format, ...);
	static void statistic(const char* format, ...);

private:

	static std::unique_ptr<char> get_timestamp();
	static int write_message(const char* level, const char* format, va_list args);

};