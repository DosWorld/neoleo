#include <exception>
#include <getopt.h>
#include <iostream>
#include <string>
#include <sys/auxv.h>
//#include <sys/param.h> // for PATH_MAX
#include <unistd.h>
#include <vector>

#include "assert.h"
#include "basic.h"
#include "cmd.h"
#include "config.h"
#include "defuns.h"
#include "init.h"
#include "io-abstract.h"
#include "io-headless.h"
#include "io-term.h"
#include "io-utils.h"
#include "logging.h"
#include "tests.h"
#include "utils.h"

#include "oleox.h"

using std::cerr;
using std::cout;
using std::endl;
using std::string;
using std::vector;


/* A bland signal handler. */
static RETSIGTYPE
got_sig (int sig)
{
}


static void
init_maps (void)
{
  num_maps = 0;
  the_maps = 0;
  map_names = 0;
  map_prompts = 0;

  the_funcs = (cmd_func**) ck_malloc (sizeof (struct cmd_func *) * 2);
  num_funcs = 1;
  the_funcs[0] = (cmd_func *) get_cmd_funcs();

  find_func (0, &end_macro_cmd, "end-macro");
  find_func (0, &digit_0_cmd, "digit-0");
  find_func (0, &digit_9_cmd, "digit-9");
  find_func (0, &break_cmd, "break");
  find_func (0, &universal_arg_cmd, "universal-argument");

  create_keymap ("universal", 0);
  push_command_frame (0, 0, 0);
}



void
init_maps_and_macros()
{
	try {
		init_maps();
		init_named_macro_strings ();
                run_init_cmds ();
	} catch (OleoJmp& e) {
		fprintf (stderr, "Error in the builtin init scripts (a bug!).\n");
                io_close_display(69);
                exit (69);
	}
}


static bool	option_tests = false;

std::string	option_tests_argument = "regular";
static char	option_separator = '\t';
static char	*option_format = NULL;

bool get_option_tests() { return option_tests;}

static char short_options[] = "4:VqfxHhsFSTvx";
static struct option long_options[] =
{
	{"version",		0,	NULL,	'V'},
	{"2019",		0,	NULL,	'x'},
	{"headless",		0,	NULL,	'H'},
	{"help",		0,	NULL,	'h'},
	{"tests",		optional_argument,	NULL,	'T'},
	{"version",		0,	NULL,	'v'},
	{NULL,			0,	NULL,	0}
};


void
print_version()
{
	printf("%s %s\n", PACKAGE_NAME, VERSION);
	printf("Copyright (c) 1992-2000 Free Software Foundation, Inc.\n");
	printf("%s comes with ABSOLUTELY NO WARRANTY.\n", PACKAGE_NAME);
	printf("You may redistribute copies of %s\n", PACKAGE);
	printf("under the terms of the GNU General Public License.\n");
	printf("For more information about these matters, ");
	printf("see the files named COPYING.\n");
	printf("\nCompiled: %s %s\n", __DATE__, __TIME__);
	printf("Auxval: %s\n", (char *)getauxval(AT_EXECFN));
	printf("Exe: %s\n", Global->argv[0]);
	printf("Datadir: %s/neoleo\n", DATADIR);
}



static void
show_usage (void)
{

	printf("This is %s %s\n\n", PACKAGE, VERSION);
	printf("Usage: %s [OPTION]... [FILE]...\n", PACKAGE);

const char* usage = R"(
  -H, --headless           run without all toolkits
  -h, --help               display this help and exit
  -V, --version            output version information and exit
  -T, --tests [x]          run test suite x
  -x, --2019               use experimental interface

Report bugs to https://github.com/blippy/neoleo/issues
)";

	printf("%s", usage);
}



void
parse_command_line(int argc, char **argv)
{
	int opt, optindex;

	while (1) {
		opt = getopt_long (argc, argv, short_options, long_options, &optindex);
		if (opt == EOF)
			break;

		switch (opt)
		{
			case 'v':
			case 'V':
				print_version();
				exit (0);
				break;
			case 'H':
				user_wants_headless = true;
				break;
			case 'h':
				show_usage ();
				exit (0);
				break;
			case 'T':
				option_tests = true;
				//cout << "optindex:" << optind << "\n";
				if(!optarg 
						&& optind < argc
						&& NULL !=argv[optind] 
						&& '\0' != argv[optind][0]
						&& '-' != argv[optind][0])
					option_tests_argument = argv[optind++];
				//exit(1);
				break;
			case 'x':
				use_2019 = true;
				break;
		}
	}


	if (argc - optind > 1)
	{
		show_usage ();
		exit (1);
	}
}


void run_nonexperimental_mode(int argc, char** argv, int command_line_file)
{
	init_basics();
	headless_graphics(); // fallback position

	if(get_option_tests()) {
		bool all_pass = headless_tests();
		int ret = all_pass ? EXIT_SUCCESS : EXIT_FAILURE;
		//ret = EXIT_FAILURE;
		exit(ret);
	}



	FD_ZERO (&read_fd_set);
	FD_ZERO (&read_pending_fd_set);
	FD_ZERO (&exception_fd_set);
	FD_ZERO (&exception_pending_fd_set);

	bool force_cmd_graphics = false;
	choose_display(force_cmd_graphics);
	io_open_display ();

	OleoSetEncoding(OLEO_DEFAULT_ENCODING);

	init_maps_and_macros();

	using namespace std::literals;
	execute_command_sv("set-default-format general.float"sv);

	if (argc - optind == 1) {
		if (FILE *fp = fopen (argv[optind], "r")) {
			try {
				read_file_and_run_hooks (fp, 0, argv[optind]);
			} catch (OleoJmp& e) {
				fprintf (stderr, ", error occured reading '%s'\n", argv[optind]);
				io_info_msg(", error occured reading '%s'\n", argv[optind]);
			} 
			fclose (fp);
		}

		command_line_file = 1;
		FileSetCurrentFileName(argv[optind]);
		optind++;
	}

	/* Force the command frame to be rebuilt now that the keymaps exist. */
	{
		struct command_frame * last_of_the_old = the_cmd_frame->next;
		while (the_cmd_frame != last_of_the_old)
			free_cmd_frame (the_cmd_frame);
		free_cmd_frame (last_of_the_old);
	}

	io_recenter_cur_win ();
	Global->display_opened = 1;
	Global->modified = 0;
	io_run_main_loop();
}



int 
main(int argc, char **argv)
{
	int command_line_file = 0;	/* was there one? */

	InitializeGlobals();
	Global->argc = argc;
	Global->argv = argv;
	parse_command_line(argc, argv);
	run_nonexperimental_mode(argc, argv, command_line_file);

	return (0); /* Never Reached! */
}
