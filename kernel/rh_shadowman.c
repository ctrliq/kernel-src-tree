#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/printk.h>

/* Display a shadowman logo on the console screen */
static int __init rh_shadowman(char *str)
{
	pr_info("                   .^~!7???JJJJJJ???7!~^.                   \n");
	pr_info("              .:~7?JJJJJJJJJJJJJJJJJJJJJJ?7~:.              \n");
	pr_info("           .^!?JJJJJJJJJJJJJJJJJJJJJJJJJJJJJJ?!^.           \n");
	pr_info("         :!?JJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJ?!:         \n");
	pr_info("       :7JJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJ7:       \n");
	pr_info("     .!JJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJ!.     \n");
	pr_info("    ^JJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJ^    \n");
	pr_info("   ~JJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJ~   \n");
	pr_info("  !JJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJ!  \n");
	pr_info(" ^JJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJ?7JJJJJJJJJJJJJJJJJJ^ \n");
	pr_info(".?JJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJ7^  :7JJJJJJJJJJJJJJJJ?.\n");
	pr_info("~JJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJ7:      :7JJJJJJJJJJJJJJJ~\n");
	pr_info("?JJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJ!:          :!JJJJJJJJJJJJJ?\n");
	pr_info("JJJJJJJJJJJJJJJJJJJJJJJJJJJJJJ!.              .!?JJJJJJJJJJJ\n");
	pr_info("JJJJJJJJJJJJJJJJJJJJJJJJJJJ?~.                  .~?JJJJJJJJJ\n");
	pr_info("?JJJJJJJJJJJJJJJJJJJJJJJJ?~.                      .^?JJJJJJ?\n");
	pr_info("~JJJJJJJJJJJJJJJJJJJJJJ?^             .:             ^7JJJJ~\n");
	pr_info(".?JJJJJJJJJJJJJJJJJJJ7^             :!JJ!:             :7J?.\n");
	pr_info(" ^JJJJJJJJJJJJJJJJJ7:             :7JJJJJJ7:             :^ \n");
	pr_info("  !JJJJJJJJJJJJJJ!:             ^7JJJJJJJJJJ7^              \n");
	pr_info("   ~JJJJJJJJJJJ!.             ^?JJJJJJJJJJJJJJ?^            \n");
	pr_info("    ^JJJJJJJ?~.            .~?JJJJJJJJJJJJJJJJJJ?~.         \n");
	pr_info("     .!JJJ?~.            .~?JJJJJJJJJJJJJJJJJJJJJJ?!.       \n");
	pr_info("       :!^             .!JJJJJJJJJJJJJJJJJJJJJJJJJJ7:       \n");
	pr_info("                     :!JJJJJJJJJJJJJJJJJJJJJJJJJ?!:         \n");
	pr_info("                   :7JJJJJJJJJJJJJJJJJJJJJJJJ?!^.           \n");
	pr_info("                 .!JJJJJJJJJJJJJJJJJJJJJJ?7~:.              \n");
	pr_info("                  ..^~!7???JJJJJJ???7!~^.                   \n");
	pr_info(" ");
	return 1;
}

__setup("shadowman", rh_shadowman);
