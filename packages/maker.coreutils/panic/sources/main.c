/* Copyright © 2018-2019 MAKER.                                               */
/* This code is licensed under the MIT License.                               */
/* See: LICENSE.md                                                            */

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    // do a page fault
    *(char*)(0xffffffff) = 1;

    return 0;
}
