/*
    Entry point, must be the first object of the link: AmigaOS starts a hunk at
    its first byte, and the strings of main.c end up at the beginning of its
    own .text. Keep this file free of any data.
*/

int main(void);

int _start(void)
{
    return main();
}
