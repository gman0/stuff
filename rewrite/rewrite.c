/*
 * rewrite - destroys contents of a file(s)
 *
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <gman@codefreax.org> wrote this file. As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.
 * ----------------------------------------------------------------------------
 *
 * made out of pure boredom
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <dirent.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <time.h>

#define PACKAGE "rewrite"
#define VERSION "1.4.0"


unsigned long g_block_size = 5242880;
char *g_block = 0;
int g_flags = 0;
unsigned g_passes = 1;

typedef unsigned long offset_t;

void   print_version();
void   print_help(const char *prog_name);
int    print_error(int code, const char *format, ...);
bool   interactive(const char *file_name);
bool   interactive_answer(const char *answer, const char *lng, const char *shrt);
void   delete(const char *file_name);
void   fuck_file(const char *file_name);
size_t block_size(size_t size, offset_t offset);
void   fill_buffer_rand(char *buf, size_t len);
void   list_dir(const char *dir_name, int passes, void (*callback)(const char *));

typedef enum
{
	A_QUIET =			1 << 1,
	A_INTERACTIVE =		1 << 2,
	A_YES_INTERACTIVE =	1 << 3,
	A_DELETE =			1 << 4,
	A_RECURSIVE =		1 << 8,
	F_PATTERN_ONES =	1 << 5,
	F_PATTERN_RANDOM =	1 << 6,
	F_PATTERN_REGEN =	1 << 7
} flag_t;

inline bool is_on(short flags, flag_t mask)
{
	return (flags & mask) == mask;
}

int main(int argc, char *argv[])
{
	if (argc < 2)
		print_help(argv[0]);

	struct option opts[] = {
		{"help",		no_argument,		0,	'h'},
		{"version",		no_argument,		0,	'v'},
		{"quiet",		no_argument,		0,	'q'},
		{"interactive",	no_argument,		0,	'i'},
		{"yes",			no_argument,		0,	'y'},
		{"pattern-ones",no_argument,		0,	'o'},
		{"pattern-rand",no_argument,		0,	'r'},
		{"always-rand",	no_argument,		0,	'a'},
		{"delete",		no_argument,		0,	'd'},
		{"passes",		required_argument,	0,	'p'},
		{"block-size",	required_argument,	0,	'b'},
		{"recursive",	no_argument,		0,	  1},
		{0,				0,					0,	  0}
	};

	int c;

	while ((c = getopt_long(argc, argv, "hvqiyoradp:b:", opts, 0)) != -1)
	{
		switch (c)
		{
			case 'h':
				print_help(argv[0]);
				break;

			case 'v':
				print_version();
				break;

			case 'q':
				g_flags |= A_QUIET;
				break;

			case 'i':
				g_flags |= A_INTERACTIVE;
				break;

			case 'y':
				g_flags |= A_YES_INTERACTIVE;
				break;

			case 'o':
				g_flags |= F_PATTERN_ONES;
				break;

			case 'r':
				g_flags |= F_PATTERN_RANDOM;
				break;
			
			case 'a':
				g_flags |= F_PATTERN_RANDOM | F_PATTERN_REGEN;
				break;

			case 'd':
				g_flags |= A_DELETE;
				break;

			case 'p':
				g_passes = atoi(optarg);
				break;

			case 'b':
				g_block_size = atol(optarg);
				break;

			case 1:
				g_flags |= A_RECURSIVE;
				break;
		}
	}

	if (g_passes < 1)
		return 0;


	g_block = (char*)malloc(g_block_size);

	if (is_on(g_flags, F_PATTERN_RANDOM))
		fill_buffer_rand(g_block, g_block_size);
	else if (is_on(g_flags, F_PATTERN_ONES))
		memset(g_block, 1, g_block_size);
	else
		memset(g_block, 0, g_block_size);

	bool interact[argc - 1];
	memset(interact, 0, sizeof(bool) * (argc - 1));

	if (!is_on(g_flags, A_RECURSIVE)) // no interaction for recursion...at least for now
	{
		int i;
		for (i = optind; i < argc; i++)
		{
			/* first, we have to figure out if we want to
			 * destroy the particular file */
			if ((interact[i] = interactive(argv[i])))
				fuck_file(argv[i]);
		}

		int pass = 1; // we have already made 1 pass
		for (; pass < g_passes; pass++)
		{
			int j;
			for (j = optind; j < argc; j++)
				if (interact[j]) fuck_file(argv[j]);
		}

		// delete all the files if required
		if (is_on(g_flags, A_DELETE))
		{
			for (i = optind; i < argc; i++)
				if (interact[i]) delete(argv[i]);
		}
	}
	else
	{
		int i;
		for (i = optind; i < argc; i++)
			list_dir(argv[i], g_passes, &fuck_file);

		if (is_on(g_flags, A_DELETE))
		{
			for (i = optind; i < argc; i++)
				list_dir(argv[i], 0, &delete);
		}
	}


	free(g_block);

	return 0;
}

void print_version()
{
	printf("%s %s\n", PACKAGE, VERSION);
	exit(1);
}

void print_help(const char *prog_name)
{
	printf("%s fucks up contents of a file(s)\n"
		   "Usage: %s [-hvqiyoradp] [-b block size] file ...\n"
		   "\t-h  --help\t\t\tprints this help message\n"
		   "\t-v  --version\t\t\tprints version\n"
		   "\t-q  --quiet\t\t\tdon't report errors\n"
		   "\t-i  --interactive\t\ttoggles interactive mode\n"
		   "\t-y  --yes\t\t\tassumes Yes in interactive mode (implies --interactive)\n"
		   "\t-o  --pattern-ones\t\tuse 1's to fill the buffer (default pattern are 0's)\n"
		   "\t-r  --pattern-rand\t\tuse 0's or 1's randomly\n"
		   "\t-a  --always-rand\t\tuse 0's or 1's randomly and generate a new random pattern after each pass (can really slow down the proccess)\n"
		   "\t-d  --delete\t\t\tdelete the file(s) too\n"
		   "\t-p  --passes passes\t\tnumber of passes (defaults to 1)\n"
		   "\t-b  --block-size block size\tsets block size (defaults to 5242880 bytes (5MB))\n"
		   "\t    --recursive\t\t\tsearch directories recursively\n",
		   PACKAGE, prog_name);

	exit(1);
}

int print_error(int code, const char *format, ...)
{
	int ret = 0;

	if (!is_on(g_flags, A_QUIET))
	{
		va_list ap;
		va_start(ap, format);
		ret = vfprintf(stderr, format, ap);
		va_end(ap);

		if (code)
			exit(code);
	}

	return ret;
}

bool interactive(const char *file_name)
{
	bool yes_interactive = is_on(g_flags, A_YES_INTERACTIVE);

	if (!is_on(g_flags, A_INTERACTIVE) && !yes_interactive)
		return true;

	char answer[4];
	answer[3] = '\0';

	printf("Destroy file \"%s\" ", file_name);

	if (yes_interactive)
		printf("[Y/n]: ");
	else
		printf("[y/N]: ");

	fgets(answer, 4, stdin);

	if (yes_interactive)
		return !interactive_answer(answer, "no", "n\n");
	else
		return interactive_answer(answer, "yes", "y\n");
}

bool interactive_answer(const char *answer, const char *lng, const char *shrt)
{
	if (!strcmp(answer, lng) || !strcmp(answer, shrt))
		return true;

	return false;
}

void delete(const char *file_name)
{
	if (unlink(file_name) < 0) 
		print_error(0, "Cannot delete file \"%s\": %s\n", file_name, strerror(errno));
}

void fuck_file(const char *file_name)
{
	FILE *file = fopen(file_name, "r+");

	if (!file)
	{
		print_error(0, "Cannot write to \"%s\": %s\n", file_name, strerror(errno));
		return;
	}

	size_t size;

	fseek(file, 0, SEEK_END);
	size = ftell(file);
	rewind(file);

	offset_t offset = 0;

	while (offset < size)
	{
		size_t blck_size = block_size(size, offset);
		fseek(file, offset, SEEK_SET);
		fwrite(g_block, blck_size, 1, file);

		offset += blck_size;
	}

	fclose(file);

	if (is_on(g_flags, F_PATTERN_REGEN))
		fill_buffer_rand(g_block, g_block_size);
}

size_t block_size(size_t size, offset_t offset)
{
	if (offset + g_block_size > size)
		return (size - offset);

	return g_block_size;
}

void fill_buffer_rand(char *buf, size_t len)
{
	srand(time(0));

	size_t i;
	for (i = 0; i < len; i++)
	{
		char byte = rand() % 2;
		buf[i] = byte;
	}
}

void list_dir(const char *dir_name, int passes, void (*callback)(const char *))
{
	DIR * dir = opendir(dir_name);

	if (!dir)
	{
		print_error(0, "Cannot open directory \"%s\": %s\n", dir_name, strerror(errno));
		return;
	}

	while (1)
	{
		struct dirent *entry = readdir(dir);
		const char *d_name = entry->d_name;

		if (!entry)
			break; // No more entries in this directory.

		/* Skip the "." and ".." */
		if (strcmp(d_name, ".") && strcmp(d_name, ".."))
		{
			char path[PATH_MAX];
			int path_length = snprintf(path, PATH_MAX, "%s/%s", dir_name, d_name);

			if (path_length >= PATH_MAX)
			{
				print_error(0, "Path \"%s\" is too long (>=%d), skipping\n", path, PATH_MAX);
				break;
			}

			if (entry->d_type & DT_DIR)
				list_dir(path, passes, callback); // Recursively call list_dir with new path.
			else
			{
				int i;
				for (i = 0; i < passes; i++)
					(*callback)(path);
			}
		}
	}

	if (closedir(dir))
		print_error(0, "Cannot close \"%s\": %s\n", dir_name, strerror(errno));
}
