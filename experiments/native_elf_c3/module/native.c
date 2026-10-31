/* Freestanding ET_DYN: static rodata, writable BSS/data, local variables and
 * an indirect call into a separately compiled translation unit. */
extern int helper_add(int a, int b);
static unsigned call_count;
static const char message[] = "elf";
static int (*volatile function)(int, int) = helper_add;

int app_main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    const volatile char *text = message;
    const int local_result = function(20, 22);
    const int string_check = text[0] - 'e' + text[1] - 'l' + text[2] - 'f';
    ++call_count;
    return local_result + call_count - 1u + string_check;
}
